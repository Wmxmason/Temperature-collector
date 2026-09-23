#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <ti/drivers/rf/RF.h>

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/rf_data_entry.h)
#include DeviceFamily_constructPath(driverlib/rf_mailbox.h)
#include DeviceFamily_constructPath(driverlib/rf_prop_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_prop_mailbox.h)

#include "rf_receiver.h"
#include "smartrf_settings.h"

#define RF_RX_ENTRY_COUNT                    (5U)
#define RF_RX_PAYLOAD_LENGTH                 (9U)
#define RF_RX_LENGTH_BYTES                   (1U)
#define RF_RX_STATUS_BYTES                   (1U)
#define RF_RX_ENTRY_HEADER_SIZE              (8U)
#define RF_RX_ENTRY_DATA_SIZE                \
    (RF_RX_LENGTH_BYTES + RF_RX_PAYLOAD_LENGTH + RF_RX_STATUS_BYTES)
#define RF_RX_ENTRY_BUFFER_SIZE              \
    ((RF_RX_ENTRY_HEADER_SIZE + RF_RX_ENTRY_DATA_SIZE + 3U) & ~3U)

/* TI RF Entry头必须与数据起始偏移一致，不能将尾部对齐填充算入容量。 */
typedef char rf_rx_entry_header_size_check[
    (offsetof(rfc_dataEntryGeneral_t, data) == RF_RX_ENTRY_HEADER_SIZE) ? 1 : -1];

static RF_Object rf_object;
static RF_Handle rf_handle;
static dataQueue_t rf_rx_queue;
static rfc_propRxOutput_t rf_rx_output;
static uint8_t rf_rx_read_index;

#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_ALIGN(rf_rx_entry_pool, 4)
#endif
/* 基址4字节对齐，20字节步长保证池中每一个Entry都4字节对齐。 */
static uint8_t rf_rx_entry_pool[RF_RX_ENTRY_COUNT][RF_RX_ENTRY_BUFFER_SIZE];

static void rf_receiver_queue_init(void)
{
    rfc_dataEntryGeneral_t *entry;
    uint8_t i;

    memset(rf_rx_entry_pool, 0, sizeof(rf_rx_entry_pool));
    memset(&rf_rx_output, 0, sizeof(rf_rx_output));

    for (i = 0U; i < RF_RX_ENTRY_COUNT; i++)
    {
        entry = (rfc_dataEntryGeneral_t *)rf_rx_entry_pool[i];
        entry->pNextEntry = rf_rx_entry_pool[(i + 1U) % RF_RX_ENTRY_COUNT];
        entry->status = DATA_ENTRY_PENDING;
        entry->config.type = DATA_ENTRY_TYPE_GEN;
        entry->config.lenSz = 0U;
        entry->config.irqIntv = 0U;
        /* 数据区包含RF长度字节、温度负载和接收状态字节。 */
        entry->length = RF_RX_ENTRY_DATA_SIZE;
    }

    rf_rx_read_index = 0U;
    rf_rx_queue.pCurrEntry = rf_rx_entry_pool[0];
    rf_rx_queue.pLastEntry = NULL;
}

bool rf_receiver_init(void)
{
    RF_EventMask events;
    RF_Params params;

    if (rf_handle != NULL)
    {
        return true;
    }

    RF_Params_init(&params);
    rf_handle = RF_open(&rf_object,
                        &RF_prop,
                        (RF_RadioSetup *)&RF_cmdPropRadioDivSetup,
                        &params);
    if (rf_handle == NULL)
    {
        return false;
    }

    RF_cmdFs.status = 0U;
    events = RF_runCmd(rf_handle,
                       (RF_Op *)&RF_cmdFs,
                       RF_PriorityNormal,
                       NULL,
                       0U);
    if (((events & RF_EventLastCmdDone) == 0U) ||
        (RF_cmdFs.status != DONE_OK))
    {
        RF_close(rf_handle);
        rf_handle = NULL;
        return false;
    }

    rf_receiver_queue_init();
    RF_cmdPropRx.pQueue = &rf_rx_queue;
    RF_cmdPropRx.pOutput = (uint8_t *)&rf_rx_output;
    RF_cmdPropRx.maxPktLen = RF_RX_PAYLOAD_LENGTH;
    RF_cmdPropRx.pktConf.bRepeatOk = 0U;
    RF_cmdPropRx.pktConf.bRepeatNok = 1U;
    return true;
}

