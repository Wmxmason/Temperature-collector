#include <stddef.h>

#include "collector_frame.h"

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
