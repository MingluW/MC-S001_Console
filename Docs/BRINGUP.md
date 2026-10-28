# 板卡基础验证

## 非阻塞闪灯（2026-10-28）

代码：`USER/src/main.c` → `USER/src/App/app.c` → `USER/src/Board/board_led.c`、`USER/src/Board/board_time.c`。
只新增当前使用的模块，没有创建UART/SPI/LCD/CAN空壳。

- 生成代码负责时钟、GPIO、NVIC、DMA初始化，继续提供1ms SysTick计数 mwTick。
- App_Init 初始点亮蓝灯，随后每1500ms切换一次，完整周期约3秒。
- App_Process 未到期立即返回，无阻塞延时或等待循环。
- 间隔判断使用 uint32_t 时间差，允许计数回绕。要求主循环正常运行，不能跨一个完整回绕周期不调度。
- 错过一个或多个周期时只切换一次，不补闪；实际周期会受到主循环调度延迟影响。
- 修改间隔在 USER/src/App/app.c 的 APP_LED_INTERVAL_MS；修改引脚/有效电平在 USER/inc/board_pins.h，
  同时必须在N32Cube中同步GPIO配置，避免只改宏却未初始化新引脚。

### 本次软件验证

- ARM GCC 构建通过，新 App/Board 源文件均参加编译；已生成ELF/HEX/BIN。
- 新增手写模块无编译警告；生成代码/链接的既有警告没有屏蔽。
- 软件静态检查和编译不代表实际板上计时验证通过。

### 实板验证步骤（待执行）

1. 接好SWD并为板子供电，在 Makefile 目录执行 make download。
2. 检查下载校验成功，LED501蓝灯应亮/灭各约1500ms。
3. 断电再上电，确认自动闪灯。
4. 在 App_Process 设断点可看到其持续被调用，不会停留在延时等待里。
5. 暂停超过3秒后继续运行，灯最多立即切换一次，之后恢复1500ms间隔，不连续补闪。

本轮没有烧写；以上实板步骤尚未执行。

目录约定更新（2026-10-28）：App/Board 是后加的手写模块；.c 位于 USER/src/App、USER/src/Board，.h 全部位于 USER/inc。根目录不再保留 App/Board。
