#include <stddef.h>
#include <stdint.h>

#include <ti/sysbios/knl/Task.h>

#include "collector_app.h"
#include "collector_frame.h"
#include "rf_receiver.h"
#include "rs485.h"
#include "system_config.h"

static uint8_t collector_rx_buffer[RF_RECEIVER_MAX_PAYLOAD_LENGTH];
static volatile uint32_t collector_forwarded_count;
static volatile uint32_t collector_rf_error_count;
static volatile uint32_t collector_rs485_error_count;

static void collector_fatal_halt(void)
{
    while (1)
    {
        Task_sleep(SYSTEM_FATAL_SLEEP_TICKS);
    }
}

void *collector_app_main(void *argument)
{
    collector_frame_t frame;
    uint16_t received_length;

    (void)argument;

    if (!rs485_init())
    {
        collector_fatal_halt();
    }

    if (!rf_receiver_init())
    {
        collector_fatal_halt();
    }

    while (1)
    {
        if (!rf_receiver_read(collector_rx_buffer,
                              sizeof(collector_rx_buffer),
                              &received_length))
        {
            collector_rf_error_count++;
            continue;
        }

        if (!collector_frame_prepare(collector_rx_buffer,
                                     received_length,
                                     &frame))
        {
            collector_rf_error_count++;
            continue;
        }

        if (!rs485_send(frame.data, frame.length))
        {
            collector_rs485_error_count++;
            continue;
        }

        collector_forwarded_count++;
    }
}
