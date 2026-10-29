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
- LCD其余数字/图标及完整业务显示、CAN/CAN FD、新下控协议。
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
基线提交 `7e272df5e8b1d1bbc3476db386b919c9723a30e1`：LCD全亮已实板确认。
已推送main和标签bringup-lcd-v0.1.0，并通过git ls-remote核对；GitHub页面确认Private。
后续功能阶段独立提交，重要实测节点打标签；不得默认强制推送或丢弃未提交修改。


## 按键与LCD数字演示（2026-10-28）

已建立BoardKeys → Keys → AppControl/AppState → AppDisplay → LCD的最小链路。
K804/PB13速度减、K805/PB14速度加，低电平有效、Board手工初始化上拉输入。
上电显示0.0，每次0.1，演示范围0.0~30.0；不发送电机控制命令。
10ms采样、20ms消抖、按住500ms后每100ms连调；上电按住须先松手，两键同时按下不调整。
AppDisplay变化时最多每50ms提交完整16字节，LCD每轮最多发送一个9位字，保留帧快照。
数字/小数点/速度图标const映射来自只读Cypress lcdfont.c；其余图标和业务尚未接入。
新增App/Board/Modules源码及平铺inc头文件，Makefile已纳入keys；VS Code现有inc路径覆盖新增头文件。
保留用户1500ms闪灯，以及已关闭的回显/周期自测调用。

GCC编译通过：text 19780 / data 104 / bss 8048字节（bss含堆栈预留）；无新增模块编译警告。
Tests/test_keys_display.py通过，执行真实ARM ELF：按键抖动、长按、上电按住、队列满、计时回绕、
控制上下限、同时按键、0.0~30.0逐值段码对照、50ms合并、帧复制及失败重试，并回归旧LCD和UART/日志。
RX读取因用户关闭回显被链接器裁剪，仍明确SKIP；未采集串口或测量实板CPU占用。
已执行make download：19892字节BIN写入0x08000000，verifybin成功，随后复位并运行。
用户回复“显示和加减都正常”，确认本版屏幕显示及V+/V-加减功能实板正常。
该回复针对0.0显示、每次0.1加减和长按连调的验证问题；未逐项确认的边界、
所有数字/图标位置、同时按键、上电按住及断电重启仍待专项验证。
使用方法和源码职责见KEYS_LCD_DEMO.md。
本轮未提交或推送GitHub。

## 速度单位选择（2026-10-28）

用户澄清KMPH应选择KPH或MPH，PH共用，K/M分别控制。
已核对旧lcdfont.c逐段Lcd_code注释：第13字节低4位为图标01、PH02、M04、K08。
正常显示不再全开0F：默认KPH=0B，可选MPH=07，包含原速度图标，不覆盖数字高4位。
AppState增加speed_unit，由AppControl_SetSpeedUnit统一修改；默认宏APP_DEFAULT_SPEED_UNIT为KPH。
AppDisplay将单位纳入变化检测，数值不变但单位改变也刷新。无效单位不提交。
本版仅修改显示单位，演示数值不换算、不存Flash，未定义单位切换按键。
构建通过：text 19928 / data 104 / bss 8056字节；既有警告保留，无新增模块警告。
Tests/test_keys_display.py通过：两种单位逐值段码、K/M互斥且PH保留、默认KPH、单位接口与无效值、
仅改单位刷新，并回归按键/LCD/UART日志；RX读取仍SKIP。
本轮未烧写，单位拆分的实板显示待验证；未提交或推送GitHub。

## 按键可读性整理（2026-10-28）

按用户要求将keys.c中的消抖和长按连发分别提取为static debounce_process与repeat_process。
Keys_Process仅负责10ms采样，再按“消抖 → 连发”的顺序逐键调用。
保留20ms消抖、500ms首次连发等待、100ms连发间隔、上电按住保护、释放候选暂停连发及队列策略。
GCC构建通过：text 20016 / data 104 / bss 8056字节，无新增编译警告。
Tests/test_keys_display.py回归通过，包含消抖、连发、回绕、队列及LCD单位显示测试；RX读取仍SKIP。
本轮未烧写、未提交或推送GitHub。


## 开机动画与开始/停止界面（2026-10-28）

