#ifndef COLLECTOR_RING_BUFFER_H
#define COLLECTOR_RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#define COLLECTOR_RING_BUFFER_CAPACITY          (5U)
#define COLLECTOR_RING_BUFFER_FRAME_MAX_LENGTH  (9U)

typedef struct
{
    uint8_t data[COLLECTOR_RING_BUFFER_FRAME_MAX_LENGTH];
    uint8_t length;
} collector_ring_frame_t;

typedef struct
{
    collector_ring_frame_t frames[COLLECTOR_RING_BUFFER_CAPACITY];
    uint8_t read_index;
    uint8_t write_index;
    uint8_t count;
} collector_ring_buffer_t;

void collector_ring_buffer_init(collector_ring_buffer_t *ring_buffer);

bool collector_ring_buffer_push(collector_ring_buffer_t *ring_buffer,
                                const uint8_t *data,
                                uint16_t length);

bool collector_ring_buffer_pop(collector_ring_buffer_t *ring_buffer,
                               collector_ring_frame_t *frame);

#endif
