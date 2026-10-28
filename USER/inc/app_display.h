#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include "app_control.h"

/** 复位显示缓存，首次Process立即提交；now_ms单位毫秒，仅主循环。 */
void AppDisplay_Init(uint32_t now_ms);

/** 根据只读状态构建完整帧，同页参数变化最多每50ms提交一次，切页立即提交；不保留state指针。
 * LCD忙时由LCD保存最新目标，不破坏发送中的快照；失败保留待提交状态。
 * NULL不操作，不等待，仅主循环使用。
 */
void AppDisplay_Process(uint32_t now_ms, const AppState *state);

#endif