已按用户要求增加非阻塞流程：全亮→3333→2222→1111→0000→8888闪烁等待开始。
全亮和倒计时每步暂定500ms，8888每1000ms亮灭切换；计时从目标帧发送完成后开始，
因此LCD初始化期间不会先跳过全亮动画，错过周期也不会连跳倒计时。
开始后坡度清0，显示GRADE 0，完整显示500ms后切速度0.0/KPH；每次重新开始均清零演示值。
手动按坡度键切坡度页并调整整数值，保持该页；按速度键切速度并调整0.1，保持该页。
停止可打断开机、等待、启动停留和手动页面，立即提交----，再次开始走坡度0→速度流程。
停止仍经过正常20ms消抖；传输中快照完成后发送新帧，不在SPI字中途打断。
切页不受同页50ms刷新限速影响；初始化/发送失败不算帧完成。

已接入PB10开始、PB11坡度减、PB12坡度加、PB13速度减、PB14速度加、PB15停止。
开始/停止仅按下事件，加减保留长按连发。停止独立锁存，普通队列满也可取出，
清除旧普通事件，且同轮开始/加减不能覆盖停止。上电按住保护保留。
坡度暂定0~15整数档、速度0.0~30.0；动画/等待阶段忽略开始或加减，开始仅在等待/停止有效。
启动坡度0的500ms内忽略加减，停止始终有效。全部参数是本地显示演示，不含实际机构动作。
AppScreen状态与计时由AppControl维护，AppDisplay只构帧；LCD增加四位整数、GRADE和----映射。
用户1500ms蓝灯及关闭的串口自测保持；未添加CAN，后续可独立接入通信与确认状态。

GCC构建通过：text 21768 / data 104 / bss 8128字节，无新增模块编译警告，既有警告保留。
Tests/test_ui_flow.py通过（包含此前keys/display/LCD/UART回归），检查：
帧完成计时、全亮/每个倒计时段码/闪烁明灭、启动500ms边界、KPH、手动切页保持、坡度限值、
停止/开始同按优先级、停止打断启动及动画、停止绕过50ms合并、普通队列满时停止、计时回绕。
RX读取仍因回显关闭而SKIP。软件验证不能替代PB10/11/12/15和新段码/时序的实板验证。
本轮未烧写，新流程及此前KPH/MPH拆分均待实板确认；未提交或推送GitHub。
使用步骤、时序宏与后续CAN边界见KEYS_LCD_DEMO.md。


## 本阶段版本保存（2026-10-28）

按用户要求，将本阶段六键、AppControl/AppDisplay、LCD数字/单位、开机动画与开始/停止流程，
连同测试、构建配置和文档保存为一次功能提交，提交说明为：
feat: add key-driven LCD UI with startup animation and start-stop flow。
此前“本轮未提交”的记录是各实现步骤完成时的历史状态，相关源码随此次版本保存统一纳入。
本阶段已编译并通过Tests/test_ui_flow.py及其回归；最新动画、KPH/MPH与新增按键流程尚未烧写。
保留已实板确认的速度加减结论及LCD点亮基线标签，不将本次保存标记为新实板验证节点。


## CAN资料初审（2026-10-28）

已读取D:/Nsing/CAN通讯协议的核心总线/FP2文档及速度、坡度、控制器Feature相关页面，
对照上控/下控CAN收发器与终端图纸；具体事实、文档矛盾、接入问题和实施顺序见CAN_REVIEW.md。
需要优先确认既有下控固件/协议版本、实际Feature子集、KPH/%与Hz/Count边界、
START/STOP语义、心跳失联规则、ACK事务超时及实际节点/终端配置。
未新增CAN源码，未改动固件、未烧写或发送CAN报文；本次审阅记录尚未提交GitHub。


CAN范围进一步确认：下控已有固件，首版开始/停止/速度加减，速度KPH、停止先写速度0，
心跳由上控发送，故障显示沿用Cypress并优先无应答/过流/过压。
通用文档未确定跑步机独立使能报文，也未给出本机保活报文/周期/失联阈值；
Cypress心跳500ms仅作旧实现参考，不当作CAN要求。详细依据及故障映射边界见CAN_REVIEW.md。
本次未修改固件、未烧写、未提交GitHub。


## CAN FD首版速度与串口日志（2026-10-28）

