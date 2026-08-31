#include <stddef.h>
#include <stdint.h>

#include <ti/devices/cc13x0/driverlib/cpu.h>
#include <ti/devices/cc13x0/inc/hw_memmap.h>
#include <ti/devices/cc13x0/inc/hw_types.h>
#include <ti/devices/cc13x0/inc/hw_uart.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/UART.h>

#include "Board.h"
#include "rs485.h"
#include "system_config.h"

static PIN_State rs485_pin_state;
static PIN_Handle rs485_pin_handle;
static UART_Handle rs485_uart_handle;

static const PIN_Config rs485_pin_config[] =
{
    Board_RS485_DE_RE_PIN | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW |
    PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

static bool rs485_tx_drained(void)
{
    uint32_t retry;

    for (retry = 0U; retry < SYSTEM_RS485_TX_DRAIN_RETRIES; retry++)
    {
        uint32_t flags;

        flags = HWREG(UART0_BASE + UART_O_FR);
        if (((flags & UART_FR_TXFE) != 0U) &&
            ((flags & UART_FR_BUSY) == 0U))
        {
            return true;
        }

        CPUdelay(4U);
    }

    return false;
}

bool rs485_init(void)
{
    UART_Params params;

    if ((rs485_pin_handle != NULL) && (rs485_uart_handle != NULL))
    {
        return true;
    }

    rs485_pin_handle = PIN_open(&rs485_pin_state, rs485_pin_config);
    if (rs485_pin_handle == NULL)
    {
        return false;
    }

    UART_init();
    UART_Params_init(&params);
    params.baudRate = SYSTEM_RS485_BAUD_RATE;
    params.readDataMode = UART_DATA_BINARY;
    params.writeDataMode = UART_DATA_BINARY;
    params.readMode = UART_MODE_BLOCKING;
    params.writeMode = UART_MODE_BLOCKING;
    params.readEcho = UART_ECHO_OFF;
    params.dataLength = UART_LEN_8;
    params.stopBits = UART_STOP_ONE;
    params.parityType = UART_PAR_NONE;

    rs485_uart_handle = UART_open(Board_UART0, &params);
    if (rs485_uart_handle == NULL)
    {
        PIN_close(rs485_pin_handle);
        rs485_pin_handle = NULL;
        return false;
    }

    return true;
}

bool rs485_send(const uint8_t *data, uint16_t length)
{
    int_fast32_t write_length;
    bool sent;
    bool direction_restored;

    if ((data == NULL) ||
        (length == 0U) ||
        (length > RS485_FRAME_MAX_LENGTH) ||
        (rs485_pin_handle == NULL) ||
        (rs485_uart_handle == NULL))
    {
        return false;
    }

    if (PIN_setOutputValue(rs485_pin_handle,
                           Board_RS485_DE_RE_PIN,
                           1U) != PIN_SUCCESS)
    {
        return false;
    }

    write_length = UART_write(rs485_uart_handle, data, length);
    sent = (write_length == (int_fast32_t)length) && rs485_tx_drained();

    direction_restored =
        (PIN_setOutputValue(rs485_pin_handle,
                            Board_RS485_DE_RE_PIN,
                            0U) == PIN_SUCCESS);

    return sent && direction_restored;
}
