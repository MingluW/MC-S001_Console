#ifndef BOARD_SAFETY_H
#define BOARD_SAFETY_H

#include <stdbool.h>

/** 配置PA9检测及PB6硬件禁止输出，初始禁止；仅主循环，非阻塞。 */
void BoardSafety_Init(void);
/** 原始DMK高电平表示断开；仅主循环，非阻塞。 */
bool BoardSafety_IsDisconnected(void);
/** true使PB6输出高禁止SAFEKEY_R；false解除软件禁止，物理锁仍参与硬件通路。
 * 不代表电机已停或可运行；仅主循环，非阻塞。 */
void BoardSafety_SetInhibited(bool is_inhibited);

#endif
