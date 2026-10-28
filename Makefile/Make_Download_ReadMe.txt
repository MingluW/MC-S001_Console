一、Make 命令
1.编译
      make :编译,带debug信息；
      make release=1:编译,不带debug信息；
      
2.下载
      make download_jlink:JLINK下载；
      make download_cmsisdap:CMSIS_DAP下载；

3.调试
      make debug_jlink: JLINK调试
      make debug_cmsisdap: CMSIS_DAP调试

二、更改CMSIS-DAP 调试器配置

1.更换调试器设备
若使用的 CMSIS-DAP 调试器非默认的 nslink，需修改配置文件 XXXXXX_cmsisdap.cfg 中的 VID/PID：

    # 修改前（默认 nslink）
     set ADAPTER_VID 0x19F5
     set ADAPTER_PID 0x3106

     # 修改后（替换为您的设备 VID/PID）
     set ADAPTER_VID <您的VID>
     set ADAPTER_PID <您的PID>

2.调整通信速率
在 XXXXXX_cmsisdap.cfg 中修改 adapter speed 参数：

    # 默认 1MHz，可根据实际连线质量调整
    adapter speed 1000

三、NSLink下载失败的应急尝试
建议优先尝试以下操作：
1.断开 NRST 引脚：排除复位电路干扰。
2.降低通信速率：将 adapter speed 调低，增强信号稳定性