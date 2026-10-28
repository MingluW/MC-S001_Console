#ifndef LCD_H
#define LCD_H

#include <stdbool.h>
#include <stdint.h>

/** 初始化GPIO并启动屏幕初始化时序。now_ms为当前毫秒，仅主循环，不等待。 */
void Lcd_Init(uint32_t now_ms);

/** 每轮推进一个初始化步骤或最多一个9位字，初始化等待不阻塞。仅主循环。 */
void Lcd_Process(uint32_t now_ms);

/** 修改目标段码为全亮/全灭，允许初始化期间调用；不立即传输，仅主循环。 */
void Lcd_SetAll(bool is_on);

/** 初始化序列是否已发完，不代表有硬件应答或实测点亮。仅主循环，不阻塞。 */
bool Lcd_IsReady(void);

/** 是否仍在初始化或提交显示；出错时返回false，通过HasError区分。仅主循环。 */
bool Lcd_IsBusy(void);

/** 是否发生本地接口发送失败。仅主循环，不阻塞。 */
bool Lcd_HasError(void);

#endif
