#ifndef BOARD_CAN_H
#define BOARD_CAN_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD_CAN_TX_TIMEOUT_MS 200u

typedef struct
{
    uint32_t id;
    uint8_t length; /* DLC对应的物理长度，0~64。 */
    bool is_extended;
    bool is_fd;
    bool has_brs;
    uint8_t data[64];
} BoardCanFrame;

typedef enum
{
    BOARD_CAN_OK = 0,
    BOARD_CAN_EMPTY,
    BOARD_CAN_BUSY,
    BOARD_CAN_NOT_READY,
    BOARD_CAN_INVALID,
    BOARD_CAN_ERROR
} BoardCanResult;

typedef enum
{
    BOARD_CAN_TX_IDLE = 0,
    BOARD_CAN_TX_PENDING,
    BOARD_CAN_TX_SENT,
    BOARD_CAN_TX_FAILED
} BoardCanTxState;

typedef struct
{
    uint32_t rx_lost;
    uint32_t tx_failed;
    uint32_t tx_error_count;
    uint32_t rx_error_count;
    uint32_t last_error;
    uint32_t last_data_error;
    uint32_t activity; /* 0同步，1空闲，2接收，3发送。 */
    uint32_t controller_control; /* CCCR原值，用于区分INIT/正常模式。 */
    bool is_bus_off;
    bool is_rx_high; /* PB3输入采样，空闲通常应为高。 */
} BoardCanStats;

/** 手工配置FDCAN1/PA15/PB3，250k/2M FD+BRS，PLL240M/6=40MHz。
 * 使用静态SRAM消息区，正常模式，无中断；仅初始化阶段有SDK有界等待。
 * false表示时钟或初始化失败，不会自动改为其他速率；仅主循环。
 */
bool BoardCan_Init(void);

/** 推进单个发送状态及诊断；发送200ms超时请求取消，不阻塞，仅主循环。 */
void BoardCan_Process(uint32_t now_ms);

/** 复制帧至硬件专用缓冲；OK仅表示请求已提交，不表示已发送或FP2应答。
 * 只支持标准FD+BRS数据帧，length必须为合法DLC长度；不保留指针。
 * BUSY/NOT_READY/INVALID/ERROR不提交；仅主循环，不等待。
 */
BoardCanResult BoardCan_TrySend(const BoardCanFrame *frame, uint32_t now_ms);

/** 返回最近一次请求状态，直到下一次TrySend成功或取消；仅主循环，不等待。 */
BoardCanTxState BoardCan_GetTxState(void);

/** 读取并复制一帧；EMPTY表示暂时无数据，ERROR表示读取失败，不保留指针。
 * 每次最多一帧，主循环调用方必须限制每轮次数，不等待。
 */
BoardCanResult BoardCan_Read(BoardCanFrame *frame);

/** 请求取消当前待发帧，不等待确认；确认前不能提交新帧。仅主循环。 */
void BoardCan_CancelTx(void);

/** 返回硬件/丢帧统计快照；仅主循环，不等待。 */
BoardCanStats BoardCan_GetStats(void);

#endif
