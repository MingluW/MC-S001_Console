#include "app_powerboard.h"

#include <stddef.h>

#include "log.h"
#include "powerboard.h"

static AppScreen previous_screen;
static uint16_t observed_speed_x10;

void AppPowerboard_Init(void)
{
    previous_screen = APP_SCREEN_BOOT_ALL;
    observed_speed_x10 = 0u;
    if (!Powerboard_Init())
    {
        (void)ERR("[CAN] initialization failed; UI remains available, no target sent");
    }
}

void AppPowerboard_Process(uint32_t now_ms, const AppState *state)
{
    if (state != NULL)
    {
        bool accepted = true;
        if ((state->screen == APP_SCREEN_STOPPED) && (previous_screen != APP_SCREEN_STOPPED))
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
        if (!accepted)
        {
            (void)WARN("[CAN] UI request not accepted; check init/link, STOP then START to retry");
        }
        previous_screen = state->screen;
        observed_speed_x10 = state->demo_speed_x10;
    }
    Powerboard_Process(now_ms);
}
