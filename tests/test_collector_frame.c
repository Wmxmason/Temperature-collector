#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "collector_frame.h"

static void test_temperature_request_matches_protocol(void)
{
    static const uint8_t expected[] = {
        0xFFU, 0xFFU, 0xFFU, 0x00U, 0x41U, 0xF0U
    };
    uint8_t frame[COLLECTOR_TEMPERATURE_REQUEST_LENGTH];
    uint16_t length;
    uint16_t i;

    assert(collector_frame_build_temperature_request(frame,
                                                     sizeof(frame),
                                                     &length));
    assert(length == sizeof(expected));
    for (i = 0U; i < length; i++)
    {
        assert(frame[i] == expected[i]);
    }
}

static void test_temperature_request_rejects_invalid_arguments(void)
{
    uint8_t frame[COLLECTOR_TEMPERATURE_REQUEST_LENGTH];
    uint16_t length;

    length = 1U;
    assert(!collector_frame_build_temperature_request(NULL,
                                                      sizeof(frame),
                                                      &length));
    assert(length == 0U);

    length = 1U;
    assert(!collector_frame_build_temperature_request(
        frame,
        COLLECTOR_TEMPERATURE_REQUEST_LENGTH - 1U,
        &length));
    assert(length == 0U);

    assert(!collector_frame_build_temperature_request(frame,
                                                      sizeof(frame),
                                                      NULL));
}

static void test_valid_payload_is_not_modified(void)
{
    static const uint8_t payload[] = {
        0x00U, 0x01U, 0x7FU, 0x09U, 0x78U,
        0xC7U, 0x04U, 0xDFU, 0x6DU
    };
    collector_frame_t frame;

    assert(collector_frame_prepare(payload, sizeof(payload), &frame));
    assert(frame.data == payload);
    assert(frame.length == sizeof(payload));
}

static void test_all_payload_lengths_are_preserved(void)
{
    static uint8_t payload[COLLECTOR_FRAME_MAX_LENGTH];
    collector_frame_t frame;

    assert(collector_frame_prepare(payload, 1U, &frame));
    assert(frame.length == 1U);

    assert(collector_frame_prepare(payload, sizeof(payload), &frame));
    assert(frame.length == sizeof(payload));
}

static void test_invalid_arguments_are_rejected(void)
{
    static uint8_t payload[COLLECTOR_FRAME_MAX_LENGTH + 1U];
    collector_frame_t frame;

    assert(!collector_frame_prepare(NULL, 1U, &frame));
    assert(!collector_frame_prepare(payload, 0U, &frame));
    assert(!collector_frame_prepare(payload, sizeof(payload), &frame));
    assert(!collector_frame_prepare(payload, 1U, NULL));
}

int main(void)
{
    test_temperature_request_matches_protocol();
    test_temperature_request_rejects_invalid_arguments();
    test_valid_payload_is_not_modified();
    test_all_payload_lengths_are_preserved();
    test_invalid_arguments_are_rejected();
    return 0;
}
