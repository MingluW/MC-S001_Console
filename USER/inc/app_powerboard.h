#ifndef APP_POWERBOARD_H
#define APP_POWERBOARD_H

#include "app_control.h"

/** 初始化下控服务及状态比较缓存；仅主循环，初始化CAN有SDK有界等待。 */
void AppPowerboard_Init(void);

/** 将开始、停止及速度目标变化交给下控服务，再推进收发和日志。
 * 不保留state指针；NULL仍服务通信。坡度/心跳暂不发送，仅主循环，不等待。
 */
void AppPowerboard_Process(uint32_t now_ms, const AppState *state);

#endif
