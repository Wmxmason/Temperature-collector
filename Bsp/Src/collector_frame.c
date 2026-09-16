#include <stddef.h>

#include "collector_frame.h"

#define COLLECTOR_FRAME_HEADER_0              (0xFFU)
#define COLLECTOR_FRAME_HEADER_1              (0xFFU)
#define COLLECTOR_FRAME_BROADCAST_COMMAND     (0xFFU)
#define COLLECTOR_FRAME_RESERVED              (0x00U)
#define COLLECTOR_FRAME_CRC_INITIAL_VALUE     (0xFFFFU)
#define COLLECTOR_FRAME_CRC_POLYNOMIAL        (0xA001U)

static uint16_t collector_frame_crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc;
    uint16_t i;
    uint8_t bit;

    crc = COLLECTOR_FRAME_CRC_INITIAL_VALUE;
    for (i = 0U; i < length; i++)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1U) ^ COLLECTOR_FRAME_CRC_POLYNOMIAL;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

bool collector_frame_prepare(const uint8_t *data,
                             uint16_t length,
                             collector_frame_t *frame)
{
    if ((data == NULL) ||
        (length == 0U) ||
        (length > COLLECTOR_FRAME_MAX_LENGTH) ||
        (frame == NULL))
    {
        return false;
    }

    frame->data = data;
    frame->length = length;
    return true;
}

bool collector_frame_build_temperature_request(uint8_t *data,
                                               uint16_t capacity,
                                               uint16_t *length)
{
    uint16_t crc;

    if (length != NULL)
    {
        *length = 0U;
    }

    if ((data == NULL) ||
        (capacity < COLLECTOR_TEMPERATURE_REQUEST_LENGTH) ||
        (length == NULL))
    {
        return false;
    }

    data[0] = COLLECTOR_FRAME_HEADER_0;
    data[1] = COLLECTOR_FRAME_HEADER_1;
    data[2] = COLLECTOR_FRAME_BROADCAST_COMMAND;
    data[3] = COLLECTOR_FRAME_RESERVED;

    crc = collector_frame_crc16(data,
                                COLLECTOR_TEMPERATURE_REQUEST_LENGTH - 2U);
    data[4] = (uint8_t)(crc & 0xFFU);
    data[5] = (uint8_t)(crc >> 8U);
    *length = COLLECTOR_TEMPERATURE_REQUEST_LENGTH;
    return true;
}
