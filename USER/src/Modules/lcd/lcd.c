#include "lcd.h"

#include <stddef.h>
#include <string.h>

#include "board_lcd_port.h"

typedef enum
{
    LCD_STATE_OFF = 0,
    LCD_STATE_RESET_HIGH,
    LCD_STATE_RESET_LOW,
    LCD_STATE_RESET_RELEASE,
    LCD_STATE_INITIALIZING,
    LCD_STATE_READY,
    LCD_STATE_ERROR
} LcdState;

typedef struct
{
    uint16_t word;
    uint16_t delay_after_ms;
} LcdInitStep;

/*
 * 来自已运行的Cypress LCD.c，bit8=1代表write_data。
 * 保留0x1D2（旧代码write_data(0xD2)），首版不改写旧初始化序列。
 */
static const LcdInitStep init_steps[] = {
    {0x011u, 0u},  /* sleep out */
    {0x0B7u, 0u}, {0x100u, 0u},
    {0x0C0u, 0u}, {0x119u, 0u}, {0x100u, 0u},
    {0x0C3u, 0u}, {0x101u, 0u},
    {0x02Au, 0u}, {0x100u, 0u}, {0x103u, 0u},
    {0x02Bu, 0u}, {0x100u, 0u}, {0x103u, 0u},
    {0x0B2u, 0u}, {0x11Au, 0u},
    {0x0D2u, 0u}, {0x106u, 20u},
    {0x1D2u, 0u}, {0x104u, 20u},
    {0x0D2u, 0u}, {0x100u, 20u},
    {0x020u, 0u}, {0x00Fu, 0u}, {0x10Fu, 0u},
    {0x022u, 0u}, {0x029u, 0u}
};

/* 与旧show_dot一致：设置SEG范围，发Memory Write，再发16字节。 */
static const uint16_t frame_header[] = {0x02Au, 0x100u, 0x103u, 0x02Cu};

static LcdState state;
static uint32_t last_step_ms;
static uint32_t wait_ms;
static size_t init_index;
static size_t refresh_index;
static uint8_t desired_frame[LCD_FRAME_BYTES];
static uint8_t sending_frame[LCD_FRAME_BYTES];
static bool is_dirty;
static bool is_refreshing;

static bool send_word(uint16_t word)
{
    if (!BoardLcdPort_Write9(word))
    {
        state = LCD_STATE_ERROR;
        BoardLcdPort_SetBacklight(false);
        return false;
    }
    return true;
}

void Lcd_Init(uint32_t now_ms)
{
    BoardLcdPort_Init();
    state = LCD_STATE_RESET_HIGH;
    last_step_ms = now_ms;
    wait_ms = 100u;
    init_index = refresh_index = 0u;
    memset(desired_frame, 0, sizeof(desired_frame));
    memset(sending_frame, 0, sizeof(sending_frame));
    is_dirty = true;
    is_refreshing = false;
}

bool Lcd_SetFrame(const uint8_t frame[LCD_FRAME_BYTES])
{
    if ((frame == NULL) || (state == LCD_STATE_OFF) || (state == LCD_STATE_ERROR))
    {
        return false;
    }
    if (memcmp(desired_frame, frame, sizeof(desired_frame)) != 0)
    {
        memcpy(desired_frame, frame, sizeof(desired_frame));
        is_dirty = true;
    }
    return true;
}

void Lcd_SetAll(bool is_on)
{
    uint8_t value = is_on ? 0xFFu : 0x00u;

    for (size_t i = 0u; i < LCD_FRAME_BYTES; ++i)
    {
        if (desired_frame[i] != value)
        {
            desired_frame[i] = value;
            is_dirty = true;
        }
    }
}

static void refresh_process(void)
{
    uint16_t word;

    if (!is_refreshing)
    {
        if (!is_dirty)
        {
            return;
        }
        /* 本次传输使用快照，中途变更目标内容留到下一帧。 */
        memcpy(sending_frame, desired_frame, sizeof(sending_frame));
        is_dirty = false;
        is_refreshing = true;
        refresh_index = 0u;
    }
    if (refresh_index < sizeof(frame_header) / sizeof(frame_header[0]))
    {
        word = frame_header[refresh_index];
    }
    else
    {
        word = 0x100u | sending_frame[refresh_index - 4u];
    }
    if (!send_word(word))
    {
        return;
    }
    ++refresh_index;
    if (refresh_index == 4u + LCD_FRAME_BYTES)
    {
        is_refreshing = false;
    }
}

void Lcd_Process(uint32_t now_ms)
{
    if ((state == LCD_STATE_OFF) || (state == LCD_STATE_ERROR))
    {
        return;
    }
    if (state == LCD_STATE_READY)
    {
        refresh_process();
        return;
    }
    if ((uint32_t)(now_ms - last_step_ms) < wait_ms)
    {
        return;
    }

    switch (state)
    {
        case LCD_STATE_RESET_HIGH:
            BoardLcdPort_SetReset(false);
            state = LCD_STATE_RESET_LOW;
            wait_ms = 200u;
            break;
        case LCD_STATE_RESET_LOW:
            BoardLcdPort_SetReset(true);
            state = LCD_STATE_RESET_RELEASE;
            wait_ms = 200u;
            break;
        case LCD_STATE_RESET_RELEASE:
            state = LCD_STATE_INITIALIZING;
            wait_ms = 0u;
            break;
        case LCD_STATE_INITIALIZING:
            if (init_index == sizeof(init_steps) / sizeof(init_steps[0]))
            {
                state = LCD_STATE_READY;
                break;
            }
            if (!send_word(init_steps[init_index].word))
            {
                return;
            }
            wait_ms = init_steps[init_index].delay_after_ms;
            ++init_index;
            break;
        default:
            break;
    }
    last_step_ms = now_ms;
}

bool Lcd_IsReady(void)
{
    return state == LCD_STATE_READY;
}

bool Lcd_IsBusy(void)
{
    return ((state != LCD_STATE_OFF) && (state != LCD_STATE_ERROR)) &&
           ((state != LCD_STATE_READY) || is_dirty || is_refreshing);
}

bool Lcd_HasError(void)
{
    return state == LCD_STATE_ERROR;
}
