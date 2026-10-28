# MC-S001 Console

MC-S001上控板固件，目标芯片N32H473CCL7 / LQFP48。
N32Cube生成基础工程，使用ARM GCC、GNU Make和J-Link。

当前基线（2026-10-28）：LCD初始化已移植，固件已下载并校验，用户确认全部数字段与图标亮起。

## 当前功能

- PB2蓝灯非阻塞闪烁，亮灭各1500ms。
- USART2调试串口115200 8N1，printf及ERR/WARN/DBG分级日志。
- 日志显示运行天数与时分秒，发送队列由主循环推进。
- LCD GPIO模拟9位Mode3/MSB，初始化用时间状态机，每轮最多发送一个字。
- 当前LCD初始化后保持全亮，独立数字/图标和业务显示尚未加入。
- 调试串口回显、周期日志任务目前由用户注释关闭。

## 目录

```text
USER/src/App/          应用入口与任务调度
USER/src/Board/        当前板卡适配
USER/src/Modules/      LCD、日志等模块
USER/inc/              全部手写头文件及生成头文件
Driver/                N32Cube附带SDK，保留厂商声明
Makefile/              构建、链接、J-Link和CMSIS-DAP脚本
.vscode/               共享编译/调试配置
Tests/                 ARM指令模拟测试，不接入固件
Docs/                  规范、验证记录和项目状态
```

开发前阅读[工程约定](AGENTS.md)、[代码规范](Docs/CODING_STANDARD.md)、[项目状态](Docs/PROJECT_STATUS.md)。

## 编译、下载和复位

VS Code打开整个工程，Ctrl+Shift+B编译。
命令行进入Makefile目录：

```powershell
make
make download
make reset
```

- make：只编译，生成build中的ELF/HEX/BIN。
- make download：先构建，然后J-Link下载同一BIN并校验、复位运行。
- make reset：只复位并运行现有程序，不编译、不下载。

本机默认J-Link目录D:/Jlink/app/JLink，SWD 100kHz。
其他电脑安装工具后可覆盖路径：

```powershell
make GCC_PATH=D:/Nsing/Tools/ArmGNU/bin/
make download JLINK_PATH=D:/Jlink/app/JLink
```

GCC和Make需在PATH中，或使用绝对路径调用Make。VS Code目前含本机绝对路径，换电脑需调整。
Driver中SDK代码保留原版权与许可声明，不能将整份工程视为统一开源许可。

## 验证和限制

- [LCD点亮记录](Docs/LCD_BRINGUP.md)：引脚、时序、已确认现象、后续工作。
- [串口和日志](Docs/UART_LOG.md)：接线、接口和测试依赖。
- Tests/test_lcd.py执行实际ARM ELF中的状态机和GPIO输出逻辑，并回归串口日志测试。
- LCD测试会读取旧Cypress工程作命令序列参考；新电脑运行需具备相应只读参考路径。
- 编译产物、板上Flash备份和临时检查脚本不提交Git；所需flash_op下载算法保留。
- 当前保留SDK/nosys/RWX及已关闭演示的未使用函数警告，详见项目状态。

## 版本管理

当前准备作为第一个“LCD点亮基线”提交。功能开发按可编译、可验证的阶段独立提交。
已提交且共享的错误变更优先用git revert生成回退提交；不要默认用reset --hard丢弃本地工作。
私有GitHub仓库的创建和首次推送状态以Docs/PROJECT_STATUS.md及实际远端为准。
