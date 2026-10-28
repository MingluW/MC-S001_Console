#ifndef BOARD_TIME_H
#define BOARD_TIME_H

#include <stdint.h>

/**
 * 返回开机后的 uint32_t 毫秒计数，约 49.7 天回绕。
 * 先完成生成的 RCC_Configuration()，其配置 1ms SysTick。
 * 不阻塞，可在主循环或 ISR 读取；比较间隔时使用无符号时间差。
 */
uint32_t BoardTime_Millis(void);

#endif