bool rf_receiver_send(const uint8_t *data, uint16_t length)
{
    RF_EventMask events;

    if ((data == NULL) ||
        (length == 0U) ||
        (length > RF_RECEIVER_MAX_PAYLOAD_LENGTH) ||
        (rf_handle == NULL))
    {
        return false;
    }

    RF_cmdPropTx.status = 0U;
    RF_cmdPropTx.pktLen = (uint8_t)length;
    RF_cmdPropTx.pPkt = (uint8_t *)data;
    events = RF_runCmd(rf_handle,
                       (RF_Op *)&RF_cmdPropTx,
                       RF_PriorityNormal,
                       NULL,
                       0U);

    return ((events & RF_EventLastCmdDone) != 0U) &&
           (RF_cmdPropTx.status == PROP_DONE_OK);
}

bool rf_receiver_read(uint8_t *data,
                      uint16_t capacity,
                      uint16_t *length,
                      uint32_t timeout_ms)
{
    rfc_dataEntryGeneral_t *entry;
    uint8_t *received_data;
    uint16_t received_length;
    RF_EventMask events;
    bool received;

    if (length != NULL)
    {
        *length = 0U;
    }

    if ((data == NULL) ||
        (capacity == 0U) ||
        (length == NULL) ||
        (timeout_ms == 0U) ||
        (rf_handle == NULL))
    {
        return false;
    }

    if (rf_rx_read_index >= RF_RX_ENTRY_COUNT)
    {
        return false;
    }

    entry = (rfc_dataEntryGeneral_t *)rf_rx_entry_pool[rf_rx_read_index];
    if ((entry->status != DATA_ENTRY_PENDING) ||
        (entry->length != RF_RX_ENTRY_DATA_SIZE))
    {
        return false;
    }

    RF_cmdPropRx.status = 0U;
    RF_cmdPropRx.endTrigger.triggerType = TRIG_REL_START;
    RF_cmdPropRx.endTime = RF_convertMsToRatTicks(timeout_ms);
    events = RF_runCmd(rf_handle,
                       (RF_Op *)&RF_cmdPropRx,
                       RF_PriorityNormal,
                       NULL,
                       0U);

    received = false;
    if (((events & RF_EventLastCmdDone) != 0U) &&
        (RF_cmdPropRx.status == PROP_DONE_OK) &&
        (entry->status == DATA_ENTRY_FINISHED))
    {
        received_data = &entry->data;
        received_length = received_data[0];
        if ((received_length > 0U) &&
            (received_length <= capacity) &&
            (received_length <= RF_RX_PAYLOAD_LENGTH) &&
            (received_length <=
             entry->length - RF_RX_LENGTH_BYTES - RF_RX_STATUS_BYTES))
        {
            memcpy(data, &received_data[1], received_length);
            *length = received_length;
            received = true;
        }
    }

    /* 已完成的Entry即使本次命令失败也要释放并推进读位置，避免读写错位。 */
    if (entry->status == DATA_ENTRY_FINISHED)
    {
        entry->status = DATA_ENTRY_PENDING;
        rf_rx_read_index = (rf_rx_read_index + 1U) % RF_RX_ENTRY_COUNT;
    }
    else
    {
        /* 超时未完成一帧时保留读位置，供下一次单包命令重新使用。 */
        entry->status = DATA_ENTRY_PENDING;
    }
    return received;
}
