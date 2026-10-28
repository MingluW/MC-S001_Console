#include "board_stdio.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

#include "board_uart.h"

/* newlib内部使用的有界堆；应用模块不直接动态分配。由链接脚本预留。 */
extern unsigned char __heap_start__;
extern unsigned char __heap_end__;

void BoardStdio_Init(void)
{
    (void)setvbuf(stdout, NULL, _IONBF, 0u);
    (void)setvbuf(stderr, NULL, _IONBF, 0u);
}

int _isatty(int fd)
{
    if ((fd == 1) || (fd == 2))
    {
        return 1;
    }
    errno = EBADF;
    return 0;
}

int _fstat(int fd, struct stat *status)
{
    if (((fd != 1) && (fd != 2)) || (status == NULL))
    {
        errno = EBADF;
        return -1;
    }
    *status = (struct stat){0};
    status->st_mode = S_IFCHR;
    return 0;
}

int _write(int fd, const void *data, size_t length_bytes)
{
    BoardUartStatus status;

    if ((fd != 1) && (fd != 2))
    {
        errno = EBADF;
        return -1;
    }
    status = BoardUart_TryWrite(data, length_bytes);
    if (status != BOARD_UART_OK)
    {
        errno = (status == BOARD_UART_FULL) ? EAGAIN : EIO;
        return -1;
    }
    return (int)length_bytes;
}

void *_sbrk(ptrdiff_t increment)
{
    static uintptr_t heap_current;
    uintptr_t heap_start = (uintptr_t)&__heap_start__;
    uintptr_t heap_end = (uintptr_t)&__heap_end__;
    uintptr_t previous;
    uintptr_t amount;

    if (heap_current == 0u)
    {
        heap_current = heap_start;
    }
    previous = heap_current;
    amount = (increment < 0) ? (uintptr_t)(-(increment + 1)) + 1u
                             : (uintptr_t)increment;
    if (((increment >= 0) && (amount > heap_end - heap_current)) ||
        ((increment < 0) && (amount > heap_current - heap_start)))
    {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_current = (increment < 0) ? heap_current - amount : heap_current + amount;
    return (void *)previous;
}
