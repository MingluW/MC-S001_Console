# CAN 内部回环实板验证（2026-10-29）

## 结论与当前状态

按用户要求，先将原工程提交并推送 GitHub main：`5b28e2823c930e0a2c3bafa516bf72c22ac9180a`。
随后临时烧写内部回环测试固件。原发送状态查询逻辑暴露竞态；临时调整查询顺序后，
三次独立复位运行均通过 110/110 帧，共 330 帧逐字节一致，TEC/REC 均为 0。
**这不是原代码未经修正即可通过的结论。**

测试结束已恢复该提交的全部固件源码并重新编译、下载、verifybin 校验和复位运行。
当前为正常 CAN 模式，不留回环入口或临时测试文件，也不保留临时竞态修正。
后续正式联调前应单独修复并回归该竞态，不能因回环通过而忽略它。

## 测试方法和覆盖

- 使用现有 N32H473CC 实板及 J-Link，SWD 100kHz；未接下控，不短接引脚。
- FDCAN1 `FDCAN_MODE_INTERNAL_LOOPBACK`，控制器内部返回发送数据，保持 250kbps/2Mbps、ISO FD+BRS。
- 临时 AppCanLoopback 替代 AppPowerboard 服务，使用真实 BoardCan、SDK、消息 RAM、发送完成位及 RX FIFO；没有模拟外设。
- 初始化后等待 1000ms；每帧等待完成/接收，100ms 超时。所有等待采用主循环状态机。
- 每轮 110 帧，17种用例循环：FD长度 0/1/2/3/4/5/6/7/8/12/16/20/24/32/48/64，加一条0.3 KPH真实FP2组包。
- 标准 ID 覆盖 0、0x7FF 和多个中间值；其余数据按 `(frame_index * 17 + byte_index * 29) ^ 0xA5` 生成。
- 0.3 KPH：ID 0x201，12字节 `08 12 06 2D 01 9A 99 99 3E 00 00 00`。
- 校验 ID、DLC长度、标准帧、FD/BRS标志和全部有效数据；最后检查FIFO无额外帧。
- 结果存放实际 MCU SRAM，经 SWD 读取，未依赖串口截图推断；串口输出也保留测试日志入口。

## 首轮发现：发送完成查询竞态

保留原查询逻辑的首轮：前7帧收发匹配，第8帧（ID 0x208、长度7）被软件报告TX_FAILED。
随后直接读硬件：TXBRP=0、TXBTO=1、TXBCF=0、RXF0S=0x00080701，接收FIFO仍有该帧。
TEC=REC=0。因此证据指向软件误判完成状态，而非该帧未完成硬件发送。

原顺序先读 TXBTO，再读 TXBRP。硬件若在两次读取之间完成发送，可能看到旧完成位0、新pending位0，
于是进入失败分支。临时验证改为先读pending：

```c
if (FDCAN_CheckTxBufRequest(BOARD_CAN_DEVICE, FDCAN_TX_BUFFER0) == RESET)
{
    /* pending清除后，再判定完成还是失败。 */
    if (FDCAN_CheckBufTxResult(BOARD_CAN_DEVICE, FDCAN_TX_BUFFER0) != RESET)
    {
        tx_state = BOARD_CAN_TX_SENT;
    }
    else
    {
        tx_state = BOARD_CAN_TX_FAILED;
        ++stats.tx_failed;
    }
}
else if (/* 原200ms超时条件 */)
{
    /* 原取消流程 */
}
```

此处为说明性片段；临时实测源码和固件存于本机临时备份目录，未合入生产代码。

## 临时修正后的实测

| 运行 | TX完成 | RX | 完整匹配 | TX/RX摘要 | TEC/REC |
| --- | ---: | ---: | ---: | --- | --- |
| 首次下载运行 | 110 | 110 | 110 | 3550AC70 / 3550AC70 | 0 / 0 |
| 第一次复位 | 110 | 110 | 110 | 3550AC70 / 3550AC70 | 0 / 0 |
| 第二次复位 | 110 | 110 | 110 | 3550AC70 / 3550AC70 | 0 / 0 |

每轮phase=2、failure=0、CCCR=0x3E0，从初始化到结束1024ms（包含开头1000ms等待）。
摘要仅作记录，测试同时逐帧逐字节比较，不只比较摘要。

## 恢复核验

- USER/src/Board/board_can.c、USER/src/App/app.c 与基线SHA256一致；USER整体git diff为空。
- 已删除临时 app_can_loopback.c/.h，恢复 AppPowerboard 正常服务及 FDCAN_MODE_NORMAL。
- 原BIN和恢复后BIN：33864字节，SHA256均为
  `2d86e66e33f34ba562ffad7a9fb7bebd30e282780a7bd9bb8fb8a6770f768d3b`。
- 实板重新下载并verifybin成功，复位运行后CCCR=0x340、TEST=0x80（RX高，LBCK位为0）。
- 恢复后构建成功，Tests/test_can.py通过；既有未使用自测函数、nosys及RWX警告保留。
  RX读取API测试因回显关闭被链接器移除，仍为SKIP。软件测试未模拟上述两次寄存器读取之间硬件完成的时刻。

原始读回日志、首轮失败及三轮通过结果、恢复摘要见 [验证数据](Validation/can_loopback_20261029/)。
本机完整临时源码/固件备份路径：
`C:/Users/mingluw/AppData/Local/Temp/mcs001-can-loopback-20261029-tuviyfoq`。

内部回环验证了控制器内部收发、消息RAM和软件取帧路径；未验证外部引脚、收发器、CANH/CANL、
终端、电气波形、对端位时序和FP2应答，更不能据此声明下控或电机运行已经验证。
