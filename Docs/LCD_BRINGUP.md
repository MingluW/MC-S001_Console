# LCD 点亮基线

日期：2026-10-28。
结果：make download成功、BIN逐字节校验通过并复位运行；用户确认全部数字段和图标亮起。
第一阶段目标已完成，尚未移植独立数字/图标显示与业务界面。

## 已确认的来源

- 用户确认新屏与Cypress方案完全相同，旧Cypress代码可以正常运行。
- 旧代码：C:/Users/mingluw/Documents/cypress/260824/Q008_0926.cydsn/LCD.c。
- 旧SPI生成配置：codegentemp/Q008_0926.v，ModeCPHA=1、ModePOL=1、9位、MSB First。
- 原理图：D:/Nsing/N32H473CCL7/上控/MC-S001_Console_B756Y_V1_0_20260924.pdf。
- 第一版保留旧初始化命令/数据标记、顺序和延时，包括write_data(0xD2)，不在移植时猜测修改。

## 引脚

| MCU | 功能 | 当前方式 |
| --- | --- | --- |
| PA4 | LCD_CS0 | GPIO，低有效片选 |
| PA5 | LCD_CLOCK | GPIO，空闲高 |
| PA7 | LCD_DATA | GPIO，9位MSB先发 |
| PC15 | LCD_RESET | GPIO，低有效复位 |
| PB5 | Backlite_EN | GPIO，高电平打开背光 |

屏幕逻辑3.3V、背光使用原理图中的5VDC_LCD，背光开关由PB5控制。
GPIO模式和时钟由BoardLcdPort_Init手动配置，未在N32Cube中新增LCD配置。
N32Cube重新生成后需避免重复配置覆盖，并复查以上引脚。

## 文件和接口

```text
USER/src/Board/board_lcd_port.c     GPIO和9位串行输出
USER/src/Modules/lcd/lcd.c         初始化状态机、16字节缓存、分步刷新
USER/src/App/app.c                开机设置全亮、每轮调度、一次完成日志
USER/inc/board_lcd_port.h          板级接口
USER/inc/lcd.h                     屏幕模块接口
```

- Lcd_Init(now_ms)：启动复位/初始化，返回时屏幕尚未初始化完成。
- Lcd_SetAll(true/false)：设置全亮/全灭目标缓存，可以在初始化期间调用，不立即传输。
- Lcd_Process(now_ms)：每轮最多发一个9位字，无毫秒阻塞等待。
- Lcd_IsReady：初始化序列已发完，不表示有硬件应答。
- Lcd_IsBusy：初始化或帧提交仍在进行；出错需结合Lcd_HasError判断。
- Lcd_HasError：本地发送接口失败，不检测屏幕是否连接或屏幕供电。

点亮基线的App_Init启动LCD并设置全亮；之后保持全亮，不周期性全亮/全灭。
后续按键演示版本改为上电显示0.0，见KEYS_LCD_DEMO.md。
用户此前注释掉的uart_echo_process、log_process调用保持关闭，没有重新启用。
已有闪灯仍为亮灭各1500ms，日志时间和串口发送继续按原结构运行。

## 时序和缓存

- 复位：高100ms → 低200ms → 高200ms；由毫秒状态机推进，期间主循环可处理串口/闪灯。
- 旧初始化中的3处20ms等待保留；命令表共27个9位字。
- 发送帧与旧show_dot一致：C:2A、D:00、D:03、C:2C，然后16字节显示数据。
- 9位字bit8=0是命令，bit8=1是数据；CPOL=1/CPHA=1，下降沿更新、上升沿采样。
- 首版用GPIO发送，当前SDK接口仅提供8/16位SPI配置，不能直接将9位字扩成16个时钟。
- 每字独立片选，固定9个时钟；有限循环延时，无无限轮询，不屏蔽中断。
- 实际GPIO时钟频率和CPU耗时未用逻辑分析仪/DWT实测，不宣称固定频率或微秒耗时。
- 显示传输使用快照；传输中修改目标缓存只触发下一帧，不混入本次帧。
- 相同内容不重复刷屏。当前不搬入旧FlashLcd中的启停/通信/风扇逻辑。

## 验证结果

- GCC构建通过：text 17160、data 104、bss 7976字节（bss含堆栈预留）。
- ARM模拟测试Tests/test_lcd.py通过：直接从旧Cypress代码提取命令作对照，验证9位波形顺序、
  复位和20ms等待、每轮单字预算、全亮/全灭帧、缓存快照、内容不变不刷新、失败处理。
- 同时回归printf/日志/TX缓冲测试。RX读取因用户关闭回显而被链接器移除，该项测试明确SKIP。
- 新LCD代码未出现编译警告。旧SDK/nosys/RWX及用户关闭演示留下的未使用静态函数警告未屏蔽。
- make download成功，17272字节BIN校验通过；用户确认全段/全图标亮起。
- COM28被其他程序占用，本轮未采集LCD日志，不声称已读取串口输出。
- 断电重启、单数字/单图标、独立背光开关和刷新耗时尚未验证。

运行软件测试：先在Makefile目录make，再运行python Tests/test_lcd.py（从工程根目录）。
依赖及临时安装位置同UART_LOG.md。

## 后续进度

已将旧Tube1~Tube4的数字及速度图标/小数点整理为const映射，接入app_display，
继续按“改缓存 → 标记变化 → Lcd_Process分步提交”的方式扩展；按键演示记录见KEYS_LCD_DEMO.md。
其余图标和页面按实际需求逐项扩展验证。
保持Cypress工程只读，新增硬件优化需对比这份已点亮基线。
