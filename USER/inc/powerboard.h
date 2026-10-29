#ifndef POWERBOARD_H
#define POWERBOARD_H

#include <stdbool.h>
#include <stdint.h>

#define POWERBOARD_CONTROLLER_INDEX 1u
#define POWERBOARD_TX_ID (0x200u + POWERBOARD_CONTROLLER_INDEX)
#define POWERBOARD_RX_ID (0x240u + POWERBOARD_CONTROLLER_INDEX)
#define POWERBOARD_ACK_TIMEOUT_MS 500u /* 首版联调超时配置，不是协议规定。 */

/** 初始化CAN及服务，默认节点1，失败返回false；不自动发送运行目标。仅主循环。 */
bool Powerboard_Init(void);

/** 明确开始请求，speed_x10为KPH的10倍，0~300；false表示无效/未就绪。
 * 本版直接写301，不构造未定义的使能命令；成功只代表已保存待发目标。
 * 复制数值，非阻塞，仅主循环；显式开始可解除上一次应用应答超时锁存。
 */
bool Powerboard_RequestStart(uint16_t speed_x10);

/** 保存最新速度，长按产生的尚未提交目标合并；失败不发送。仅主循环、不等待。 */
bool Powerboard_RequestSpeed(uint16_t speed_x10);

/** 优先保存速度0并取消未完成旧目标、撤销待发非零值；不等待。
 * false表示服务未初始化；成功不等于下控已停机。仅主循环。
 */
bool Powerboard_RequestStop(void);

/** 每轮收最多4帧、提交最多1个请求；打印TX请求/TX完成/RX/ACK/Error/超时。
 * 一次只等一个FP2应答，不自动重试运行目标；不发心跳，仅主循环，不等待。
 */
void Powerboard_Process(uint32_t now_ms);

#endif
