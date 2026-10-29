#include "board_can.h"

#include <stddef.h>
#include <string.h>

#include "board_pins.h"
#include "n32h47x_48x_fdcan.h"
#include "n32h47x_48x_rcc.h"

#define BOARD_CAN_RX_ELEMENTS 16u
#define BOARD_CAN_ELEMENT_WORDS 18u
#define BOARD_CAN_DIAGNOSTIC_MS 100u

static uint32_t can_message_ram[(BOARD_CAN_RX_ELEMENTS + 1u) * BOARD_CAN_ELEMENT_WORDS]
    __attribute__((aligned(4)));
static FDCAN_MsgRamType can_ram_info;
static bool is_can_ready;
static bool is_cancel_pending;
static uint32_t tx_started_ms;
static uint32_t last_diagnostic_ms;
static BoardCanTxState tx_state;
static BoardCanStats stats;
static const uint8_t dlc_bytes[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

bool BoardCan_Init(void)
{
    GPIO_InitType gpio;
    RCC_ClocksType clocks;
    FDCAN_InitType init = {0};

    is_can_ready = false;
    is_cancel_pending = false;
    tx_state = BOARD_CAN_TX_IDLE;
    memset(&stats, 0, sizeof(stats));
    last_diagnostic_ms = 0u;
    RCC_GetClocksFreqValue(&clocks);
    if ((clocks.SysclkFreq != 240000000u) || (RCC_GetSysclkSrc() != RCC_CFG_SCLKSTS_PLL) ||
        ((RCC->PLLCTRL & RCC_PLLCTRL_SCLKPLLSEL) != RCC_PLLCTRL_SCLKPLLSEL_PLL))
    {
        return false;
    }
    RCC_ConfigFDCANPllClk(RCC_FDCAN_PLLSRC_DIV6);
    RCC_ConfigFDCANClksrc(RCC_FDCAN_CLKSRC_PLL);
    RCC_EnableAHB1PeriphClk(RCC_AHB_PERIPHEN_GPIOA | RCC_AHB_PERIPHEN_GPIOB, ENABLE);
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_FDCAN1, ENABLE);
    FDCAN_DeInit(BOARD_CAN_DEVICE);

    GPIO_InitStruct(&gpio);
    gpio.GPIO_Mode = GPIO_MODE_AF_PP;
    gpio.GPIO_Pull = GPIO_PULL_UP;
    gpio.GPIO_Slew_Rate = GPIO_SLEW_RATE_FAST;
    gpio.GPIO_Current = GPIO_DS_8mA;
    gpio.Pin = BOARD_CAN_TX_PIN;
    gpio.GPIO_Alternate = BOARD_CAN_TX_AF;
    GPIO_InitPeripheral(BOARD_CAN_TX_PORT, &gpio);
    /* RX由收发器驱动。AF_PP是复用输出，会与外部RXD争用，不能沿用TX模式。 */
    gpio.GPIO_Mode = GPIO_MODE_INPUT;
    gpio.Pin = BOARD_CAN_RX_PIN;
    gpio.GPIO_Alternate = BOARD_CAN_RX_AF;
    GPIO_InitPeripheral(BOARD_CAN_RX_PORT, &gpio);

    init.FrameFormat = FDCAN_FRAME_FD_BRS;
    init.Mode = FDCAN_MODE_NORMAL;
    init.Prescaler = 1u;
    init.SyncJumpWidth = 20u;
    init.TimeSeg1 = 139u;
    init.TimeSeg2 = 20u;
    init.DataPrescaler = 1u;
    init.DataSyncJumpWidth = 4u; /* 采用总线正文值；SocketCAN示例dsjw8未照抄。 */
    init.DataTimeSeg1 = 14u;
    init.DataTimeSeg2 = 5u;
    init.MsgRamStrAddr = (uint32_t)can_message_ram;
    init.RxFifo0Size = BOARD_CAN_RX_ELEMENTS;
    init.RxFifo0DataSize = FDCAN_DATA_BYTES_64;
    init.TxBufferSize = 1u;
    init.TxBufferDataSize = FDCAN_DATA_BYTES_64;
    init.pMsgInfo = &can_ram_info;
    /* 首版单次硬件尝试，失败由软件记录，避免旧速度无限重传。 */
    init.AutoRetransmission = DISABLE;
    init.ProtocolException = ENABLE;
    if (FDCAN_Init(BOARD_CAN_DEVICE, &init) != SUCCESS)
    {
        return false;
    }
    FDCAN_EnableISOMode(BOARD_CAN_DEVICE);
    /* 联调时接收所有标准数据帧，保留广告等信息；解析层再按ID识别。 */
    FDCAN_ConfigGlobalFilter(BOARD_CAN_DEVICE, FDCAN_ACCEPT_STD_IN_RX_FIFO0,
                            FDCAN_REJECT_EXT, FDCAN_REJECT_STD_REMOTE,
                            FDCAN_REJECT_EXT_REMOTE);
    FDCAN_ConfigRxFifoMode(BOARD_CAN_DEVICE, FDCAN_RX_FIFO0, FDCAN_RX_FIFO_BLOCKING);
    FDCAN_Start(BOARD_CAN_DEVICE);
    is_can_ready = true;
    return true;
}

void BoardCan_CancelTx(void)
{
    if (is_can_ready && (tx_state == BOARD_CAN_TX_PENDING) && !is_cancel_pending)
    {
        FDCAN_AbortTxRequest(BOARD_CAN_DEVICE, FDCAN_TX_BUFFER0);
        is_cancel_pending = true;
    }
}

