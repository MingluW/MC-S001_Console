#ifndef BOARD_LCD_PORT_H
#define BOARD_LCD_PORT_H

#include <stdbool.h>
#include <stdint.h>

/** 配置LCD GPIO输出，CS/CLK/RESET高，背光打开。先完成生成的初始化，仅主循环。 */
void BoardLcdPort_Init(void);

/** true释放复位（高），false保持复位（低）。仅主循环，立即返回。 */
void BoardLcdPort_SetReset(bool is_released);

/** true打开PB5控制的背光，false关闭。仅主循环，立即返回。 */
void BoardLcdPort_SetBacklight(bool is_on);

/**
 * 同步发送一个9位字：bit8为命令/数据标志，其余8位为载荷，MSB先发。
 * Mode3，每字独立CS；输入必须<=0x1FF。返回true只代表波形已输出，不代表屏幕应答。
 * 仅主循环，固定9位和有界边沿延时，无硬件等待循环，不关闭中断。
 * GPIO速度需实测，当前不承诺标称波特率；中断可能拉长单个时钟周期。
 */
bool BoardLcdPort_Write9(uint16_t word);

#endif
