#ifndef CAN_TRACE_H
#define CAN_TRACE_H

#include "board_can.h"

typedef enum
{
    CAN_TRACE_TX_REQUEST = 0,
    CAN_TRACE_TX_SENT,
    CAN_TRACE_RX
} CanTraceType;

/** 初始化16条原始帧日志队列；仅主循环，不等待。 */
void CanTrace_Init(void);

/** 复制原始帧和本地事务号；满时丢新并计数，false表示未记录，不影响CAN收发。
 * 不保留指针；仅主循环、不等待。事务号仅本地日志，不加入协议。
 */
bool CanTrace_Push(CanTraceType type, uint32_t sequence, const BoardCanFrame *frame);

/** 每轮最多输出一个32字节分片；UART满时留待下轮，溢出计数最多每秒报告。
 * 记录的是日志队列顺序，时间前缀是输出时间；仅主循环、不等待。
 */
void CanTrace_Process(uint32_t now_ms);

#endif
