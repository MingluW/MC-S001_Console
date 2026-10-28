#include "board_lcd_port.h"

#include "board_pins.h"
#include "n32h47x_48x_rcc.h"

/* 先用保守的有限延时点屏，实际时钟频率以逻辑分析仪测量为准。 */
#define BOARD_LCD_EDGE_DELAY_LOOPS 48u

static bool is_port_ready;

static void edge_delay(void)
{
    for (volatile uint32_t i = 0u; i < BOARD_LCD_EDGE_DELAY_LOOPS; ++i)
    {
        __NOP();
    }
}

void BoardLcdPort_Init(void)
{
    GPIO_InitType gpio;

    RCC_EnableAHB1PeriphClk(RCC_AHB_PERIPHEN_GPIOA | RCC_AHB_PERIPHEN_GPIOB |
                            RCC_AHB_PERIPHEN_GPIOC, ENABLE);
    /* 先设置输出锁存电平，再切换输出模式，避免CS和RESET的上电毛刺。 */
    GPIO_SetBits(BOARD_LCD_BUS_PORT, BOARD_LCD_CS_PIN | BOARD_LCD_CLOCK_PIN);
    GPIO_ResetBits(BOARD_LCD_BUS_PORT, BOARD_LCD_DATA_PIN);
    GPIO_SetBits(BOARD_LCD_RESET_PORT, BOARD_LCD_RESET_PIN);
    GPIO_SetBits(BOARD_LCD_BACKLIGHT_PORT, BOARD_LCD_BACKLIGHT_PIN);

    GPIO_InitStruct(&gpio);
    gpio.GPIO_Mode = GPIO_MODE_OUTPUT_PP;
    gpio.GPIO_Pull = GPIO_NO_PULL;
    gpio.GPIO_Slew_Rate = GPIO_SLEW_RATE_SLOW;
    gpio.GPIO_Current = GPIO_DS_2mA;
    gpio.GPIO_Alternate = GPIO_NO_AF;
    gpio.Pin = BOARD_LCD_CS_PIN | BOARD_LCD_CLOCK_PIN | BOARD_LCD_DATA_PIN;
    GPIO_InitPeripheral(BOARD_LCD_BUS_PORT, &gpio);
    gpio.Pin = BOARD_LCD_RESET_PIN;
    GPIO_InitPeripheral(BOARD_LCD_RESET_PORT, &gpio);
    gpio.Pin = BOARD_LCD_BACKLIGHT_PIN;
    GPIO_InitPeripheral(BOARD_LCD_BACKLIGHT_PORT, &gpio);
    is_port_ready = true;
}

void BoardLcdPort_SetReset(bool is_released)
{
    if (is_released)
    {
        GPIO_SetBits(BOARD_LCD_RESET_PORT, BOARD_LCD_RESET_PIN);
    }
    else
    {
        GPIO_ResetBits(BOARD_LCD_RESET_PORT, BOARD_LCD_RESET_PIN);
    }
}

void BoardLcdPort_SetBacklight(bool is_on)
{
    if (is_on)
    {
        GPIO_SetBits(BOARD_LCD_BACKLIGHT_PORT, BOARD_LCD_BACKLIGHT_PIN);
    }
    else
    {
        GPIO_ResetBits(BOARD_LCD_BACKLIGHT_PORT, BOARD_LCD_BACKLIGHT_PIN);
    }
}

bool BoardLcdPort_Write9(uint16_t word)
{
    if (!is_port_ready || (word > 0x1FFu))
    {
        return false;
    }

    GPIO_ResetBits(BOARD_LCD_BUS_PORT, BOARD_LCD_CS_PIN);
    edge_delay();
    for (uint16_t mask = 0x100u; mask != 0u; mask >>= 1u)
    {
        GPIO_ResetBits(BOARD_LCD_BUS_PORT, BOARD_LCD_CLOCK_PIN);
        if ((word & mask) != 0u)
        {
            GPIO_SetBits(BOARD_LCD_BUS_PORT, BOARD_LCD_DATA_PIN);
        }
        else
        {
            GPIO_ResetBits(BOARD_LCD_BUS_PORT, BOARD_LCD_DATA_PIN);
        }
        edge_delay();
        GPIO_SetBits(BOARD_LCD_BUS_PORT, BOARD_LCD_CLOCK_PIN);
        edge_delay();
    }
    GPIO_SetBits(BOARD_LCD_BUS_PORT, BOARD_LCD_CS_PIN);
    edge_delay();
    return true;
}
