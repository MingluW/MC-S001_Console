#include "board_safety.h"

#include "board_pins.h"
#include "n32h47x_48x_rcc.h"

void BoardSafety_Init(void)
{
    GPIO_InitType gpio;

    RCC_EnableAHB1PeriphClk(RCC_AHB_PERIPHEN_GPIOA | RCC_AHB_PERIPHEN_GPIOB, ENABLE);
    /* 先预置禁止电平再切输出，避免初始化时短暂放行。 */
    BoardSafety_SetInhibited(true);
    GPIO_InitStruct(&gpio);
    gpio.Pin = BOARD_SAFETY_INHIBIT_PIN;
    gpio.GPIO_Mode = GPIO_MODE_OUTPUT_PP;
    gpio.GPIO_Pull = GPIO_NO_PULL;
    gpio.GPIO_Alternate = GPIO_NO_AF;
    GPIO_InitPeripheral(BOARD_SAFETY_INHIBIT_PORT, &gpio);

    GPIO_InitStruct(&gpio);
    gpio.Pin = BOARD_SAFETY_INPUT_PIN;
    gpio.GPIO_Mode = GPIO_MODE_INPUT;
    gpio.GPIO_Pull = GPIO_PULL_UP;
    gpio.GPIO_Alternate = GPIO_NO_AF;
    GPIO_InitPeripheral(BOARD_SAFETY_INPUT_PORT, &gpio);
}

bool BoardSafety_IsDisconnected(void)
{
    return GPIO_ReadInputDataBit(BOARD_SAFETY_INPUT_PORT, BOARD_SAFETY_INPUT_PIN) != 0u;
}

void BoardSafety_SetInhibited(bool is_inhibited)
{
    if (is_inhibited)
    {
        GPIO_SetBits(BOARD_SAFETY_INHIBIT_PORT, BOARD_SAFETY_INHIBIT_PIN);
    }
    else
    {
        GPIO_ResetBits(BOARD_SAFETY_INHIBIT_PORT, BOARD_SAFETY_INHIBIT_PIN);
    }
}
