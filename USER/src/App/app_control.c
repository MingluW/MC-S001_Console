#include "app_control.h"

#include "keys.h"
#include "log.h"

static AppState app_state;
static uint32_t reported_key_drops;
static uint32_t last_screen_step_ms;
static bool is_waiting_for_frame;

static void enter_screen(AppScreen screen)
{
    app_state.screen = screen;
    is_waiting_for_frame = true;
}

void AppControl_Init(void)
{
    app_state.demo_speed_x10 = 0u;
    app_state.demo_incline = 0u;
    app_state.countdown_digit = 3u;
    app_state.is_wait_visible = true;
    (void)AppControl_SetSpeedUnit(APP_DEFAULT_SPEED_UNIT);
    reported_key_drops = 0u;
    last_screen_step_ms = 0u;
    enter_screen(APP_SCREEN_BOOT_ALL);
}

bool AppControl_SetSpeedUnit(AppSpeedUnit unit)
{
    if ((unit != APP_SPEED_UNIT_KPH) && (unit != APP_SPEED_UNIT_MPH))
    {
        return false;
    }
    app_state.speed_unit = unit;
    return true;
}

/* 只推进动画/启动停留，手动选择的坡度和速度页面没有自动返回计时。 */
static void screen_time_process(uint32_t now_ms, bool is_frame_complete)
{
    if (is_waiting_for_frame)
    {
        if (!is_frame_complete)
        {
            return;
        }
        is_waiting_for_frame = false;
        last_screen_step_ms = now_ms;
    }
    uint32_t elapsed_ms = (uint32_t)(now_ms - last_screen_step_ms);

    switch (app_state.screen)
    {
        case APP_SCREEN_BOOT_ALL:
            if (elapsed_ms >= APP_BOOT_STEP_MS)
            {
                app_state.countdown_digit = 3u;
                enter_screen(APP_SCREEN_BOOT_COUNTDOWN);
            }
            break;
        case APP_SCREEN_BOOT_COUNTDOWN:
            if (elapsed_ms >= APP_BOOT_STEP_MS)
            {
                if (app_state.countdown_digit > 0u)
                {
                    --app_state.countdown_digit;
                    is_waiting_for_frame = true;
                }
                else
                {
                    app_state.is_wait_visible = true;
                    enter_screen(APP_SCREEN_WAIT_START);
                }
            }
            break;
        case APP_SCREEN_WAIT_START:
            if (elapsed_ms >= APP_WAIT_BLINK_MS)
            {
                app_state.is_wait_visible = !app_state.is_wait_visible;
                is_waiting_for_frame = true;
            }
            break;
        case APP_SCREEN_START_INCLINE:
            if (elapsed_ms >= APP_START_INCLINE_MS)
            {
                enter_screen(APP_SCREEN_SPEED);
            }
            break;
        default:
            break;
    }
}

static void adjustment_process(KeysId id)
{
    bool is_speed_key = (id == KEYS_SPEED_DEC) || (id == KEYS_SPEED_INC);
    bool is_incline_key = (id == KEYS_INCLINE_DEC) || (id == KEYS_INCLINE_INC);

    if (is_speed_key &&
        !(Keys_IsPressed(KEYS_SPEED_DEC) && Keys_IsPressed(KEYS_SPEED_INC)))
    {
        if ((id == KEYS_SPEED_INC) && (app_state.demo_speed_x10 < APP_DEMO_SPEED_MAX_X10))
        {
            app_state.demo_speed_x10 += APP_DEMO_SPEED_STEP_X10;
        }
        else if ((id == KEYS_SPEED_DEC) &&
                 (app_state.demo_speed_x10 >= APP_DEMO_SPEED_STEP_X10))
        {
            app_state.demo_speed_x10 -= APP_DEMO_SPEED_STEP_X10;
        }
        if (app_state.screen != APP_SCREEN_SPEED)
        {
            enter_screen(APP_SCREEN_SPEED);
        }
    }
    else if (is_incline_key &&
             !(Keys_IsPressed(KEYS_INCLINE_DEC) && Keys_IsPressed(KEYS_INCLINE_INC)))
    {
        if ((id == KEYS_INCLINE_INC) && (app_state.demo_incline < APP_DEMO_INCLINE_MAX))
        {
            ++app_state.demo_incline;
        }
        else if ((id == KEYS_INCLINE_DEC) && (app_state.demo_incline > 0u))
        {
            --app_state.demo_incline;
        }
        if (app_state.screen != APP_SCREEN_INCLINE)
        {
            enter_screen(APP_SCREEN_INCLINE);
        }
    }
}

void AppControl_Process(uint32_t now_ms, bool is_frame_complete)
{
    KeysEvent event;
    bool has_stopped = false;
    AppScreen previous_screen = app_state.screen;
    uint16_t previous_speed_x10 = app_state.demo_speed_x10;
    uint16_t previous_incline = app_state.demo_incline;
    uint32_t drops = Keys_GetDroppedCount();

    if ((drops != reported_key_drops) &&
        WARN("[KEYS] dropped events=%lu", (unsigned long)drops))
    {
        reported_key_drops = drops;
    }
    /* 停止事件由Keys优先提供；本轮之后的开始/加减不能撤销停止。 */
    for (uint32_t i = 0u; (i < 9u) && Keys_PopEvent(&event); ++i)
    {
        if (event.id == KEYS_STOP)
        {
            enter_screen(APP_SCREEN_STOPPED);
            has_stopped = true;
        }
        if (has_stopped || Keys_IsPressed(KEYS_STOP))
        {
            continue;
        }
        if ((event.id == KEYS_START) && (event.type == KEYS_EVENT_PRESS) &&
            ((app_state.screen == APP_SCREEN_WAIT_START) ||
             (app_state.screen == APP_SCREEN_STOPPED)))
        {
            app_state.demo_incline = 0u;
            app_state.demo_speed_x10 = 0u;
            enter_screen(APP_SCREEN_START_INCLINE);
        }
        else if ((app_state.screen == APP_SCREEN_SPEED) ||
                 (app_state.screen == APP_SCREEN_INCLINE))
        {
            adjustment_process(event.id);
        }
    }
    /* 进入新页这一轮还没有渲染，不能把上一页的完成信号当成新页已显示。 */
    if (!has_stopped && (app_state.screen == previous_screen))
    {
        screen_time_process(now_ms, is_frame_complete);
    }
    if ((previous_screen != app_state.screen) ||
        (previous_speed_x10 != app_state.demo_speed_x10) ||
        (previous_incline != app_state.demo_incline))
    {
        (void)DBG("[UI] page=%u speed=%u.%u incline=%u", (unsigned)app_state.screen,
                  app_state.demo_speed_x10 / 10u, app_state.demo_speed_x10 % 10u,
                  app_state.demo_incline);
    }
}

const AppState *AppControl_GetState(void)
{
    return &app_state;
}
