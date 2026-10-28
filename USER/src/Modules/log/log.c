#include "log.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "board_time.h"
#include "board_uart.h"

#define LOG_LINE_CAPACITY 192u
#define LOG_DAY_MS 86400000u

static LogLevel output_level = LOG_LEVEL_DBG;
static char line_buffer[LOG_LINE_CAPACITY];
static uint32_t last_time_ms;
static uint32_t day_time_ms;
static uint32_t uptime_days;

void Log_Process(uint32_t now_ms)
{
    uint32_t elapsed_ms = (uint32_t)(now_ms - last_time_ms);
    uint32_t remaining_ms = LOG_DAY_MS - day_time_ms;

    last_time_ms = now_ms;
    /* 常规主循环只有32位加减/比较；只有跨天才进行除法。 */
    if (elapsed_ms < remaining_ms)
    {
        day_time_ms += elapsed_ms;
        return;
    }
    elapsed_ms -= remaining_ms;
    uptime_days += 1u + elapsed_ms / LOG_DAY_MS;
    day_time_ms = elapsed_ms % LOG_DAY_MS;
}

void Log_SetLevel(LogLevel level)
{
    if ((level >= LOG_LEVEL_OFF) && (level <= LOG_LEVEL_DBG))
    {
        output_level = level;
    }
}

bool Log_Write(LogLevel level, const char *format, ...)
{
    static const char *const labels[] = {"OFF", "ERR", "WARN", "DBG"};
    va_list args;
    int prefix_length;
    int body_length;
    size_t length_bytes;
    size_t available;

    if ((level < LOG_LEVEL_ERR) || (level > LOG_LEVEL_DBG) || (format == NULL))
    {
        return false;
    }
    Log_Process(BoardTime_Millis());
    if (level > output_level)
    {
        return true;
    }
    /* 仅实际打印时转换时分秒，不使用浮点或64位除法。 */
    prefix_length = snprintf(line_buffer, sizeof(line_buffer),
                             "[%lud %02lu:%02lu:%02lu.%03lu][%s] ",
                             (unsigned long)uptime_days,
                             (unsigned long)(day_time_ms / 3600000u),
                             (unsigned long)((day_time_ms / 60000u) % 60u),
                             (unsigned long)((day_time_ms / 1000u) % 60u),
                             (unsigned long)(day_time_ms % 1000u), labels[level]);
    if ((prefix_length < 0) || ((size_t)prefix_length >= sizeof(line_buffer) - 2u))
    {
        return false;
    }
    available = sizeof(line_buffer) - (size_t)prefix_length - 2u;
    va_start(args, format);
    body_length = vsnprintf(&line_buffer[prefix_length], available, format, args);
    va_end(args);
    if ((body_length < 0) || ((size_t)body_length >= available))
    {
        return false;
    }
    length_bytes = (size_t)prefix_length + (size_t)body_length;
    /* 兼容调用方已有换行，统一为一个CRLF。 */
    while ((length_bytes > (size_t)prefix_length) &&
           ((line_buffer[length_bytes - 1u] == '\r') ||
            (line_buffer[length_bytes - 1u] == '\n')))
    {
        --length_bytes;
    }
    line_buffer[length_bytes++] = '\r';
    line_buffer[length_bytes++] = '\n';
    return BoardUart_TryWrite(line_buffer, length_bytes) == BOARD_UART_OK;
}
