/**
 * @file main.c
 * @author N32cube
 */
 //!!!!!!!!!!!!!!!!NOTE!!!!!!!!!!!!!!!
 // Code cannot be added between /* NTFx CODE START xxxxx*/ and /* NTFx CODE END xxxxx*/
/* NTFx CODE START Include*/
#include "main.h"
#include <stdio.h>
#include <stdint.h>
/* NTFx CODE END Include*/

#include "app.h"

/**
 * @brief  Main program.
 */
int main(void)
{
    /* NTFx CODE START Config*/
    RCC_Configuration();
    GPIO_Configuration();
    NVIC_Configuration();
    DMA_Configuration();
    /* NTFx CODE END Config*/
    App_Init();

    while(1)
    {
        App_Process();
    }
}


