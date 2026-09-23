#include <stddef.h>
#include <string.h>

#include "collector_ring_buffer.h"

void collector_ring_buffer_init(collector_ring_buffer_t *ring_buffer)
{
    if (ring_buffer == NULL)
    {
        return;
    }

    memset(ring_buffer, 0, sizeof(*ring_buffer));
}

bool collector_ring_buffer_push(collector_ring_buffer_t *ring_buffer,
                                const uint8_t *data,
                                uint16_t length)
{
    collector_ring_frame_t *frame;

    if ((ring_buffer == NULL) ||
        (data == NULL) ||
        (length == 0U) ||
        (length > COLLECTOR_RING_BUFFER_FRAME_MAX_LENGTH) ||
        (ring_buffer->count >= COLLECTOR_RING_BUFFER_CAPACITY))
    {
        return false;
    }

    frame = &ring_buffer->frames[ring_buffer->write_index];
    memcpy(frame->data, data, length);
    frame->length = (uint8_t)length;

    ring_buffer->write_index++;
    if (ring_buffer->write_index >= COLLECTOR_RING_BUFFER_CAPACITY)
    {
        ring_buffer->write_index = 0U;
    }
    ring_buffer->count++;
    return true;
}

bool collector_ring_buffer_pop(collector_ring_buffer_t *ring_buffer,
                               collector_ring_frame_t *frame)
{
    if ((ring_buffer == NULL) ||
        (frame == NULL) ||
        (ring_buffer->count == 0U))
    {
        return false;
    }

    *frame = ring_buffer->frames[ring_buffer->read_index];

    ring_buffer->read_index++;
    if (ring_buffer->read_index >= COLLECTOR_RING_BUFFER_CAPACITY)
    {
        ring_buffer->read_index = 0U;
    }
    ring_buffer->count--;
    return true;
}
