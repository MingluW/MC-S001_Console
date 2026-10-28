#include "app.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "board_led.h"
#include "board_stdio.h"
#include "board_time.h"
#include "board_uart.h"
#include "log.h"
#include "lcd.h"
#include "keys.h"
#include "app_control.h"
#include "app_display.h"

#define APP_LED_INTERVAL_MS 1500u
#define APP_LOG_INTERVAL_MS 5000u

static bool is_led_on;
static uint32_t last_led_change_ms;
static uint32_t last_log_ms;
static uint32_t reported_rx_error_count;
static bool has_echo_byte;
static uint8_t echo_byte;
static bool is_lcd_result_reported;

void App_Init(void)
{
    BoardUart_Init();
    BoardStdio_Init();
    Log_SetLevel(LOG_LEVEL_DBG);
    is_led_on = true;
    BoardLed_Set(is_led_on);
    last_led_change_ms = BoardTime_Millis();
    last_log_ms = last_led_change_ms;
    reported_rx_error_count = 0u;
    has_echo_byte = false;
    is_lcd_result_reported = false;
    Lcd_Init(last_led_change_ms);
    Keys_Init(last_led_change_ms);
    AppControl_Init();
    AppDisplay_Init(last_led_change_ms);
    AppDisplay_Process(last_led_change_ms, AppControl_GetState());

    /* 一次性日志示例，不代表真实故障；非关键示例允许在队列满时丢弃。 */
    (void)printf("MC-S001 Console: USART2 %lu 8N1\r\n",
                 (unsigned long)BOARD_UART_BAUD_RATE);
    (void)ERR("[SELFTEST] error log example (not a real fault)");
    (void)WARN("[SELFTEST] warning log example");
    (void)DBG("[APP] boot, LED interval=%ums", APP_LED_INTERVAL_MS);
    (void)DBG("[UI] boot animation, START/STOP and speed/incline demo; no motor commands");
}

static void led_process(uint32_t now_ms)
{
    /* 无符号时间差允许毫秒计数回绕；未到期立即返回。 */
    if ((uint32_t)(now_ms - last_led_change_ms) < APP_LED_INTERVAL_MS)
    {
        return;
    }

    /* 若调试暂停等导致错过周期，只切换一次，不连续补闪。 */
    last_led_change_ms = now_ms;
    is_led_on = !is_led_on;
    BoardLed_Set(is_led_on);
}

static void uart_echo_process(void)
{
    BoardUartStatus status;

    /* 每轮最多32字节，满时保留当前字节，下轮重试。仅作为调试口验证。 */
    for (uint32_t i = 0u; i < 32u; ++i)
    {
        if (!has_echo_byte)
        {
            status = BoardUart_ReadByte(&echo_byte);
            if (status != BOARD_UART_OK)
            {
                return;
            }
            has_echo_byte = true;
        }
        if (BoardUart_TryWrite(&echo_byte, 1u) != BOARD_UART_OK)
        {
            return;
        }
        has_echo_byte = false;
    }
}

static void log_process(uint32_t now_ms)
{
    BoardUartStats stats;

    if ((uint32_t)(now_ms - last_log_ms) < APP_LOG_INTERVAL_MS)
    {
        return;
    }
    last_log_ms = now_ms;
    stats = BoardUart_GetStats();
    if (stats.rx_error_count != reported_rx_error_count)
    {
        if (WARN("[UART] RX errors/overflow=%lu", (unsigned long)stats.rx_error_count))
        {
            reported_rx_error_count = stats.rx_error_count;
        }
    }
    /* 周期诊断允许丢弃，满时不等待、不反复打印。 */
    (void)DBG("[APP] alive, TX rejected=%lu",
              (unsigned long)stats.tx_rejected_count);
}

static void lcd_process(uint32_t now_ms)
{
    Lcd_Process(now_ms);
    if (is_lcd_result_reported)
    {
        return;
    }
    if (Lcd_HasError())
    {
        is_lcd_result_reported = ERR("[LCD] local transfer failed");
    }
    else if (Lcd_IsReady() && !Lcd_IsBusy())
    {
        is_lcd_result_reported = DBG("[LCD] init and first UI frame sent; check screen");
    }
}

void App_Process(void)
{
    uint32_t now_ms = BoardTime_Millis();

    /* 每个任务独立判断时间并快速返回，不影响后续任务。 */
    Log_Process(now_ms);
    BoardUart_Process();
    led_process(now_ms);
    Keys_Process(now_ms);
    AppControl_Process(now_ms, Lcd_IsReady() && !Lcd_IsBusy());
    AppDisplay_Process(now_ms, AppControl_GetState());
    lcd_process(now_ms);
//    uart_echo_process();
//    log_process(now_ms);
}
