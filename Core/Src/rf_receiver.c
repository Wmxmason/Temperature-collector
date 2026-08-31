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

#define RF_RX_APPENDED_BYTES                 (2U)
#define RF_RX_ENTRY_HEADER_SIZE              (8U)
#define RF_RX_ENTRY_DATA_SIZE                \
    (RF_RECEIVER_MAX_PAYLOAD_LENGTH + RF_RX_APPENDED_BYTES)
#define RF_RX_ENTRY_BUFFER_SIZE              \
    (RF_RX_ENTRY_HEADER_SIZE + RF_RX_ENTRY_DATA_SIZE)

static RF_Object rf_object;
static RF_Handle rf_handle;
static dataQueue_t rf_rx_queue;
static rfc_propRxOutput_t rf_rx_output;

#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_ALIGN(rf_rx_entry_buffer, 4)
#endif
static uint8_t rf_rx_entry_buffer[RF_RX_ENTRY_BUFFER_SIZE];

static void rf_receiver_queue_init(void)
{
    rfc_dataEntryGeneral_t *entry;

    memset(rf_rx_entry_buffer, 0, sizeof(rf_rx_entry_buffer));
    memset(&rf_rx_output, 0, sizeof(rf_rx_output));

    entry = (rfc_dataEntryGeneral_t *)rf_rx_entry_buffer;
    entry->pNextEntry = rf_rx_entry_buffer;
    entry->status = DATA_ENTRY_PENDING;
    entry->config.type = DATA_ENTRY_TYPE_GEN;
    entry->config.lenSz = 0U;
    entry->config.irqIntv = 0U;
    entry->length = RF_RX_ENTRY_DATA_SIZE;

    rf_rx_queue.pCurrEntry = rf_rx_entry_buffer;
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
    RF_cmdPropRx.maxPktLen = RF_RECEIVER_MAX_PAYLOAD_LENGTH;
    RF_cmdPropRx.pktConf.bRepeatOk = 0U;
    RF_cmdPropRx.pktConf.bRepeatNok = 1U;
    return true;
}

bool rf_receiver_read(uint8_t *data,
                      uint16_t capacity,
                      uint16_t *length)
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
        (rf_handle == NULL))
    {
        return false;
    }

    RF_cmdPropRx.status = 0U;
    events = RF_runCmd(rf_handle,
                       (RF_Op *)&RF_cmdPropRx,
                       RF_PriorityNormal,
                       NULL,
                       0U);

    entry = (rfc_dataEntryGeneral_t *)rf_rx_entry_buffer;
    received = false;
    if (((events & RF_EventLastCmdDone) != 0U) &&
        (RF_cmdPropRx.status == PROP_DONE_OK) &&
        (entry->status == DATA_ENTRY_FINISHED))
    {
        received_data = &entry->data;
        received_length = received_data[0];
        if ((received_length > 0U) &&
            (received_length <= capacity) &&
            (received_length <= RF_RECEIVER_MAX_PAYLOAD_LENGTH))
        {
            memcpy(data, &received_data[1], received_length);
            *length = received_length;
            received = true;
        }
    }

    entry->status = DATA_ENTRY_PENDING;
    return received;
}
