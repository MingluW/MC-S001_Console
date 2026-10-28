#include "keys.h"

#include <stddef.h>
#include <string.h>

#include "board_keys.h"

#define KEYS_SCAN_MS 10u
#define KEYS_DEBOUNCE_MS 20u
#define KEYS_REPEAT_DELAY_MS 500u
#define KEYS_REPEAT_INTERVAL_MS 100u
#define KEYS_QUEUE_CAPACITY 8u

typedef struct
{
    bool is_candidate_pressed;
    bool is_pressed;
    bool is_armed;
    bool is_repeating;
    uint32_t candidate_since_ms;
    uint32_t last_event_ms;
} KeyState;

static const uint32_t input_masks[KEYS_COUNT] = {
    BOARD_KEYS_SPEED_DEC, BOARD_KEYS_SPEED_INC, BOARD_KEYS_START, BOARD_KEYS_STOP,
    BOARD_KEYS_INCLINE_DEC, BOARD_KEYS_INCLINE_INC
};
static KeyState key_states[KEYS_COUNT];
static KeysEvent events[KEYS_QUEUE_CAPACITY];
static uint32_t last_scan_ms;
static uint32_t dropped_count;
static uint8_t read_index;
static uint8_t event_count;
static bool has_stop_event;

static void push_event(KeysId id, KeysEventType type)
{
    /* 停止不与普通连发争队列容量，并撤销此前积压的普通输入。 */
    if (id == KEYS_STOP)
    {
        has_stop_event = true;
        read_index = event_count = 0u;
        return;
    }
    if (event_count == KEYS_QUEUE_CAPACITY)
    {
        ++dropped_count;
        return;
    }
    uint8_t index = (read_index + event_count) % KEYS_QUEUE_CAPACITY;
    events[index].id = id;
    events[index].type = type;
    ++event_count;
}

void Keys_Init(uint32_t now_ms)
{
    BoardKeys_Init();
    uint32_t raw = BoardKeys_Read();

    memset(key_states, 0, sizeof(key_states));
    read_index = event_count = 0u;
    dropped_count = 0u;
    has_stop_event = false;
    last_scan_ms = now_ms;
    for (uint32_t i = 0u; i < KEYS_COUNT; ++i)
    {
        KeyState *key = &key_states[i];
        key->is_pressed = (raw & input_masks[i]) != 0u;
        key->is_candidate_pressed = key->is_pressed;
        key->is_armed = !key->is_pressed;
        key->candidate_since_ms = now_ms;
        key->last_event_ms = now_ms;
    }
}

/* 只确认按下/松开：候选状态持续稳定20ms后才接受。 */
static void debounce_process(KeysId id, bool is_raw_pressed, uint32_t now_ms)
{
    KeyState *key = &key_states[id];

    if (is_raw_pressed != key->is_candidate_pressed)
    {
        key->is_candidate_pressed = is_raw_pressed;
        key->candidate_since_ms = now_ms;
    }
    if ((key->is_pressed != key->is_candidate_pressed) &&
        ((uint32_t)(now_ms - key->candidate_since_ms) >= KEYS_DEBOUNCE_MS))
    {
        key->is_pressed = key->is_candidate_pressed;
        key->is_repeating = false;
        key->last_event_ms = now_ms;
        if (!key->is_pressed)
        {
            key->is_armed = true;
        }
        else if (key->is_armed)
        {
            push_event(id, KEYS_EVENT_PRESS);
        }
    }
}

/* 只处理长按连发：首次等待500ms，之后每100ms一条，不补发错过的周期。 */
static void repeat_process(KeysId id, uint32_t now_ms)
{
    KeyState *key = &key_states[id];

    if ((id == KEYS_START) || (id == KEYS_STOP))
    {
        return;
    }

    /* 一旦采样到释放就暂停连发，不等消抖结束，避免松手时多走一步。 */
    if (key->is_pressed && key->is_candidate_pressed && key->is_armed &&
        ((uint32_t)(now_ms - key->last_event_ms) >=
         (key->is_repeating ? KEYS_REPEAT_INTERVAL_MS : KEYS_REPEAT_DELAY_MS)))
    {
        push_event(id, KEYS_EVENT_REPEAT);
        key->is_repeating = true;
        key->last_event_ms = now_ms;
    }
}

void Keys_Process(uint32_t now_ms)
{
    if ((uint32_t)(now_ms - last_scan_ms) < KEYS_SCAN_MS)
    {
        return;
    }
    last_scan_ms = now_ms;
    uint32_t raw = BoardKeys_Read();

    for (uint32_t i = 0u; i < KEYS_COUNT; ++i)
    {
        bool is_raw_pressed = (raw & input_masks[i]) != 0u;

        /* 先确认状态，再根据确认后的状态判断连发。 */
        debounce_process((KeysId)i, is_raw_pressed, now_ms);
        repeat_process((KeysId)i, now_ms);
    }
}

bool Keys_PopEvent(KeysEvent *event)
{
    if (event == NULL)
    {
        return false;
    }
    if (has_stop_event)
    {
        has_stop_event = false;
        event->id = KEYS_STOP;
        event->type = KEYS_EVENT_PRESS;
        return true;
    }
    if (event_count == 0u)
    {
        return false;
    }
    *event = events[read_index];
    read_index = (read_index + 1u) % KEYS_QUEUE_CAPACITY;
    --event_count;
    return true;
}

bool Keys_IsPressed(KeysId id)
{
    return ((uint32_t)id < KEYS_COUNT) && key_states[id].is_pressed;
}

uint32_t Keys_GetDroppedCount(void)
{
    return dropped_count;
}
