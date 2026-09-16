# CC1310 RF-RS485采集器

本工程是自制CC1310采集器的最终独立工程，只承担一条数据链路：

```text
温度节点 -> RF中继 -> 本采集器 -> MAX3485 -> USB-RS485 -> PC
```

## 功能边界

- 采集器上电后立即广播一次温度上报请求，之后每30秒广播一次。
- 广播请求为 `FF FF FF 00 41 F0`，其中功能码为 `0xFF`、备用字段为0，CRC仅覆盖4字节数据域、低字节在前。
- 每次广播结束后立即转为接收，并持续接收、转发RF帧，直到下一次30秒广播时刻。
- RF负载被视为已经组好的完整二进制业务帧，采集器不解析温度、不修改字段、不重新计算CRC。
- 收到的RF负载按原长度、原字节顺序直接通过RS485发送给PC。
- PC只需要被动监听，不需要向采集器发送Modbus查询。
- 接收超时不上传数据，也不会重复发送上一轮的历史帧。
- RF发送/接收失败、非法长度或UART发送失败只丢弃当前周期，采集任务继续运行。

这里的“Modbus RTU传输”指UART/RS485使用Modbus RTU常用的串口和CRC字节约定；采集器不会在RF负载外再封装一层Modbus报文。

## 工程架构

```text
Bsp/          采集器桥接逻辑、透明帧边界检查及MAX3485/RS485交互
Core/         CC1310内部RF外设驱动
Driver/       TI SmartRF生成的厂商驱动配置
Middlewares/  第三方中间件层，当前没有依赖
System/       板级定义、CC1310_LAUNCHXL、系统宏、CCFG及链接配置
Tasks/        TI-RTOS入口main_tirtos.c
tests/        不参与固件编译的主机侧边界与架构测试
targetConfigs/ CCS调试目标配置
```

## 硬件与串口参数

- DIO1：UART RX，连接MAX3485 RO
- DIO2：UART TX，连接MAX3485 DI
- DIO4：连接MAX3485 DE与低有效RE并联端
- UART：9600 bit/s、8位数据位、无校验、1位停止位
- RTS、DTR：关闭
- MAX3485与CC1310均使用3.3V逻辑并共地

## 构建与烧录

工具链：

- Code Composer Studio 21 / Theia
- SimpleLink CC13x0 SDK 4.20.02.07
- TI ARM Compiler 18.12.5.LTS
- TI-RTOS / SYS-BIOS

在CCS中导入本目录中的工程，选择 `Debug` 或 `Release` 配置后构建并烧录对应的 `rf.out`。

调试器中可以观察以下计数器：

- `collector_forwarded_count`：成功转发帧数
- `collector_rf_error_count`：RF接收或长度异常次数
- `collector_rs485_error_count`：RS485发送失败次数
