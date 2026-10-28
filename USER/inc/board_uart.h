#ifndef BOARD_UART_H
#define BOARD_UART_H

#include <stddef.h>
#include <stdint.h>

#define BOARD_UART_BAUD_RATE 115200u
#define BOARD_UART_TX_CAPACITY 1024u
#define BOARD_UART_RX_CAPACITY 128u

typedef enum
{
    BOARD_UART_OK = 0,
    BOARD_UART_NOT_READY,
    BOARD_UART_INVALID,
    BOARD_UART_FULL,
    BOARD_UART_EMPTY,
    BOARD_UART_RX_ERROR
} BoardUartStatus;

typedef struct
{
    uint32_t tx_rejected_count;
    uint32_t rx_error_count;
} BoardUartStats;

/** 在生成的时钟/GPIO初始化之后调用一次，配置USART2 115200 8N1。仅主循环。 */
void BoardUart_Init(void);

/** 每轮推进TX，最多32字节；不等待硬件就绪。仅主循环，不阻塞。 */
void BoardUart_Process(void);

/**
 * 非阻塞整块入队：成功后已复制length_bytes字节，调用方可立即复用数据。
 * 队列可用容量1023字节；满时整块拒绝，无部分入队。仅主循环调用。
 */
BoardUartStatus BoardUart_TryWrite(const void *data, size_t length_bytes);

/**
 * 非阻塞读取1字节。只有OK时写入*byte；EMPTY表示暂无数据，RX_ERROR表示
 * 自上次检查后有硬件错误或溢出，调用方应恢复接收解析。仅主循环调用。
 */
BoardUartStatus BoardUart_ReadByte(uint8_t *byte);

/** 返回累计拒绝发送次数、接收错误/溢出次数的快照。仅主循环，不阻塞。 */
BoardUartStats BoardUart_GetStats(void);

/** 仅USART2_IRQHandler调用；取走数据并写环形缓冲，不打印、不解析。 */
void BoardUart_IrqHandler(void);

#endif
