#ifndef BOARD_KEYS_H
#define BOARD_KEYS_H

#include <stdint.h>

#define BOARD_KEYS_SPEED_DEC (1u << 0)
#define BOARD_KEYS_SPEED_INC (1u << 1)
#define BOARD_KEYS_START (1u << 2)
#define BOARD_KEYS_STOP (1u << 3)
#define BOARD_KEYS_INCLINE_DEC (1u << 4)
#define BOARD_KEYS_INCLINE_INC (1u << 5)

/** 初始化六路面板键上拉输入；仅主循环，不等待。 */
void BoardKeys_Init(void);

/** 返回原始按下位图，1表示按下，尚未消抖；仅主循环，不等待。 */
uint32_t BoardKeys_Read(void);

#endif
