#include "board_keys.h"

#include "board_pins.h"
#include "n32h47x_48x_rcc.h"

void BoardKeys_Init(void)
{
    GPIO_InitType gpio;

    RCC_EnableAHB1PeriphClk(RCC_AHB_PERIPHEN_GPIOB, ENABLE);
    GPIO_InitStruct(&gpio);
    gpio.Pin = BOARD_KEY_SPEED_DEC_PIN | BOARD_KEY_SPEED_INC_PIN |
               BOARD_KEY_START_PIN | BOARD_KEY_STOP_PIN |
               BOARD_KEY_INCLINE_DEC_PIN | BOARD_KEY_INCLINE_INC_PIN;
    gpio.GPIO_Mode = GPIO_MODE_INPUT;
    gpio.GPIO_Pull = GPIO_PULL_UP;
    gpio.GPIO_Alternate = GPIO_NO_AF;
    GPIO_InitPeripheral(BOARD_KEYS_PORT, &gpio);
}

uint32_t BoardKeys_Read(void)
{
    uint16_t levels = GPIO_ReadInputData(BOARD_KEYS_PORT);
    uint32_t pressed = 0u;

    if ((levels & BOARD_KEY_SPEED_DEC_PIN) == 0u)
    {
        pressed |= BOARD_KEYS_SPEED_DEC;
    }
    if ((levels & BOARD_KEY_SPEED_INC_PIN) == 0u)
    {
        pressed |= BOARD_KEYS_SPEED_INC;
    }
    if ((levels & BOARD_KEY_START_PIN) == 0u)
    {
        pressed |= BOARD_KEYS_START;
    }
    if ((levels & BOARD_KEY_STOP_PIN) == 0u)
    {
        pressed |= BOARD_KEYS_STOP;
    }
    if ((levels & BOARD_KEY_INCLINE_DEC_PIN) == 0u)
    {
        pressed |= BOARD_KEYS_INCLINE_DEC;
    }
    if ((levels & BOARD_KEY_INCLINE_INC_PIN) == 0u)
    {
        pressed |= BOARD_KEYS_INCLINE_INC;
    }
    return pressed;
}
