#ifndef RF_RECEIVER_H
#define RF_RECEIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "system_config.h"

#define RF_RECEIVER_MAX_PAYLOAD_LENGTH       \
    SYSTEM_RF_MAX_PAYLOAD_LENGTH

/**
 * @brief 初始化CC1310专用RF接收链路
 *
 * @return 初始化成功返回true，否则返回false
 */
bool rf_receiver_init(void);

/**
 * @brief 阻塞等待一个RF负载并复制到调用者缓冲区
 *
 * @param data 负载输出缓冲区
 * @param capacity 输出缓冲区容量
 * @param length 实际RF负载长度输出地址
 *
 * @return 收到一个边界有效的数据包返回true，否则返回false
 * @note 输出数据不包含RF可变长度模式自动附加的长度字节
 */
bool rf_receiver_read(uint8_t *data,
                      uint16_t capacity,
                      uint16_t *length);

#endif
