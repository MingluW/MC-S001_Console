#ifndef LOG_H
#define LOG_H

#include <stdbool.h>
#include <stdint.h>

/**
 * 更新运行天数/日内时间，主循环每轮调用，非阻塞；仅32位整数操作。
 * 两次调用间隔须小于uint32毫秒的完整回绕周期（约49.7天）。
 * 即使关闭日志也继续调用，以便跨回绕保持时间连续。仅主循环。
 */
void Log_Process(uint32_t now_ms);

typedef enum
{
    LOG_LEVEL_OFF = 0,
    LOG_LEVEL_ERR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_DBG
} LogLevel;

/** 设置最高输出级别，默认DBG；OFF关闭日志，不影响原始printf。仅主循环。 */
void Log_SetLevel(LogLevel level);

/**
 * printf风格格式化，自动附加[天数d HH:MM:SS.mmm]、级别和CRLF。
 * 仅主循环，不等待串口。这是开机运行时长，不是日期/实时时钟。
 * 返回true表示入队成功或被级别过滤；false表示格式化失败/超长/队列满。
 * 单条含前缀和CRLF最多191字节；超长整条拒绝，数据入队后内部缓冲可复用。
 * 不支持ISR调用；整数/字符串为默认支持范围，未启用浮点格式化。
 */
bool Log_Write(LogLevel level, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

#define ERR(...) Log_Write(LOG_LEVEL_ERR, __VA_ARGS__)
#define WARN(...) Log_Write(LOG_LEVEL_WARN, __VA_ARGS__)
#define DBG(...) Log_Write(LOG_LEVEL_DBG, __VA_ARGS__)

#endif
