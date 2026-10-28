#ifndef BOARD_STDIO_H
#define BOARD_STDIO_H

/** BoardUart_Init后调用一次，禁用stdout/stderr缓冲。仅主循环，启动阶段使用。 */
void BoardStdio_Init(void);

#endif
