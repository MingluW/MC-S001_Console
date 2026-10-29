#include "powerboard.h"

#include <stddef.h>
#include <string.h>

#include "board_can.h"
#include "can_trace.h"
#include "fp2.h"
#include "log.h"

#define POWERBOARD_REASSEMBLY_MS 200u

typedef enum
{
    TRANSFER_IDLE = 0,
    TRANSFER_WAIT_BUS,
    TRANSFER_WAIT_REPLY
} TransferState;

static bool is_service_ready;
static bool is_run_requested;
static bool is_faulted;
static bool has_speed_target;
static bool has_stop_target;
static bool needs_subscription;
static bool is_preempting;
static bool active_is_stop;
static bool stop_has_failed;
static uint32_t stop_failed_ms;
static uint32_t process_time_ms;
static uint16_t requested_speed_x10;
static uint8_t active_command;
static uint32_t active_sequence;
static uint32_t reply_started_ms;
static uint32_t bus_requested_ms;
static bool reported_bus_timeout;
static uint32_t last_status_ms;
static uint32_t summary_lost;
static uint32_t reported_summary_lost;
static uint32_t observed_rx_lost;
static uint32_t observed_tx_failed;
static bool reported_bus_off;
static TransferState transfer;
static BoardCanFrame active_frame;
static uint8_t assembly[FP2_MAX_MESSAGE_BYTES];
static size_t assembly_length;
static size_t expected_length;
static uint32_t last_fragment_ms;

static void finish_request(bool success)
{
    if (!success && active_is_stop)
    {
        has_stop_target = true;
        stop_has_failed = true;
        stop_failed_ms = process_time_ms;
    }
    if (!success && (active_command == 0x12u) && !is_preempting)
    {
        /* 不自动重试运行目标。再次明确按开始才解除，STOP仍允许发送。 */
        is_faulted = true;
        is_run_requested = false;
        has_speed_target = false;
        needs_subscription = false;
    }
    transfer = TRANSFER_IDLE;
}

bool Powerboard_Init(void)
{
    CanTrace_Init();
    is_service_ready = BoardCan_Init();
    is_run_requested = is_faulted = has_speed_target = has_stop_target = false;
    needs_subscription = is_preempting = reported_bus_off = false;
    active_is_stop = stop_has_failed = false;
    requested_speed_x10 = 0u;
    active_sequence = summary_lost = reported_summary_lost = observed_rx_lost = 0u;
    observed_tx_failed = 0u;
    assembly_length = expected_length = 0u;
    last_status_ms = 0u;
    transfer = TRANSFER_IDLE;
    if (!DBG("[CAN] init=%s 250k/2M FD+BRS TX=%03X RX=%03X; heartbeat OFF",
             is_service_ready ? "OK" : "FAILED", POWERBOARD_TX_ID, POWERBOARD_RX_ID))
    {
        ++summary_lost;
    }
    return is_service_ready;
}

bool Powerboard_RequestStart(uint16_t speed_x10)
{
    if (!is_service_ready || (speed_x10 > 300u))
    {
        return false;
    }
    is_run_requested = true;
    is_faulted = false;
    needs_subscription = true;
    requested_speed_x10 = speed_x10;
    has_speed_target = true;
    return true;
}

bool Powerboard_RequestSpeed(uint16_t speed_x10)
{
    if (!is_service_ready || !is_run_requested || is_faulted || (speed_x10 > 300u))
    {
        return false;
    }
    requested_speed_x10 = speed_x10;
    has_speed_target = true;
    return true;
}

bool Powerboard_RequestStop(void)
{
    if (!is_service_ready)
    {
        return false;
    }
    is_run_requested = false;
    has_speed_target = false;
    needs_subscription = false;
    has_stop_target = true;
    stop_has_failed = false;
    if (transfer != TRANSFER_IDLE)
    {
        is_preempting = true;
        BoardCan_CancelTx();
        if (transfer == TRANSFER_WAIT_REPLY)
        {
            transfer = TRANSFER_IDLE;
        }
    }
    return true;
}

