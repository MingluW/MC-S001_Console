#include "board_led.h"

#include "board_pins.h"

void BoardLed_Set(bool is_on)
{
    bool is_high = BOARD_LED_ACTIVE_LOW ? !is_on : is_on;

    if (is_high)
    {
        GPIO_SetBits(BOARD_LED_PORT, BOARD_LED_PIN);
    }
    else
    {
        GPIO_ResetBits(BOARD_LED_PORT, BOARD_LED_PIN);
    }
}
