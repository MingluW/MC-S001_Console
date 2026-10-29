# CAN FD首版：KPH目标速度与串口收发日志

日期：2026-10-28。已编码、编译及软件模拟验证；本版未烧写，未验证实板CAN或电机动作。
用户要求：暂不做心跳，先接开始、速度加减，初始0.3 KPH；停止优先写速度0。

## 当前行为

- 开机只初始化CAN，不发送运行目标或心跳。保留原开机动画及等待START流程。
- 按开始时保存并发送0.3 KPH目标，未构造文档没有明确规定的独立使能/START报文。
  UI仍先显示坡度0，按当前已有APP_START_INCLINE_MS=1000停留后显示速度0.3；此参数未改回500。
- V+/V-每次修改0.1 KPH，范围沿用当前0.0~30.0配置；这是上控配置，不保证下控全范围支持。
- 停止优先写0.0，撤销未提交非零目标，并请求取消仍在硬件等待的旧请求；确认取消前不覆盖消息RAM。
  失败的STOP目标保留，每500ms再尝试写零。普通非零目标失败不自动重试。
- UI显示的是请求目标；不把发送成功、FP2 ACK或屏幕变化当作实际电机速度/停止确认。
- 第一个目标应答后订阅302当前KPH、2108控制器错误、2380电机错误，便于看返回值。
  订阅不支持时记录Error；厂商故障值先打印原始float位型，不猜过流/过压数字映射。
- 不发送坡度控制、心跳、固件升级；未增加实际下控固件没有确认的其他运行命令。
- 当前对接固定KPH。现有MPH选项仍是此前的显示演示接口，不参与发送值换算；暂无界面切换入口。

## 硬件与配置

Board手工初始化FDCAN1，PA15/AF25为TX、PB3/AF26为RX，对应原理图U307收发器。
TX为复用推挽输出，RX为上拉输入并保留AF26路由；2026-10-29修复RX误用输出模式，详见CAN_DEBUG_20261029.md。
上控使用11位标准FD+BRS数据帧，ISO模式；接收全部标准数据帧用于联调，拒绝扩展及远程帧。
轮询Rx FIFO0（16个64字节元素），每轮服务最多4帧，无新增CAN中断。专用TX缓冲只有1个。
静态SRAM消息RAM为17*18个32位字=1224字节，按4字节对齐，未放CCM或堆栈。

时钟沿用生成的主PLL240MHz，FDCAN从主PLL除6得到40MHz；初始化核对当前系统为主PLL240MHz。
标称：Prescaler1、Seg1=139、Seg2=20、SJW20 → 250kbps，采样点87.5%。
数据：Prescaler1、Seg1=14、Seg2=5、SJW4 → 2Mbps，采样点75%。
采用总线PDF正文SJW4，未照抄矛盾的SocketCAN dsjw8。使用现有HSI PLL，电气时序仍需台架验证。
SDK初始化为有界等待，运行中不等待TX/RX完成；硬件自动重发关闭，避免非零旧目标无限重传。
TX等待200ms未完成则请求取消，未确认时保持BUSY并记录TX_TIMEOUT，不复用缓冲。
Bus-off不自动恢复/重发运行目标；诊断日志报告状态，需复位或明确重新初始化检查。

节点配置在USER/inc/powerboard.h：按总线规范速度控制器索引1，发送201、接收241。
该索引是文档默认配置，尚未从实板广告/现有下控固件确认。实际不同需调整，不能将无回应直接判硬件坏。

## 小数速度与报文

App内部以整数speed_x10保存KPH的10倍：3表示0.3 KPH，组包时转换为float32小端。
FP2发送者为Console Brainboard（高半字节1），Write命令2，目标Feature301（十进制，0x012D）。

0.3 KPH的完整CAN数据区（12字节）为：

```text
08 12 06 2D 01 9A 99 99 3E 00 00 00
```

08是iCANt模块数据长度；12 06是FP2头；2D 01为Feature301；9A 99 99 3E为float32的0.3。
有效CAN载荷9字节，DLC只能向上取12，尾部3个00为填充，不算进iCANt长度。
停止报文为：08 12 06 2D 01 00 00 00 00 00 00 00。
速度写入不会附加旧串口AA/55、转义、SEQ或CRC16。

