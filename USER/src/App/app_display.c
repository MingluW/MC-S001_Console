#include "app_display.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "lcd_segments.h"

#define APP_DISPLAY_INTERVAL_MS 50u

static bool has_submitted_frame;
static uint8_t submitted_frame[LCD_FRAME_BYTES];
static AppScreen submitted_screen;
static uint32_t last_submit_ms;
static uint8_t submitted_error;
static bool submitted_safety;

void AppDisplay_Init(uint32_t now_ms)
{
    has_submitted_frame = false;
    last_submit_ms = now_ms;
}

static bool build_frame(const AppState *state, uint8_t visible_error,
                        uint8_t frame[LCD_FRAME_BYTES])
{
    if (state->is_safety_active)
    {
        return LcdSegments_BuildSafety(frame);
    }
    if (visible_error != 0u) { return LcdSegments_BuildError(visible_error, frame); }
    switch (state->screen)
    {
        case APP_SCREEN_BOOT_ALL:
            memset(frame, 0xFF, LCD_FRAME_BYTES);
            return true;
        case APP_SCREEN_BOOT_COUNTDOWN:
            if (state->countdown_digit > 3u)
            {
                return false;
            }
            return LcdSegments_BuildDigits(state->countdown_digit * 1111u, true, frame);
        case APP_SCREEN_WAIT_START:
            if (!state->is_wait_visible)
            {
                memset(frame, 0, LCD_FRAME_BYTES);
                return true;
            }
            return LcdSegments_BuildDigits(8888u, true, frame);
        case APP_SCREEN_START_INCLINE:
        case APP_SCREEN_INCLINE:
            return LcdSegments_BuildIncline(state->demo_incline, frame);
        case APP_SCREEN_STOPPED:
            return LcdSegments_BuildStopped(frame);
        case APP_SCREEN_SPEED:
            if (state->speed_unit == APP_SPEED_UNIT_KPH)
            {
                return LcdSegments_BuildSpeed(state->demo_speed_x10, LCD_SPEED_UNIT_KPH, frame);
            }
            if (state->speed_unit == APP_SPEED_UNIT_MPH)
            {
                return LcdSegments_BuildSpeed(state->demo_speed_x10, LCD_SPEED_UNIT_MPH, frame);
            }
            return false;
        default:
            return false;
    }
}

void AppDisplay_Process(uint32_t now_ms, const AppState *state)
{
    uint8_t frame[LCD_FRAME_BYTES];

    if (state == NULL) { return; }
    uint8_t visible_error = state->error_code;
    if ((visible_error == 1u) && state->is_speed_preview &&
        (state->screen == APP_SCREEN_SPEED))
    {
        visible_error = 0u;
    }
    if (!build_frame(state, visible_error, frame))
    {
        return;
    }
    if (has_submitted_frame)
    {
        if (memcmp(frame, submitted_frame, sizeof(frame)) == 0)
        {
            return;
        }
        /* 切页（尤其STOP）立即提交；同一页的参数变化最多每50ms一次。 */
        if (!state->is_safety_active && !submitted_safety &&
            !state->is_speed_preview && (visible_error == submitted_error) &&
            (state->screen == submitted_screen) &&
            ((uint32_t)(now_ms - last_submit_ms) < APP_DISPLAY_INTERVAL_MS))
        {
            return;
        }
    }
    if (Lcd_SetFrame(frame))
    {
        memcpy(submitted_frame, frame, sizeof(submitted_frame));
        submitted_screen = state->screen;
        submitted_error = visible_error;
        submitted_safety = state->is_safety_active;
        has_submitted_frame = true;
        last_submit_ms = now_ms;
    }
}
