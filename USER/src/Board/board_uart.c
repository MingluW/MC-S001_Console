#include "board_uart.h"

#include <stdbool.h>

#include "board_pins.h"
#include "n32h47x_48x_rcc.h"
#include "n32h47x_48x_usart.h"

#define BOARD_UART_TX_BUDGET 32u
#define BOARD_UART_RX_ERROR_FLAGS \
    (USART_FLAG_OREF | USART_FLAG_FEF | USART_FLAG_NEF | USART_FLAG_PEF)

static bool is_ready;
static uint8_t tx_buffer[BOARD_UART_TX_CAPACITY];
static size_t tx_head;
static size_t tx_tail;
static uint32_t tx_rejected_count;

/* RX为ISR单生产者、主循环单消费者；只在数据写完后发布head。 */
static volatile uint8_t rx_buffer[BOARD_UART_RX_CAPACITY];
static volatile uint32_t rx_head;
static volatile uint32_t rx_tail;
static volatile uint32_t rx_error_count;
static uint32_t observed_rx_error_count;

void BoardUart_Init(void)
{
    GPIO_InitType gpio;
    USART_InitType uart;

    NVIC_DisableIRQ(USART2_IRQn);
    RCC_EnableAHB1PeriphClk(RCC_AHB_PERIPHEN_GPIOA, ENABLE);
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_USART2, ENABLE);
    USART_DeInit(BOARD_DEBUG_UART);

    GPIO_InitStruct(&gpio);
    gpio.GPIO_Mode = GPIO_MODE_AF_PP;
    gpio.GPIO_Pull = GPIO_PUPD_PULL_UP;
    gpio.GPIO_Slew_Rate = GPIO_SLEW_RATE_SLOW;
    gpio.GPIO_Current = GPIO_DS_2mA;
    gpio.Pin = BOARD_DEBUG_UART_TX_PIN;
    gpio.GPIO_Alternate = BOARD_DEBUG_UART_TX_AF;
    GPIO_InitPeripheral(BOARD_DEBUG_UART_PORT, &gpio);
    gpio.Pin = BOARD_DEBUG_UART_RX_PIN;
    gpio.GPIO_Alternate = BOARD_DEBUG_UART_RX_AF;
    GPIO_InitPeripheral(BOARD_DEBUG_UART_PORT, &gpio);

    USART_StructInit(&uart);
    uart.BaudRate = BOARD_UART_BAUD_RATE;
    uart.WordLength = USART_WL_8B;
    uart.StopBits = USART_STPB_1;
    uart.Parity = USART_PE_NO;
    uart.Mode = USART_MODE_TX | USART_MODE_RX;
    uart.HardwareFlowControl = USART_HFCTRL_NONE;
    USART_Init(BOARD_DEBUG_UART, &uart);

    tx_head = tx_tail = 0u;
    rx_head = rx_tail = 0u;
    tx_rejected_count = rx_error_count = observed_rx_error_count = 0u;
    is_ready = true;
    NVIC_SetPriority(USART2_IRQn, 10u);
    NVIC_ClearPendingIRQ(USART2_IRQn);
    USART_ConfigInt(BOARD_DEBUG_UART, USART_INT_RXDNE, ENABLE);
    USART_ConfigInt(BOARD_DEBUG_UART, USART_INT_ERRF, ENABLE);
    USART_Enable(BOARD_DEBUG_UART, ENABLE);
    NVIC_EnableIRQ(USART2_IRQn);
}

BoardUartStatus BoardUart_TryWrite(const void *data, size_t length_bytes)
{
    const uint8_t *bytes = data;
    size_t free_bytes;

    if (!is_ready || (__get_IPSR() != 0u))
    {
        return BOARD_UART_NOT_READY;
    }
    if ((data == NULL) && (length_bytes != 0u))
    {
        return BOARD_UART_INVALID;
    }
    free_bytes = (tx_tail + BOARD_UART_TX_CAPACITY - tx_head - 1u)
                 % BOARD_UART_TX_CAPACITY;
    if (length_bytes > free_bytes)
    {
        ++tx_rejected_count;
        return BOARD_UART_FULL;
    }
    for (size_t i = 0u; i < length_bytes; ++i)
    {
        tx_buffer[tx_head] = bytes[i];
        tx_head = (tx_head + 1u) % BOARD_UART_TX_CAPACITY;
    }
    return BOARD_UART_OK;
}

void BoardUart_Process(void)
{
    if (!is_ready)
    {
        return;
    }
    for (uint32_t i = 0u; (i < BOARD_UART_TX_BUDGET) && (tx_tail != tx_head); ++i)
    {
        if (USART_GetFlagStatus(BOARD_DEBUG_UART, USART_FLAG_TXDE) == RESET)
        {
            break;
        }
        USART_SendData(BOARD_DEBUG_UART, tx_buffer[tx_tail]);
        tx_tail = (tx_tail + 1u) % BOARD_UART_TX_CAPACITY;
    }
}

void BoardUart_IrqHandler(void)
{
    uint32_t status = BOARD_DEBUG_UART->STS;
    uint32_t errors = status & BOARD_UART_RX_ERROR_FLAGS;
    uint32_t next;
    uint8_t byte;

    if ((status & (USART_FLAG_RXDNE | BOARD_UART_RX_ERROR_FLAGS)) == 0u)
    {
        return;
    }
    byte = (uint8_t)USART_ReceiveData(BOARD_DEBUG_UART);
    if (errors != 0u)
    {
        USART_ClrFlag(BOARD_DEBUG_UART, errors);
        ++rx_error_count;
        return;
    }
    next = (rx_head + 1u) % BOARD_UART_RX_CAPACITY;
    if (next == rx_tail)
    {
        ++rx_error_count;
        return;
    }
    rx_buffer[rx_head] = byte;
    rx_head = next;
}

BoardUartStatus BoardUart_ReadByte(uint8_t *byte)
{
    uint32_t errors = rx_error_count;

    if (!is_ready)
    {
        return BOARD_UART_NOT_READY;
    }
    if (byte == NULL)
    {
        return BOARD_UART_INVALID;
    }
    if (errors != observed_rx_error_count)
    {
        observed_rx_error_count = errors;
        return BOARD_UART_RX_ERROR;
    }
    if (rx_tail == rx_head)
    {
        return BOARD_UART_EMPTY;
    }
    *byte = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1u) % BOARD_UART_RX_CAPACITY;
    return BOARD_UART_OK;
}

BoardUartStats BoardUart_GetStats(void)
{
    BoardUartStats stats = {tx_rejected_count, rx_error_count};
    return stats;
}
