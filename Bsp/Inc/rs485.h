#ifndef RS485_H
#define RS485_H

#include <stdbool.h>
#include <stdint.h>

#include "system_config.h"

#define RS485_FRAME_MAX_LENGTH               \
    SYSTEM_RF_MAX_PAYLOAD_LENGTH

/**
 * @brief 初始化MAX3485方向控制和UART0
 *
 * @return 初始化成功返回true，否则返回false
 * @note 串口固定为9600 bit/s、8N1、无流控
 */
bool rs485_init(void);

/**
 * @brief 通过RS485发送一帧二进制数据
 *
 * @param data 待发送数据
 * @param length 数据长度，范围为1~RS485_FRAME_MAX_LENGTH
 *
 * @return 完整发送并恢复接收方向返回true，否则返回false
 * @note 本接口不组帧、不解析数据、不计算CRC
 */
bool rs485_send(const uint8_t *data, uint16_t length);

#endif
