#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "collector_ring_buffer.h"

static void test_ring_buffer_preserves_fifo_order(void)
{
    collector_ring_buffer_t ring_buffer;
    collector_ring_frame_t frame;
    uint8_t data[COLLECTOR_RING_BUFFER_FRAME_MAX_LENGTH];
    uint8_t i;

    collector_ring_buffer_init(&ring_buffer);
    for (i = 0U; i < COLLECTOR_RING_BUFFER_CAPACITY; i++)
    {
        data[0] = i;
        assert(collector_ring_buffer_push(&ring_buffer, data, sizeof(data)));
    }

    for (i = 0U; i < COLLECTOR_RING_BUFFER_CAPACITY; i++)
    {
        assert(collector_ring_buffer_pop(&ring_buffer, &frame));
        assert(frame.length == sizeof(data));
        assert(frame.data[0] == i);
    }
}

static void test_ring_buffer_copies_original_frame(void)
{
    static const uint8_t expected[] = {
        0x00U, 0x01U, 0x7FU, 0x09U, 0x78U,
        0xC7U, 0x04U, 0xDFU, 0x6DU
    };
    collector_ring_buffer_t ring_buffer;
    collector_ring_frame_t frame;
    uint8_t data[sizeof(expected)];
    uint8_t i;

    for (i = 0U; i < sizeof(data); i++)
    {
        data[i] = expected[i];
    }

    collector_ring_buffer_init(&ring_buffer);
    assert(collector_ring_buffer_push(&ring_buffer, data, sizeof(data)));
    data[0] = 0xFFU;

    assert(collector_ring_buffer_pop(&ring_buffer, &frame));
    assert(frame.length == sizeof(expected));
    for (i = 0U; i < frame.length; i++)
    {
        assert(frame.data[i] == expected[i]);
    }
}

static void test_ring_buffer_rejects_full_and_invalid_frames(void)
{
    collector_ring_buffer_t ring_buffer;
    uint8_t data[COLLECTOR_RING_BUFFER_FRAME_MAX_LENGTH + 1U] = {0U};
    uint8_t i;

    collector_ring_buffer_init(&ring_buffer);
    assert(!collector_ring_buffer_push(NULL, data, 1U));
    assert(!collector_ring_buffer_push(&ring_buffer, NULL, 1U));
    assert(!collector_ring_buffer_push(&ring_buffer, data, 0U));
    assert(!collector_ring_buffer_push(&ring_buffer, data, sizeof(data)));

    for (i = 0U; i < COLLECTOR_RING_BUFFER_CAPACITY; i++)
    {
        assert(collector_ring_buffer_push(&ring_buffer, data, 1U));
    }
    assert(!collector_ring_buffer_push(&ring_buffer, data, 1U));
}

static void test_ring_buffer_rejects_empty_and_wraps_indices(void)
{
    collector_ring_buffer_t ring_buffer;
    collector_ring_frame_t frame;
    uint8_t data[1];
    uint8_t i;

    collector_ring_buffer_init(&ring_buffer);
    assert(!collector_ring_buffer_pop(&ring_buffer, &frame));
    assert(!collector_ring_buffer_pop(NULL, &frame));
    assert(!collector_ring_buffer_pop(&ring_buffer, NULL));

    for (i = 0U; i < (COLLECTOR_RING_BUFFER_CAPACITY * 2U); i++)
    {
        data[0] = i;
        assert(collector_ring_buffer_push(&ring_buffer, data, sizeof(data)));
        assert(collector_ring_buffer_pop(&ring_buffer, &frame));
        assert(frame.data[0] == i);
    }
}

int main(void)
{
    test_ring_buffer_preserves_fifo_order();
    test_ring_buffer_copies_original_frame();
    test_ring_buffer_rejects_full_and_invalid_frames();
    test_ring_buffer_rejects_empty_and_wraps_indices();
    return 0;
}