按用户要求先实现开始/速度目标（KPH）、停止写零，暂不发心跳及坡度控制。
用户追加初始0.3 KPH，已添加APP_START_SPEED_X10=3，保留0.1步长与原0~30范围；
当前APP_START_INCLINE_MS已为1000，保留该值，本轮未改回500。
BoardCan配置FDCAN1、PA15 TX/PB3 RX、主PLL240M/6=40M、250k/2M、标准FD+BRS ISO、SJW20/4。
使用静态SRAM1224字节消息区、16*64字节RX FIFO及单个专用TX缓冲，主循环每轮最多接收4帧。
SDK自动重发关闭；TX200ms取消等待不阻塞，未确认取消不覆盖RAM；Bus-off不自动恢复。
默认文档索引1，发送0x201/接收0x241，实际下控节点、线路和协议子集仍待实板确认。

新增Modules/powerboard下fp2.c、powerboard.c、can_trace.c，App新增app_powerboard.c，
头文件统一inc；Makefile纳入新目录与SDK fdcan.c，VS Code现有include路径已覆盖。
开始直接写Feature301=0.3，无猜测独立使能指令；停止清除待发非零目标并优先写0，失败保留零目标再尝试。
初次目标应答后订阅302/2108/2380以打印反馈；厂商故障仅记录原始float位型，不猜映射为过流/过压。
事务等硬件完成后等待FP2应答，500ms无应答锁存，非零不自动重发，明确STOP/START后才重新发起。
日志区分TX_REQ、TX_SENT、RX原始字节、FP2 ACK/Error/Event、总线失败及应用超时，
包含CAN ID、FD/BRS、物理长度和完整数据。64字节拆32字节两行，拥塞时丢失有统计，不阻塞业务。
正常速度0.3的CAN12字节为08 12 06 2D 01 9A 99 99 3E 00 00 00。
界面仍显示请求目标，未将ACK当作实际速度或停机确认。MPH接口仍为此前显示演示，当前CAN只按KPH对接。

构建通过：text 33484 / data 104 / bss 11192字节，无新增模块编译警告，既有SDK/nosys/RWX与关闭自测警告保留。
Tests/test_can.py通过，执行实际ARM ELF和SDK消息RAM/收发代码：时钟/位时序/DLC、float逐值、
ACK/Error、错节点、Bus-off、取消与回绕、分片/残帧、停止优先与零目标重试、日志拥塞、App接入。
Tests/test_ui_flow.py回归通过，适配0.3初始速度和当前配置的启动停留；回显关闭导致RX读取API测试仍SKIP。
本轮未烧写、未采集实板CAN/串口波形、未验证电机动作或下控兼容；未提交GitHub。
联调操作、报文与源码阅读入口见CAN_BRINGUP.md；原始文档疑点见CAN_REVIEW.md。

## CAN发送超时排查（2026-10-29）

用户提供实板串口日志：0.3KPH TX_REQ后200ms取消并TX_FAIL，无TX_SENT或RX；订阅也失败。
通过J-Link只读寄存器确认CCCR=340、PSR=707、ECR=0、PB3复用输出且输入采样0，未复位或改写实板。
发现上版BoardCan_Init沿用TX的AF_PP配置到RX，是初始化疏漏；修复PB3为INPUT，保留上拉及AF26。
同时修正DLEC掩码未移位（1792应解码7），增加RX/ACT/CCCR诊断，禁止初始速度TX失败后继续订阅。
GCC通过：text33752 / data104 / bss11200；Tests/test_can.py通过，补充RX方向/路由和诊断回归。
本次修复未烧写，实板通信是否恢复待确认，未验证电机动作，未提交GitHub。
寄存器证据及下一步观察点见CAN_DEBUG_20261029.md。

用户进一步明确：上述截图测试未连接下控。因此无CAN硬件ACK、无TX_SENT/FP2回复本身为预期，
不能据此判断下控不兼容或声称修复后必然通信成功。当前只观察到请求超时取消且主循环仍运行；
控制器完整收发尚待内部回环或对端接入验证，RX方向修复是否解决该现象也尚未实测。

2026-10-29用户提供修复后日志并再次确认未接下控：RX=1、ACT=1、CCCR=340，
TX_REQ后立即TX_FAIL，随后TEC=8/REC=0、LEC=DLEC=7；不再出现200ms等待，也不再继续订阅。
表明控制器空闲和RX高电平已观察到，发送失败符合无应答节点条件；完整CAN收发仍未验证。
TX_FAIL与STATUS中TEC快照不同由100ms诊断更新造成。详情见CAN_DEBUG_20261029.md。
