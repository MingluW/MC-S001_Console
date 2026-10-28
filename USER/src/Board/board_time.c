#include "board_time.h"

/*
 * 适配 N32Cube 现有时基：cfg.c 定义，SysTick_Handler 每 1ms 自增。
 * 不再新建另一份计数器，保留生成代码的 SysTick_Delayms 兼容性。
 * Cortex-M4 对齐 uint32_t 读取为单次访问；本模块只读，不重置计数。
 * 重新生成后需核对符号和 1ms 配置，不能将此依赖扩散到 App。
 */
extern volatile uint32_t mwTick;

uint32_t BoardTime_Millis(void)
{
    return mwTick;
}
