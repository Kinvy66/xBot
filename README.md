# xBot

香橙派 3B + STM32F103 差速底盘机器人。SOC 跑 **ROS2 Humble**，MCU 跑 FreeRTOS 固件；两端用固定二进制帧通信。

## 仓库结构

```text
xBot/
  docs/          # 引脚、协议、板载配置、ROS1 架构（事实源）
  firmware/      # STM32F103 固件 + PC 联调工具
  software/      # ROS2 包 + 香橙派冒烟脚本
  ref/           # 原厂 newbot 等参考（只读对照）
  AGENTS.md      # 仓库总约定（AI / 协作者）
  README.md
```

| 目录 | 说明 |
|------|------|
| [`docs/`](docs/) | [`pin_map.md`](docs/pin_map.md)、[`soc_mcu_protocol.md`](docs/soc_mcu_protocol.md)、[`orangepi_system_setup.md`](docs/orangepi_system_setup.md)、[`newbot_ros1_architecture.md`](docs/newbot_ros1_architecture.md) |
| [`firmware/`](firmware/) | 见 [`firmware/README.md`](firmware/README.md)、[`firmware/AGENTS.md`](firmware/AGENTS.md) |
| [`software/`](software/) | 见 [`software/README.md`](software/README.md)、[`software/AGENTS.md`](software/AGENTS.md) |

## 硬件与软件栈

| 项 | 内容 |
|----|------|
| SOC | 香橙派 3B，RK3566，Orange Pi Jammy（内核 6.6.x） |
| MCU | STM32F103，底盘电机 / 编码器 / IMU / 雷达 5V MOS |
| ROS | ROS2 Humble（板上 aarch64 编译） |
| 通信 | USART，`McuData` 29 B / `CmdData` 12 B，周期 20 ms |

本镜像常用节点（详表见系统配置文档）：

| 用途 | 节点 |
|------|------|
| ↔ STM32 | `/dev/ttyS2` |
| 雷达 UART | `/dev/ttyS0`（UART9；M1C1 绿线接 **Pin22**） |
| 相机 | `/dev/video3`（勿用 RGA `video0`） |
| LCD | `/dev/spidev3.0` |
| 喇叭 | Pulse `rk809_analog` |
| USB 麦 | `08bb:2902`（≠ 控制板 CI-03T） |

## 快速入口

**板级点亮（无需 ROS）** — [`docs/orangepi_system_setup.md`](docs/orangepi_system_setup.md)：

```powershell
# Windows 同步脚本到板
.\software\scripts\sync-to-orangepi.ps1 -ScriptsOnly
```

```bash
# 香橙派
cd ~/xbot_ws/scripts
./orangepi_audio_setup.sh HP && ./orangepi_mic_test.sh
sudo python3 orangepi_lcd_test.py
sudo systemctl stop serial-getty@ttyS2
python3 orangepi_stm32_uart_test.py
```

**固件构建** — [`firmware/README.md`](firmware/README.md)：

```powershell
cd firmware\xBot\scripts
.\build.ps1          # Debug
.\build.ps1 -Flash   # 烧录
```

**ROS2** — [`software/README.md`](software/README.md)：

```powershell
.\software\scripts\sync-to-orangepi.ps1
```

```bash
source /opt/ros/humble/setup.bash
cd ~/xbot_ws && colcon build --symlink-install --parallel-workers 1
```

## Bring-up 进度（2026-08-30）

已点亮：overlay、SPI/LCD、相机、喇叭、USB 麦、STM32 帧、M1C1 雷达数据。  
待测：电机/编码器闭环、完整 ROS2 业务节点。清单见 [`docs/orangepi_system_setup.md`](docs/orangepi_system_setup.md) §7。

## 约定文档

- 总约定：[`AGENTS.md`](AGENTS.md)
- 固件：[`firmware/AGENTS.md`](firmware/AGENTS.md)
- 软件：[`software/AGENTS.md`](software/AGENTS.md)
