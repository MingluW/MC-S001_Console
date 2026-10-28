#ifndef BOARD_LED_H
#define BOARD_LED_H

#include <stdbool.h>

/**
 * 设置 LED501 蓝灯：true 亮，false 灭，调用方不关心有效电平。
 * 先调用生成的 GPIO_Configuration()。仅主循环调用，不阻塞，无返回值。
 */
void BoardLed_Set(bool is_on);

#endif