只允许一个FP2事务等待应答。硬件TX完成后开始等应用ACK/Error，默认500ms；这是联调配置，不是文档规定。
ACK仅携带命令，不能精确辨别Feature/事务；STOP抢占时可能有旧ACK迟到，因此日志不将ACK绑定为特定速度已执行。
目标超时/拒绝后禁止继续普通非零目标，明确STOP再START可重新发起；没有自动恢复之前运行速度。
收到的iCANt有效字节按本节点FP2流重组，完整FP2最多257字节，残帧200ms超时丢弃，拒绝无效长度及NaN/Inf。

## 串口查看

USART2调试口115200、8N1，3.3V TTL。CN501的1脚GND、3脚DBG_TX接USB转串口RX；
需要输入时USB转串口TX接2脚DBG_RX。方向和供电说明见UART_LOG.md。
日志级别默认DBG，心跳/字符回显不启用，不需要另加printf轮询任务。

日志示例（仅格式示例，不是本板实测抓包）：

```text
[FP2 WRITE_REQ] #1 feature=301 target=0.3 KPH
[CAN TX_REQ] #1 id=201 fd=1 brs=1 len=12 @0 08 12 06 2D 01 9A 99 99 3E 00 00 00
[CAN TX_SENT] #1 id=201 fd=1 brs=1 len=12 @0 08 12 06 2D 01 9A 99 99 3E 00 00 00
[CAN RX] #0 id=241 fd=1 brs=1 len=4 @0 03 23 01 12
[FP2 ACK] cmd=12 error=0 match=1; ACK has no feature/sequence
[FP2 EVENT] current speed=0.3 KPH bits=3E99999A
```

- TX_REQ：数据已复制到CAN硬件并提交请求；本地#编号仅用于日志，不写入协议。
- TX_SENT：硬件报告发送完成，仍需FP2应答及实际反馈。CAN层应答不等于指定下控已执行。
- RX：实际从接收FIFO读出的原始ID/FD/BRS/DLC长度/全部数据，包含尾部填充。
- FP2 ACK/ERROR/EVENT：校验结构和发送者后解析；match仅表示当前等待命令相符。
- TX_FAIL/TX_TIMEOUT：没有确认完成总线传输；FP2 TIMEOUT：总线发送后没有收到应用应答。
- CAN STATUS：Bus-off、TEC/REC、最近错误、丢帧/日志丢失统计，不应与电机过流/过压混淆。
  新增RX输入电平、ACT（0同步/1空闲/2接收/3发送）、CCCR原始值；LEC/DLEC均显示解码后的0~7。
  DLEC=7是无新错误，不是错误计数。

原始帧日志用16条有界队列，每轮输出最多32数据字节。64字节分@0、@32两行；
UART满时留待下轮，原始日志队列满丢新并报告数量，不阻塞CAN、按键和LCD。
解析摘要日志满时可丢弃并计数；时间前缀为日志输出时刻。持续大流量下不能保证每条调试日志都保留。

## 代码阅读与检查

- USER/src/Board/board_can.c：时钟、GPIO、消息RAM、SDK收发与状态。
- USER/src/Modules/powerboard/fp2.c：纯编解码，显式端序及float位转换。
- 同目录powerboard.c：目标合并、单事务、应答/分片、停止优先。
- 同目录can_trace.c：有界原始帧日志；不阻塞通信。
- USER/src/App/app_powerboard.c：从AppState的开始/速度/停止变化发起请求，不发送坡度。
- 新头文件统一在USER/inc，Makefile显式加入powerboard目录和SDK fdcan.c；VS Code原include路径已覆盖。

构建：在Makefile目录make。软件检查：工程根目录python Tests/test_can.py、python Tests/test_ui_flow.py。
依赖与原ARM模拟测试相同。CAN测试执行实际ARM ELF、SDK初始化/消息RAM/收发操作、FP2与UART；
只有外设寄存器的硬件副作用及输入帧由测试模拟。覆盖0.0~30.0逐值浮点编码、DLC、FIFO、
取消/超时/计时回绕、Bus-off、ACK/Error/错节点、残帧、停止抢占及重试、日志拥塞和App到CAN接入。
这不能替代CAN波形、收发器电气、下控协议兼容性、0.3实际运行及安全锁/失联行为验证。

最新版本没有烧写；要验证时使用make download，然后用串口工具观察，不在开机阶段自动启动电机。
