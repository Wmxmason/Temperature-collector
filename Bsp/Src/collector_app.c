#include <stddef.h>
#include <stdint.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>

#include "collector_app.h"
#include "collector_frame.h"
#include "collector_ring_buffer.h"
#include "rf_receiver.h"
#include "rs485.h"
#include "system_config.h"

static uint8_t collector_rx_buffer[RF_RECEIVER_MAX_PAYLOAD_LENGTH];
static uint8_t collector_request_buffer[COLLECTOR_TEMPERATURE_REQUEST_LENGTH];
static uint8_t collector_rf_task_stack[SYSTEM_RF_TASK_STACK_SIZE];
static uint8_t collector_upload_task_stack[SYSTEM_UPLOAD_TASK_STACK_SIZE];
static collector_ring_buffer_t collector_ring_buffer;
static Task_Struct collector_rf_task_struct;
static Task_Struct collector_upload_task_struct;
static Semaphore_Struct collector_ring_empty_sem_struct;
static Semaphore_Struct collector_ring_filled_sem_struct;
static Semaphore_Struct collector_ring_mutex_sem_struct;
static Semaphore_Handle collector_ring_empty_sem_handle;
static Semaphore_Handle collector_ring_filled_sem_handle;
static Semaphore_Handle collector_ring_mutex_sem_handle;
static volatile uint32_t collector_forwarded_count;
static volatile uint32_t collector_rf_error_count;
static volatile uint32_t collector_rs485_error_count;
static volatile uint32_t collector_ring_overflow_count;

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

static bool collector_ring_enqueue(const collector_frame_t *frame)
{
    bool queued;

    if ((frame == NULL) ||
        (frame->length > COLLECTOR_RING_BUFFER_FRAME_MAX_LENGTH) ||
        !Semaphore_pend(collector_ring_empty_sem_handle, BIOS_NO_WAIT))
    {
        return false;
    }

    if (!Semaphore_pend(collector_ring_mutex_sem_handle, BIOS_WAIT_FOREVER))
    {
        Semaphore_post(collector_ring_empty_sem_handle);
        return false;
    }

    queued = collector_ring_buffer_push(&collector_ring_buffer,
                                        frame->data,
                                        frame->length);
    Semaphore_post(collector_ring_mutex_sem_handle);

    if (queued)
    {
        Semaphore_post(collector_ring_filled_sem_handle);
    }
    else
    {
        Semaphore_post(collector_ring_empty_sem_handle);
    }

    return queued;
}

static bool collector_ring_dequeue(collector_ring_frame_t *frame)
{
    bool dequeued;

    if ((frame == NULL) ||
        !Semaphore_pend(collector_ring_filled_sem_handle,
                        BIOS_WAIT_FOREVER))
    {
        return false;
    }

    if (!Semaphore_pend(collector_ring_mutex_sem_handle, BIOS_WAIT_FOREVER))
    {
        Semaphore_post(collector_ring_filled_sem_handle);
        return false;
    }

    dequeued = collector_ring_buffer_pop(&collector_ring_buffer, frame);
    Semaphore_post(collector_ring_mutex_sem_handle);

    if (dequeued)
    {
        Semaphore_post(collector_ring_empty_sem_handle);
    }
    else
    {
        Semaphore_post(collector_ring_filled_sem_handle);
    }

    return dequeued;
}

static void collector_rf_task(UArg arg0, UArg arg1)
{
    collector_frame_t frame;
    uint16_t request_length;
    uint16_t received_length;
    uint32_t cycle_start_ticks;
    uint32_t elapsed_ticks;
    uint32_t interval_ticks;
    uint32_t remaining_ticks;
    uint32_t remaining_ms;

    (void)arg0;
    (void)arg1;

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

            if (frame.length > COLLECTOR_RING_BUFFER_FRAME_MAX_LENGTH)
            {
                collector_rf_error_count++;
                continue;
            }

            if (!collector_ring_enqueue(&frame))
            {
                collector_ring_overflow_count++;
            }
        }

        collector_wait_next_cycle(cycle_start_ticks);
    }
}

static void collector_upload_task(UArg arg0, UArg arg1)
{
    collector_ring_frame_t frame;

    (void)arg0;
    (void)arg1;

    if (!rs485_init())
    {
        collector_fatal_halt();
    }

    while (1)
    {
        if (!collector_ring_dequeue(&frame))
        {
            collector_fatal_halt();
        }

        if (!rs485_send(frame.data, frame.length))
        {
            collector_rs485_error_count++;
            continue;
        }

        collector_forwarded_count++;
    }
}

void collector_app_start(void)
{
    Semaphore_Params semaphore_params;
    Task_Params task_params;

    collector_ring_buffer_init(&collector_ring_buffer);

    Semaphore_Params_init(&semaphore_params);
    semaphore_params.mode = Semaphore_Mode_COUNTING;
    Semaphore_construct(&collector_ring_empty_sem_struct,
                        COLLECTOR_RING_BUFFER_CAPACITY,
                        &semaphore_params);
    Semaphore_construct(&collector_ring_filled_sem_struct,
                        0,
                        &semaphore_params);

    Semaphore_Params_init(&semaphore_params);
    semaphore_params.mode = Semaphore_Mode_BINARY_PRIORITY;
    Semaphore_construct(&collector_ring_mutex_sem_struct,
                        1,
                        &semaphore_params);

    collector_ring_empty_sem_handle =
        Semaphore_handle(&collector_ring_empty_sem_struct);
    collector_ring_filled_sem_handle =
        Semaphore_handle(&collector_ring_filled_sem_struct);
    collector_ring_mutex_sem_handle =
        Semaphore_handle(&collector_ring_mutex_sem_struct);

    Task_Params_init(&task_params);
    task_params.stack = collector_upload_task_stack;
    task_params.stackSize = sizeof(collector_upload_task_stack);
    task_params.priority = SYSTEM_UPLOAD_TASK_PRIORITY;
    Task_construct(&collector_upload_task_struct,
                   (Task_FuncPtr)collector_upload_task,
                   &task_params,
                   NULL);

    Task_Params_init(&task_params);
    task_params.stack = collector_rf_task_stack;
    task_params.stackSize = sizeof(collector_rf_task_stack);
    task_params.priority = SYSTEM_RF_TASK_PRIORITY;
    Task_construct(&collector_rf_task_struct,
                   (Task_FuncPtr)collector_rf_task,
                   &task_params,
                   NULL);
}