static void decoded_process(const uint8_t *data, size_t length)
{
    Fp2Message message;
    if (!Fp2_Decode(data, length, &message) || (message.sender != 2u))
    {
        if (!WARN("[FP2 RX] invalid structure/sender, bytes=%u", (unsigned)length))
        {
            ++summary_lost;
        }
        return;
    }
    if ((message.command == 3u) || (message.command == 4u))
    {
        uint8_t command = message.replied_command;
        bool matches = (transfer == TRANSFER_WAIT_REPLY) &&
            ((command == active_command) || (command == (active_command & 15u)));
        if (!Log_Write(message.command == 4u ? LOG_LEVEL_WARN : LOG_LEVEL_DBG,
                       "[FP2 %s] cmd=%02X error=%u match=%u; ACK has no feature/sequence",
                       message.command == 3u ? "ACK" : "ERROR", command, message.error, matches))
        {
            ++summary_lost;
        }
        if (matches)
        {
            finish_request(message.command == 3u);
        }
    }
    else if (message.command == 5u)
    {
        if ((message.feature == FP2_CURRENT_KPH) &&
            (message.value >= 0.0f) && (message.value <= 100.0f))
        {
            uint32_t speed_x10 = (uint32_t)(message.value * 10.0f + 0.5f);
            if (!DBG("[FP2 EVENT] current speed=%lu.%lu KPH bits=%08lX",
                     (unsigned long)(speed_x10 / 10u), (unsigned long)(speed_x10 % 10u),
                     (unsigned long)message.value_bits))
            {
                ++summary_lost;
            }
        }
        else
        {
            /* 厂商故障码先保留原值，不猜过流/过压含义。 */
            if (!DBG("[FP2 EVENT] feature=%u value_bits=%08lX", message.feature,
                     (unsigned long)message.value_bits))
            {
                ++summary_lost;
            }
        }
    }
    else if (!DBG("[FP2 RX] response=%X payload=%u", message.command, data[1]))
    {
        ++summary_lost;
    }
}

static void receive_process(uint32_t now_ms)
{
    BoardCanFrame frame;
    if ((assembly_length != 0u) &&
        ((uint32_t)(now_ms - last_fragment_ms) >= POWERBOARD_REASSEMBLY_MS))
    {
        assembly_length = expected_length = 0u;
        if (!WARN("[FP2 RX] incomplete message timeout, assembly discarded"))
        {
            ++summary_lost;
        }
    }
    for (uint32_t budget = 0u; budget < 4u; ++budget)
    {
        BoardCanResult result = BoardCan_Read(&frame);
        if (result != BOARD_CAN_OK)
        {
            if ((result != BOARD_CAN_EMPTY) && !WARN("[CAN RX] read result=%u", result))
            {
                ++summary_lost;
            }
            break;
        }
        (void)CanTrace_Push(CAN_TRACE_RX, 0u, &frame);
        if ((frame.id != POWERBOARD_RX_ID) || frame.is_extended || !frame.is_fd)
        {
            continue; /* 广告等照样打印，但不当作本节点的FP2响应。 */
        }
        if ((frame.length < 1u) || (frame.data[0] == 0u) ||
            (frame.data[0] > 63u) || (frame.data[0] > frame.length - 1u))
        {
            assembly_length = expected_length = 0u;
            if (!WARN("[iCANt RX] bad length prefix, discarded"))
            {
                ++summary_lost;
            }
            continue;
        }
        last_fragment_ms = now_ms;
        for (size_t i = 1u; i <= frame.data[0]; ++i)
        {
            assembly[assembly_length++] = frame.data[i];
            if (assembly_length == 2u)
            {
                expected_length = 2u + assembly[1];
            }
            if ((expected_length != 0u) && (assembly_length == expected_length))
            {
                decoded_process(assembly, assembly_length);
                assembly_length = expected_length = 0u;
            }
        }
    }
}

