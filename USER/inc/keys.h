#ifndef KEYS_H
#define KEYS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    KEYS_SPEED_DEC = 0,
    KEYS_SPEED_INC,
    KEYS_START,
    KEYS_STOP,
    KEYS_INCLINE_DEC,
    KEYS_INCLINE_INC,
    KEYS_COUNT
} KeysId;

typedef enum
{
    KEYS_EVENT_PRESS = 0,
    KEYS_EVENT_REPEAT
} KeysEventType;

typedef struct
{
    KeysId id;
    KeysEventType type;
} KeysEvent;

/** 初始化GPIO和消抖。上电已按住的键须先释放；now_ms单位毫秒，仅主循环。 */
void Keys_Init(uint32_t now_ms);

/** 每10ms采样，20ms消抖；加减键500ms后每100ms连发，开始/停止不连发。 */
void Keys_Process(uint32_t now_ms);

/** 复制并取出一条事件；false表示空或NULL，不保留指针；仅主循环，不等待。
 * 停止事件独立锁存并优先取出，发生停止时清空旧普通事件。
 */
bool Keys_PopEvent(KeysEvent *event);

/** 返回消抖后的按下状态；无效id返回false。仅主循环，不等待。 */
bool Keys_IsPressed(KeysId id);

/** 8条事件满时丢新并累计计数；仅主循环，不等待。 */
uint32_t Keys_GetDroppedCount(void);

#endif
