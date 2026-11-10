#include "app_powerboard.h"

#include <stddef.h>

#include "log.h"
#include "powerboard.h"

static AppScreen previous_screen;
static uint16_t observed_speed_x10;
static bool fault_stop_requested;

void AppPowerboard_Init(void)
{
    previous_screen = APP_SCREEN_BOOT_ALL;
    observed_speed_x10 = 0u;
    fault_stop_requested = false;
    if (!Powerboard_Init())
    {
        (void)ERR("[CAN] initialization failed; UI remains available, no target sent");
    }
}

void AppPowerboard_Process(uint32_t now_ms, const AppState *state)
{
    if (state != NULL)
    {
        bool accepted = Powerboard_SetSafetyLock(state->is_safety_active);
        if (state->is_safety_active)
        {
            previous_screen = APP_SCREEN_STOPPED;
            observed_speed_x10 = 0u;
        }
        else if ((state->screen == APP_SCREEN_STOPPED) && (previous_screen != APP_SCREEN_STOPPED))
        {
            accepted = Powerboard_RequestStop();
        }
        else if ((state->screen == APP_SCREEN_START_INCLINE) &&
                 (previous_screen != APP_SCREEN_START_INCLINE))
        {
            accepted = Powerboard_RequestStart(state->demo_speed_x10);
        }
        else if (((state->screen == APP_SCREEN_SPEED) || (state->screen == APP_SCREEN_INCLINE)) &&
                 (state->demo_speed_x10 != observed_speed_x10))
        {
            accepted = Powerboard_RequestSpeed(state->demo_speed_x10);
        }
        /* 未就绪的安全STOP仍逐轮保留重试；初始化失败已有日志，不能逐轮刷屏。 */
        if (!accepted && !state->is_safety_active)
        {
            (void)WARN("[CAN] UI request not accepted; check init/link, STOP then START to retry");
        }
        previous_screen = state->screen;
        observed_speed_x10 = state->demo_speed_x10;
    }
    Powerboard_Process(now_ms);
    uint8_t controller_error = Powerboard_GetControllerError();
    AppControl_SetErrors(Powerboard_GetCommunicationError(), controller_error);
    if ((controller_error != 0u) && !fault_stop_requested)
    {
        /* 请求停止不等待ACK；失败零目标由协议状态机保留并重试。 */
        fault_stop_requested = Powerboard_RequestStop();
        if (fault_stop_requested)
        {
            previous_screen = APP_SCREEN_STOPPED;
            observed_speed_x10 = 0u;
        }
    }
}