void BoardCan_Process(uint32_t now_ms)
{
    if (!is_can_ready)
    {
        return;
    }
    if ((uint32_t)(now_ms - last_diagnostic_ms) >= BOARD_CAN_DIAGNOSTIC_MS)
    {
        FDCAN_ProtocolStatus protocol;
        FDCAN_ErrorCounters errors;
        last_diagnostic_ms = now_ms;
        FDCAN_GetProtocolStatus(BOARD_CAN_DEVICE, &protocol);
        FDCAN_GetErrorCounters(BOARD_CAN_DEVICE, &errors);
        stats.is_bus_off = protocol.BusOff == FDCAN_BUS_OFF;
        stats.last_error = protocol.LastError;
        stats.last_data_error = protocol.LastDataError >> 8;
        stats.activity = protocol.Activity >> 3;
        stats.tx_error_count = errors.TxErrorCnt;
        stats.rx_error_count = errors.RxErrorCnt;
        if (FDCAN_GetFlag(BOARD_CAN_DEVICE, FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST) != RESET)
        {
            ++stats.rx_lost;
            FDCAN_ClearFlag(BOARD_CAN_DEVICE, FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST);
        }
    }
    stats.controller_control = BOARD_CAN_DEVICE->CCCR;
    stats.is_rx_high = GPIO_ReadInputDataBit(BOARD_CAN_RX_PORT, BOARD_CAN_RX_PIN) != 0u;
    if (tx_state != BOARD_CAN_TX_PENDING)
    {
        return;
    }
    /* 先确认请求已结束，再读取完成位，避免两次读取间发送完成而误报失败。 */
    if (FDCAN_CheckTxBufRequest(BOARD_CAN_DEVICE, FDCAN_TX_BUFFER0) == RESET)
    {
        if (FDCAN_CheckBufTxResult(BOARD_CAN_DEVICE, FDCAN_TX_BUFFER0) != RESET)
        {
            tx_state = BOARD_CAN_TX_SENT;
        }
        else
        {
            tx_state = BOARD_CAN_TX_FAILED;
            ++stats.tx_failed;
        }
    }
    else if ((uint32_t)(now_ms - tx_started_ms) >= BOARD_CAN_TX_TIMEOUT_MS)
    {
        BoardCan_CancelTx();
        /* 取消未获硬件确认时保留PENDING，不复用或覆盖缓冲。 */
    }
}

BoardCanResult BoardCan_TrySend(const BoardCanFrame *frame, uint32_t now_ms)
{
    FDCAN_TxHeaderType header = {0};
    uint8_t dlc = 0u;

    if (!is_can_ready || stats.is_bus_off)
    {
        return BOARD_CAN_NOT_READY;
    }
    if ((frame == NULL) || (frame->id > 0x7FFu) || frame->is_extended ||
        !frame->is_fd || !frame->has_brs || (frame->length > 64u))
    {
        return BOARD_CAN_INVALID;
    }
    while ((dlc < 15u) && (dlc_bytes[dlc] < frame->length))
    {
        ++dlc;
    }
    if (dlc_bytes[dlc] != frame->length)
    {
        return BOARD_CAN_INVALID;
    }
    if ((tx_state == BOARD_CAN_TX_PENDING) ||
        (FDCAN_CheckTxBufRequest(BOARD_CAN_DEVICE, FDCAN_TX_BUFFER0) != RESET))
    {
        return BOARD_CAN_BUSY;
    }
    header.ID = frame->id;
    header.IdType = FDCAN_STANDARD_ID;
    header.TxFrameType = FDCAN_DATA_FRAME;
    header.DataLength = (uint32_t)dlc << FDCAN_ELEMENT_DLC_OFFSET;
    header.ErrorState = FDCAN_ESI_ACTIVE;
    header.BitRateSwitch = FDCAN_BRS_ON;
    header.FDFormat = FDCAN_FD_CAN;
    if (FDCAN_AddMsgToTxBuffer(BOARD_CAN_DEVICE, &header,
                             (uint8_t *)frame->data, FDCAN_TX_BUFFER0) != SUCCESS)
    {
        return BOARD_CAN_ERROR;
    }
    FDCAN_EnableTxBufferRequest(BOARD_CAN_DEVICE, FDCAN_TX_BUFFER0);
    tx_state = BOARD_CAN_TX_PENDING;
    tx_started_ms = now_ms;
    is_cancel_pending = false;
    return BOARD_CAN_OK;
}

BoardCanTxState BoardCan_GetTxState(void)
{
    return tx_state;
}

BoardCanResult BoardCan_Read(BoardCanFrame *frame)
{
    FDCAN_RxHeaderType header;

    if (!is_can_ready)
    {
        return BOARD_CAN_NOT_READY;
    }
    if (frame == NULL)
    {
        return BOARD_CAN_INVALID;
    }
    if (FDCAN_GetRxFifoFillLevel(BOARD_CAN_DEVICE, FDCAN_RX_FIFO0) == 0u)
    {
        return BOARD_CAN_EMPTY;
    }
    if (FDCAN_GetRxMsg(BOARD_CAN_DEVICE, FDCAN_RX_FIFO0, &header, frame->data) != SUCCESS)
    {
        return BOARD_CAN_ERROR;
    }
    frame->id = header.ID;
    frame->length = dlc_bytes[(header.DataLength >> FDCAN_ELEMENT_DLC_OFFSET) & 15u];
    frame->is_extended = header.IdType != FDCAN_STANDARD_ID;
    frame->is_fd = header.FDFormat == FDCAN_FD_CAN;
    frame->has_brs = header.BitRateSwitch == FDCAN_BRS_ON;
    return BOARD_CAN_OK;
}

BoardCanStats BoardCan_GetStats(void)
{
    return stats;
}
