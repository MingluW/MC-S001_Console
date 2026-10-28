# 当前工程状态

记录日期：2026-10-28。发生实际变化后更新，不将规划当作完成项。

## 路径与来源

- 实际固件工程：`D:/Nsing/Projects/MC-S001_Console`。
- 讨论和硬件资料工作目录：`C:/Users/mingluw/Desktop/项目/MC-S001`。
- N32Cube 工具：`D:/Nsing/N32Cube_H47x_48x_v109/NTFx.exe`；此前截图界面显示 Ver 1.0.1.5。目录名不能代替 SDK 版本证明。
- Driver 来自 N32Cube 生成资源，具体 SDK 版本尚未追溯，不填写猜测版本。
- Cypress 只读参考：`C:/Users/mingluw/Documents/cypress/260824/Q008_0926.cydsn`。
- 上控原理图：`D:/Nsing/N32H473CCL7/上控/MC-S001_Console_B756Y_V1_0_20260924.pdf`。

## 已确认

- 工程及原理图型号 N32H473CCL7，LQFP48；J-Link 设备名 N32H473CC。
- 工程链接配置：256KB Flash、144KB RAM，初始栈顶 0x20024000。
- PB2 接 LED501，低电平点亮；PA13/PA14 为 SWD。
- PA2/PA3 为USART2调试TX/RX，已实现115200 8N1、printf、ERR/WARN/DBG和接收回显，实板待验证。
- 生成 GPIO 为 PB2 推挽、无上下拉、初始高；保留 SWD。
- 生成时钟配置使用内部 HSI 经 PLL 配置到 240MHz；不能仅根据早期建议写为 HSI 直连。
- 原阻塞闪灯固件已烧写并校验，用户确认蓝灯闪烁；当前串口日志版本已通过make download烧写、校验并复位运行。
- App/Board 基础已建立：App_Init 点亮蓝灯，App_Process 每经过1500ms切换一次，主循环不等待。
- BoardTime_Millis 只读生成代码的 mwTick，继续使用现有1ms SysTick，不重复初始化或修改中断。
- BoardLed_Set 屏蔽PB2低有效细节；GPIO配置仍由生成的 GPIO_Configuration 完成。
- Makefile 已显式纳入 USER/src/App、USER/src/Board 源文件目录；头文件统一使用 USER/inc，VS Code 已同步。
- 串口日志版本GCC编译通过（text 17160 / data 104 / bss 7976字节，bss统计含堆栈预留）。
- 使用newlib-nano，libc有界堆2KB、栈预留4KB；TX/RX缓冲、分级日志和printf重定向通过ARM模拟测试。
- make download / download_jlink 默认 J-Link 路径 D:/Jlink/app/JLink，SWD 100kHz，先构建再下载、校验、复位运行。
- VS Code 已配置本地工具路径和 J-Link 调试；实际烧写成功路径为 J-Link Commander，不能据此声称 F5 全流程已实测。
- 现有编译警告包含 SDK 未使用变量、nosys 占位和 ELF RWX 段，尚未系统清理。

## 尚未实施

- Modules 下的后续功能按需添加，目前没有建立空模块。
- UART日志实板接线、输出波特率和RX回显待验证；步骤见Docs/UART_LOG.md。
- LCD独立数字/图标及业务显示、按键、CAN/CAN FD、新下控协议。
- 当前串口日志版本已烧写；非阻塞闪灯时序、断电重启、断点调试和串口现象待确认。

## 当前代码阅读入口

1. USER/src/main.c：生成的硬件初始化，之后调用 App_Init 和 App_Process。
2. USER/src/App/app.c：1500ms 闪灯任务，无符号时间差处理计数回绕。
3. USER/src/Board/board_led.c 和 USER/inc/board_pins.h：逻辑开关转换为PB2输出电平。
4. USER/src/Board/board_time.c：适配生成的 mwTick，约49.7天回绕。
5. USER/src/Board/board_uart.c、board_stdio.c：USART2收发、printf重定向。
6. USER/src/Modules/log/log.c、USER/inc/log.h：ERR/WARN/DBG。

调试暂停或其他任务超时导致错过周期时，恢复后仅切换一次，从当前时间开始下一个1500ms，
不循环补闪。后续在 App_Process 中添加其他快速返回的任务。
闪灯的时间判断已封装在私有 led_process(now_ms) 中；App_Process 只获取时间并调度，
后续任务不会因闪灯未到期而被提前 return 跳过。
重新生成必须检查 main.c 的 app.h/App 接入、Makefile 自定义目录及 mwTick/1ms时基是否保留。

## Cypress 经验

- 沿用 UART 端口、协议编解码、通信服务三层思路，评估复用纯协议核心及自测。
- 将 FlashLcd 中的业务和通信移出显示模块；按键统一输出事件。
- 时间模块只提供计时；安全锁、开发模式采用非阻塞状态机。
- 新控制协议确认前，不默认兼容旧协议；用户确认同屏，旧9位SPI初始化已复用并点亮。

目录约定更新（2026-10-28）：App/Board 是后加的手写模块；.c 位于 USER/src/App、USER/src/Board，.h 全部位于 USER/inc。根目录不再保留 App/Board。

下载修复：MC-S001_Console.jlink和flash-blink.jlink统一下载/校验同一BIN，避免HEX稀疏空隙与BIN填充值不同。make download已实测成功。

已添加 make reset（别名 reset_jlink）：通过J-Link执行 r、g、q，复位并运行现有固件，
无构建/下载/擦除依赖。新增命令仅做dry-run检查，本次未实际复位。

日志时间改为[天数d HH:MM:SS.mmm]；32位计时、仅输出时转换时分秒。进位及回绕模拟测试通过，最新版本尚未烧写，未实测CPU占用。

## LCD点亮完成（2026-10-28）

已新增USER/src/Board/board_lcd_port.c、USER/src/Modules/lcd/lcd.c及USER/inc对应头文件，
用GPIO输出Mode3/MSB/9位时序，保留旧初始化顺序与延时；复位/等待用非阻塞状态机，
每轮最多一个字，初始化后提交16字节全亮段码并保持。
已编译、通过ARM模拟对照测试、make download校验成功；用户确认全部数字段和图标亮起。
该版本包含此前新日志时间格式，已一起烧写。详情见LCD_BRINGUP.md。
用户已注释uart_echo_process和log_process调用，保留关闭状态。COM28被占用，本轮未采集串口。

## GitHub版本管理准备（2026-10-28）

用户选择新建私有仓库MC-S001_Console。已添加README、忽略规则和文本格式规则，
保留源码/SDK/配置/文档/测试，排除build、旧备份、Flash备份及临时探测脚本。
GitHub账号已确认MingluW，提交作者mingluw <mokamiyou@gmail.com>。
目标私有仓库：https://github.com/MingluW/MC-S001_Console。
基线提交说明：LCD全亮已实板确认；远端推送结果以Git实际记录及会话验证为准。
