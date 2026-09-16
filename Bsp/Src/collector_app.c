#include <stddef.h>
#include <stdint.h>

#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>

#include "collector_app.h"
#include "collector_frame.h"
#include "rf_receiver.h"
#include "rs485.h"
#include "system_config.h"

static uint8_t collector_rx_buffer[RF_RECEIVER_MAX_PAYLOAD_LENGTH];
static uint8_t collector_request_buffer[COLLECTOR_TEMPERATURE_REQUEST_LENGTH];
static volatile uint32_t collector_forwarded_count;
static volatile uint32_t collector_rf_error_count;
static volatile uint32_t collector_rs485_error_count;

static uint32_t collector_ms_to_ticks(uint32_t duration_ms)
{
    return ((duration_ms * 1000U) + Clock_tickPeriod - 1U) /
           Clock_tickPeriod;
}

static uint32_t collector_ticks_to_ms(uint32_t duration_ticks)
{
    uint64_t duration_us;

    duration_us = (uint64_t)duration_ticks * Clock_tickPeriod;
    return (uint32_t)((duration_us + 999U) / 1000U);
}

static void collector_wait_next_cycle(uint32_t cycle_start_ticks)
{
    uint32_t elapsed_ticks;
    uint32_t interval_ticks;

    elapsed_ticks = Clock_getTicks() - cycle_start_ticks;
    interval_ticks =
        collector_ms_to_ticks(SYSTEM_COLLECTOR_BROADCAST_INTERVAL_MS);
    if (elapsed_ticks < interval_ticks)
    {
        Task_sleep(interval_ticks - elapsed_ticks);
    }
}

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
    uint16_t request_length;
    uint16_t received_length;
    uint32_t cycle_start_ticks;
    uint32_t elapsed_ticks;
    uint32_t interval_ticks;
    uint32_t remaining_ticks;
    uint32_t remaining_ms;

    (void)argument;

    if (!rs485_init())
    {
        collector_fatal_halt();
    }

    if (!rf_receiver_init())
    {
        collector_fatal_halt();
    }

    if (!collector_frame_build_temperature_request(
            collector_request_buffer,
            sizeof(collector_request_buffer),
            &request_length))
    {
        collector_fatal_halt();
    }

    while (1)
    {
        cycle_start_ticks = Clock_getTicks();
        interval_ticks =
            collector_ms_to_ticks(SYSTEM_COLLECTOR_BROADCAST_INTERVAL_MS);

        if (!rf_receiver_send(collector_request_buffer, request_length))
        {
            collector_rf_error_count++;
            collector_wait_next_cycle(cycle_start_ticks);
            continue;
        }

        while (1)
        {
            elapsed_ticks = Clock_getTicks() - cycle_start_ticks;
            if (elapsed_ticks >= interval_ticks)
            {
                break;
            }

            remaining_ticks = interval_ticks - elapsed_ticks;
            remaining_ms = collector_ticks_to_ms(remaining_ticks);
            if (!rf_receiver_read(collector_rx_buffer,
                                  sizeof(collector_rx_buffer),
                                  &received_length,
                                  remaining_ms))
            {
                break;
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

        collector_wait_next_cycle(cycle_start_ticks);
    }
}
