#include "app_control.h"

#include "keys.h"
#include "board_keys.h"
#include "log.h"

static AppState app_state;
static uint32_t reported_key_drops;
static uint32_t last_screen_step_ms;
static bool is_waiting_for_frame;
static uint8_t latched_error;
static bool is_preview_waiting_for_frame;
static uint32_t preview_started_ms;
static bool is_reconnect_pending;
static uint32_t reconnect_started_ms;
static bool is_safety_keys_blocked;
static bool is_keys_release_pending;
static uint32_t keys_release_started_ms;

static void enter_screen(AppScreen screen)
{
    app_state.screen = screen;
    is_waiting_for_frame = true;
}

void AppControl_Init(void)
{
    app_state.is_safety_active = false;
    is_reconnect_pending = false;
    is_safety_keys_blocked = false;
    is_keys_release_pending = false;
    latched_error = 0u;
    app_state.is_speed_preview = false;
    is_preview_waiting_for_frame = false;
    preview_started_ms = 0u;
    app_state.error_code = 0u;
    app_state.demo_speed_x10 = 0u;
    app_state.demo_incline = 0u;
    app_state.countdown_digit = 3u;
    app_state.is_wait_visible = true;
    (void)AppControl_SetSpeedUnit(APP_DEFAULT_SPEED_UNIT);
    reported_key_drops = 0u;
    last_screen_step_ms = 0u;
    enter_screen(APP_SCREEN_BOOT_ALL);
}

void AppControl_UpdateSafety(uint32_t now_ms, bool is_disconnected)
{
    if (is_disconnected)
    {
        is_reconnect_pending = false;
        if (!app_state.is_safety_active)
        {
            app_state.is_safety_active = true;
            app_state.demo_speed_x10 = 0u;
            app_state.is_speed_preview = false;
            enter_screen(APP_SCREEN_STOPPED);
            is_safety_keys_blocked = true;
            is_keys_release_pending = false;
            (void)WARN("[SAFETY] disconnected; inhibit run, request STOP");
        }
    }
    else if (app_state.is_safety_active)
    {
        if (!is_reconnect_pending)
        {
            is_reconnect_pending = true;
            reconnect_started_ms = now_ms;
        }
        if ((uint32_t)(now_ms - reconnect_started_ms) >= APP_SAFETY_RECONNECT_MS)
        {
            app_state.is_safety_active = false;
            is_reconnect_pending = false;
            app_state.demo_speed_x10 = 0u;
            app_state.is_speed_preview = false;
            enter_screen(APP_SCREEN_STOPPED);
            (void)DBG("[SAFETY] reconnected; remain stopped, release keys then START");
        }
    }
}

static void safety_keys_process(uint32_t now_ms)
{
    if (app_state.is_safety_active || !is_safety_keys_blocked) { return; }
    bool has_pressed_key = BoardKeys_Read() != 0u;
    for (uint32_t i = 0u; i < KEYS_COUNT; ++i)
    {
        has_pressed_key = has_pressed_key || Keys_IsPressed((KeysId)i);
    }
    if (has_pressed_key)
    {
        is_keys_release_pending = false;
        return;
    }
    if (!is_keys_release_pending)
    {
        is_keys_release_pending = true;
        keys_release_started_ms = now_ms;
    }
    if ((uint32_t)(now_ms - keys_release_started_ms) >= APP_SAFETY_KEYS_RELEASE_MS)
    {
        is_safety_keys_blocked = false;
        is_keys_release_pending = false;
    }
}

void AppControl_SetErrors(uint8_t communication_error, uint8_t controller_error)
{
    if ((latched_error == 0u) && (controller_error >= 2u) && (controller_error <= 15u))
    {
        latched_error = controller_error;
        app_state.demo_speed_x10 = 0u;
        enter_screen(APP_SCREEN_STOPPED);
    }
    app_state.error_code = latched_error != 0u ? latched_error : communication_error;
    if (app_state.error_code != 1u) { app_state.is_speed_preview = false; }
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

static bool adjustment_process(KeysId id)
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
        return true;
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
    return false;
}

static void speed_preview_process(uint32_t now_ms, bool has_speed_input,
                                  bool is_frame_complete)
{
    if ((app_state.error_code != 1u) || (app_state.screen != APP_SCREEN_SPEED))
    {
        app_state.is_speed_preview = false;
        return;
    }
    if (has_speed_input)
    {
        app_state.is_speed_preview = true;
        is_preview_waiting_for_frame = true;
        return; /* 本轮速度帧尚未渲染，不能用上一帧的完成信号计时。 */
    }
    if (!app_state.is_speed_preview) { return; }
    if (is_preview_waiting_for_frame)
    {
        if (!is_frame_complete) { return; }
        is_preview_waiting_for_frame = false;
        preview_started_ms = now_ms;
    }
    if ((uint32_t)(now_ms - preview_started_ms) >= APP_COMM_SPEED_PREVIEW_MS)
    {
        app_state.is_speed_preview = false;
    }
}

void AppControl_Process(uint32_t now_ms, bool is_frame_complete)
{
    KeysEvent event;
    bool has_stopped = false;
    bool has_speed_input = false;
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
        if (app_state.is_safety_active || is_safety_keys_blocked) { continue; }
        if (event.id == KEYS_STOP)
        {
            enter_screen(APP_SCREEN_STOPPED);
            has_stopped = true;
        }
        if ((latched_error != 0u) || has_stopped || Keys_IsPressed(KEYS_STOP))
        {
            continue;
        }
        if ((event.id == KEYS_START) && (event.type == KEYS_EVENT_PRESS) &&
            ((app_state.screen == APP_SCREEN_WAIT_START) ||
             (app_state.screen == APP_SCREEN_STOPPED)))
        {
            app_state.demo_incline = 0u;
            app_state.demo_speed_x10 = APP_START_SPEED_X10;
            enter_screen(APP_SCREEN_START_INCLINE);
        }
        else if ((app_state.screen == APP_SCREEN_SPEED) ||
                 (app_state.screen == APP_SCREEN_INCLINE))
        {
            has_speed_input = adjustment_process(event.id) || has_speed_input;
        }
    }
    safety_keys_process(now_ms);
    if (app_state.is_safety_active)
    {
        return;
    }
    speed_preview_process(now_ms, has_speed_input, is_frame_complete);
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