static void transfer_process(uint32_t now_ms)
{
    if (transfer == TRANSFER_WAIT_BUS)
    {
        BoardCanTxState state = BoardCan_GetTxState();
        if (state == BOARD_CAN_TX_PENDING)
        {
            if (!reported_bus_timeout &&
                ((uint32_t)(now_ms - bus_requested_ms) >= BOARD_CAN_TX_TIMEOUT_MS))
            {
                if (!ERR("[CAN TX_TIMEOUT] #%lu cancellation pending; buffer remains busy",
                         (unsigned long)active_sequence))
                {
                    ++summary_lost;
                }
                reported_bus_timeout = true;
            }
            return;
        }
        if (state == BOARD_CAN_TX_SENT)
        {
            (void)CanTrace_Push(CAN_TRACE_TX_SENT, active_sequence, &active_frame);
            transfer = TRANSFER_WAIT_REPLY;
            reply_started_ms = now_ms;
        }
        else
        {
            BoardCanStats status = BoardCan_GetStats();
            if (!ERR("[CAN TX_FAIL] #%lu RX=%u ACT=%lu CCCR=%03lX TEC=%lu REC=%lu LEC=%lu DLEC=%lu",
                     (unsigned long)active_sequence, status.is_rx_high,
                     (unsigned long)status.activity, (unsigned long)status.controller_control,
                     (unsigned long)status.tx_error_count, (unsigned long)status.rx_error_count,
                     (unsigned long)status.last_error, (unsigned long)status.last_data_error))
            {
                ++summary_lost;
            }
            finish_request(false);
        }
        if (is_preempting)
        {
            transfer = TRANSFER_IDLE;
        }
    }
    if ((transfer == TRANSFER_WAIT_REPLY) &&
        ((uint32_t)(now_ms - reply_started_ms) >= POWERBOARD_ACK_TIMEOUT_MS))
    {
        if (!ERR("[FP2 TIMEOUT] #%lu cmd=%02X no application ACK in %ums; no auto retry",
                 (unsigned long)active_sequence, active_command, POWERBOARD_ACK_TIMEOUT_MS))
        {
            ++summary_lost;
        }
        finish_request(false);
    }
    if (transfer != TRANSFER_IDLE)
    {
        return;
    }
    uint8_t message[8];
    bool sending_stop = has_stop_target;
    if (sending_stop && stop_has_failed &&
        ((uint32_t)(now_ms - stop_failed_ms) < POWERBOARD_ACK_TIMEOUT_MS))
    {
        return;
    }
    bool sending_speed = !sending_stop && has_speed_target && !is_faulted;
    if (sending_stop || sending_speed)
    {
        (void)Fp2_BuildSpeed(sending_stop ? 0u : requested_speed_x10, message);
    }
    else if (needs_subscription)
    {
        (void)Fp2_BuildSubscription(message);
    }
    else
    {
        return;
    }
    BoardCanFrame next = {0};
    next.id = POWERBOARD_TX_ID;
    next.length = 12u; /* FP2 8 + iCANt 1=9字节，合法FD DLC补到12，尾部为0。 */
    next.is_fd = next.has_brs = true;
    next.data[0] = sizeof(message);
    memcpy(next.data + 1u, message, sizeof(message));
    BoardCanResult result = BoardCan_TrySend(&next, now_ms);
    if (result != BOARD_CAN_OK)
    {
        return; /* BUSY/NOT_READY保留目标，包括STOP；不刷屏、不等待。 */
    }
    active_frame = next;
    active_command = message[0];
    active_is_stop = sending_stop;
    ++active_sequence;
    is_preempting = false;
    transfer = TRANSFER_WAIT_BUS;
    bus_requested_ms = now_ms;
    reported_bus_timeout = false;
    if (sending_stop)
    {
        has_stop_target = false;
    }
    else if (sending_speed)
    {
        has_speed_target = false;
    }
    else
    {
        needs_subscription = false;
    }
    (void)CanTrace_Push(CAN_TRACE_TX_REQUEST, active_sequence, &active_frame);
    if (sending_stop || sending_speed)
    {
        uint16_t speed = sending_stop ? 0u : requested_speed_x10;
        if (!DBG("[FP2 WRITE_REQ] #%lu feature=301 target=%u.%u KPH%s",
                 (unsigned long)active_sequence, speed / 10u, speed % 10u,
                 sending_stop ? " STOP" : ""))
        {
            ++summary_lost;
        }
    }
}

void Powerboard_Process(uint32_t now_ms)
{
    process_time_ms = now_ms;
    if (!is_service_ready)
    {
        CanTrace_Process(now_ms);
        return;
    }
    BoardCan_Process(now_ms);
    /* 先观察硬件完成，再解析本轮RX，允许紧邻发送完成到来的ACK。 */
    if ((transfer == TRANSFER_WAIT_BUS) &&
        (BoardCan_GetTxState() == BOARD_CAN_TX_SENT))
    {
        (void)CanTrace_Push(CAN_TRACE_TX_SENT, active_sequence, &active_frame);
        transfer = is_preempting ? TRANSFER_IDLE : TRANSFER_WAIT_REPLY;
        reply_started_ms = now_ms;
    }
    receive_process(now_ms);
    transfer_process(now_ms);
    if ((uint32_t)(now_ms - last_status_ms) >= 1000u)
    {
        BoardCanStats status = BoardCan_GetStats();
        last_status_ms = now_ms;
        if ((status.is_bus_off != reported_bus_off) || (status.rx_lost != observed_rx_lost) ||
            (status.tx_failed != observed_tx_failed) ||
            (summary_lost != reported_summary_lost))
        {
            if (WARN("[CAN STATUS] busoff=%u RX=%u ACT=%lu CCCR=%03lX TEC=%lu REC=%lu LEC=%lu DLEC=%lu RXlost=%lu loglost=%lu",
                     status.is_bus_off, status.is_rx_high, (unsigned long)status.activity,
                     (unsigned long)status.controller_control, (unsigned long)status.tx_error_count,
                     (unsigned long)status.rx_error_count, (unsigned long)status.last_error,
                     (unsigned long)status.last_data_error, (unsigned long)status.rx_lost,
                     (unsigned long)summary_lost))
            {
                reported_bus_off = status.is_bus_off;
                observed_rx_lost = status.rx_lost;
                observed_tx_failed = status.tx_failed;
                reported_summary_lost = summary_lost;
            }
        }
    }
    CanTrace_Process(now_ms);
}
