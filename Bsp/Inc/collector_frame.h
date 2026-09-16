#ifndef COLLECTOR_FRAME_H
#define COLLECTOR_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#include "system_config.h"

#define COLLECTOR_FRAME_MAX_LENGTH           \
    SYSTEM_RF_MAX_PAYLOAD_LENGTH
#define COLLECTOR_TEMPERATURE_REQUEST_LENGTH (6U)

typedef struct
{
    const uint8_t *data;
    uint16_t length;
} collector_frame_t;

/**
 * @brief 检查RF负载边界并保持原始数据视图
 *
 * @param data RF收到的完整负载，不含RF长度字节
 * @param length RF负载实际长度
 * @param frame 原始数据视图输出地址
 *
 * @return 参数与长度有效返回true，否则返回false
 * @note 本函数不解析、不复制、不修改数据，也不重新计算CRC
 */
bool collector_frame_prepare(const uint8_t *data,
                             uint16_t length,
                             collector_frame_t *frame);

/**
 * @brief 构造广播温度上报请求
 *
 * @param data 请求帧输出缓冲区
 * @param capacity 输出缓冲区容量
 * @param length 实际请求帧长度输出地址
 *
 * @return 构造成功返回true，否则返回false
 * @note 帧格式为FF FF FF 00 CRC低 CRC高，CRC仅覆盖4字节数据域
 */
bool collector_frame_build_temperature_request(uint8_t *data,
                                               uint16_t capacity,
                                               uint16_t *length);

#endif
