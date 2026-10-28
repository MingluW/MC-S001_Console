#ifndef APP_H
#define APP_H

/** 初始化应用状态并点亮蓝灯；先完成生成的硬件初始化。仅主循环调用，不阻塞。 */
void App_Init(void);

/** 执行一轮应用任务，到期才切换蓝灯。仅主循环调用，不阻塞。 */
void App_Process(void);

#endif
