# USART2 调试串口与日志

日期：2026-10-28。已编码、编译并通过ARM指令模拟测试；已通过make download烧写、BIN逐字节校验并复位运行；实板串口TX/RX现象待验证。

## 接线和验证

USART2，115200 baud，8数据位、无校验、1停止位，无硬件流控。
使用3.3V TTL USB转串口，不使用RS232电平接口。

| CN501 | 板端信号 | USB转串口 |
| --- | --- | --- |
| 1 | GND | GND |
| 2 | DBG_RX / PA3 | TX |
| 3 | DBG_TX / PA2 | RX |
| 4 | MCU 3.3V | 板子已供电时不连接此脚 |

针号以原理图和实板标识为准，接线前确认方向。
串口工具设置115200、8N1并打开，在Makefile目录执行make download。
启动后应有printf启动信息和ERR/WARN/DBG示例；每5秒输出DBG运行时间；蓝灯亮灭各1500ms。
终端关闭本地回显，发送ASCII文本，应收到原样回显。断电重启再确认启动信息和闪灯。
ERR启动示例带SELFTEST，表示打印功能演示，不代表真实故障。
周期日志和字符回显是当前bring-up功能，后续调试口增加命令解析时替换此回显入口。

## 使用方式

```c
#include <stdio.h>
#include "log.h"

printf("value=%lu\r\n", (unsigned long)value);
ERR("[UART] receive failed, code=%d", code);
WARN("[KEYS] queue full");
DBG("[APP] speed=%u", speed);

Log_SetLevel(LOG_LEVEL_DBG);   /* ERR、WARN、DBG全部显示，默认值 */
Log_SetLevel(LOG_LEVEL_WARN);  /* 只显示ERR、WARN */
Log_SetLevel(LOG_LEVEL_ERR);   /* 只显示ERR */
Log_SetLevel(LOG_LEVEL_OFF);   /* 关闭分级日志，不影响printf */
```

输出示例（时间为示意值）：

```text
MC-S001 Console: USART2 115200 8N1
[0d 00:00:00.012][ERR] [SELFTEST] error log example (not a real fault)
[0d 00:00:00.012][WARN] [SELFTEST] warning log example
[0d 00:00:00.012][DBG] [APP] boot, LED interval=1500ms
[0d 00:00:05.012][DBG] [APP] alive, TX rejected=0
```

原始printf不加前缀、不转换换行，文本使用\r\n。
ERR/WARN/DBG是调用Log_Write的printf风格宏，自动添加开机时长[天数d HH:MM:SS.mmm]、级别、一个CRLF。
编译器检查格式串与参数类型；接收数据不能直接当格式串，使用DBG("%s", text)。
返回true表示入队成功或被级别过滤；false表示参数错误、超长或队列满。
关键输出应检查返回值；当前启动演示和周期DBG允许丢弃，用(void)显式标记。
记录日志不能代替业务错误处理。

## 文件职责和初始化

- USER/src/Board/board_uart.c：手动初始化USART2 PA2/PA3 AF5，缓冲收发和统计。
- USER/src/Board/board_stdio.c：newlib _write重定向、stdio设置、有界_sbrk。
- USER/src/Modules/log/log.c：格式化、时间/级别前缀、过滤。
- USER/src/App/app.c：初始化、主循环TX推进、回显和周期诊断。
- USER/src/n32h47x_48x_it.c：生成区外USART2_IRQHandler调用BoardUart_IrqHandler。
- 全部手写头文件在USER/inc。

生成的RCC/GPIO/NVIC/DMA完成后，App_Init调用BoardUart_Init、BoardStdio_Init。
USART2目前由Board独占初始化，N32Cube尚未配置USART2。
以后在N32Cube中加入USART2必须明确唯一初始化来源，不得让两套配置互相覆盖。

## 容量和限制

- TX1024字节、可用1023；整块入队，不足时整块拒绝并累计统计。
- 每轮最多推进32字节TX，硬件不就绪立即返回，不等待发送完成。
- RX128字节、可用127；满时丢新数据并报告RX_ERROR。ISR只接收、不打印、不解析。
- ReadByte区分EMPTY与RX_ERROR；错误计数变化时先报告错误，再读现有数据。
- 单条日志最多191字节，含前缀和CRLF，超长整条拒绝，不发截断的半条日志。
- printf/日志仅主循环调用，格式化耗费CPU，但不等待串口；不支持ISR、多线程或RTOS重入。
- printf成功代表交给TX队列，不代表物理线已经发完；队列满时_write返回-1、errno为EAGAIN。
  printf可能置流错误，需要时clearerr(stdout)再重试。一次printf内部可能多次调用_write，
  不保证整个超长printf原子性；整条日志使用ERR/WARN/DBG。
- ERR没有强制插队，满队列时也可能失败；重要错误还应保存在业务状态/事件中。
- 使用newlib-nano，默认支持整数、字符串，不启用%f浮点格式化；stdin/scanf未实现。
- 链接脚本为libc预留2KB有界堆、4KB栈；应用不直接malloc/free。
  栈预留不等于硬件保护，功能复杂化后检查实际栈水位。
- 日志不能持续超过115200串口线速，必要时降低输出级别。

## 软件测试及重生成

Tests/test_uart_log.py加载实际编译ELF，通过Unicorn执行ARM代码，替代发送硬件和时间，
模拟寄存器输入验证RX逻辑。已通过TX/RX空满、环形回绕、硬件忙返回、错误字节丢弃、
实际printf重定向、三级过滤、CRLF、超长/空指针/满队列拒绝和libc堆越界验证。
不验证真实引脚、波特率、中断时序或电气条件。

先make构建，再运行（测试依赖在临时目录，不加入固件）：

```powershell
python -m pip install --target "$env:TEMP/mcs001-testdeps" unicorn pyelftools
python D:/Nsing/Projects/MC-S001_Console/Tests/test_uart_log.py
```

重生成检查：USART2中断入口、Board初始化、Modules/log构建目录、USART驱动源文件、
nano.specs、链接脚本heap边界/堆栈预留，以及已有main/App接入点。

## 下载校验修复（2026-10-28）

原脚本下载HEX却对照BIN校验。链接对齐在0x08000218留下8字节空隙，HEX未包含而BIN填0，
导致读回FF时校验失败。已统一下载build/MC-S001_Console.bin到0x08000000，再用同一BIN校验。
实测make download返回0，15704字节Verify successful，随后复位运行。串口输出仍需连接终端确认。

时间显示更新：Log_Process(now_ms)在主循环每轮更新，常规路径仅32位加减比较，跨天才归一化；
只在实际输出日志时转换时分秒，不使用浮点或64位除法。两次更新间隔必须小于约49.7天，
即使日志关闭仍更新，因此正常运行跨毫秒计数回绕时时间连续。已模拟验证分/时/天进位和回绕。
最新时间格式版本已编译和软件测试通过，尚未再次烧写；没有进行实板CPU占用率测试。
