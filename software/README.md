# xBot software（ROS2 + 香橙派）

香橙派 3B 上的 **ROS2 Humble** 工作空间与板级联调脚本。底盘 STM32 固件在 `firmware/`；硬件与协议文档在 `docs/`。

## 目录

```text
software/
  src/                 # ROS2 包（板上 ~/xbot_ws/src）
  scripts/             # 同步与外设冒烟（可不依赖 ROS）
  AGENTS.md            # 本树改动约定
  README.md
```

## 环境

| 项 | 值 |
|----|-----|
| 板 | 香橙派 3B（RK3566） |
| 系统 | Orange Pi Jammy，内核 6.6.x |
| ROS | Humble |
| 工作空间 | 板上 `~/xbot_ws` |

系统 / overlay / 设备节点差异见 [`docs/orangepi_system_setup.md`](../docs/orangepi_system_setup.md)。

## 从 Windows 同步到板

```powershell
cd F:\Rep\EE\xBot
.\software\scripts\sync-to-orangepi.ps1
# .\software\scripts\sync-to-orangepi.ps1 -HostName 192.168.1.14
# .\software\scripts\sync-to-orangepi.ps1 -ScriptsOnly
```

会推送 `software/src` → `~/xbot_ws/src`，`software/scripts` → `~/xbot_ws/scripts`。

## 板载编译（aarch64）

```bash
source /opt/ros/humble/setup.bash
cd ~/xbot_ws
colcon build --symlink-install --parallel-workers 1
source install/setup.bash
ros2 run xbot_bringup bringup_node
```

不要把 x86 虚拟机编出的 `install/` 拷到板子。

## 外设冒烟脚本（无需 ROS2）

先保证 overlay 与权限，见 [`docs/orangepi_system_setup.md`](../docs/orangepi_system_setup.md)。板上已点亮：相机、LCD、STM32、M1C1 雷达、**喇叭 + USB 麦**。

```bash
cd ~/xbot_ws/scripts
chmod +x *.sh

# 音频：3.5mm（HP）或喇叭（SPK）+ USB 麦为默认采集源
./orangepi_audio_setup.sh HP
./install_orangepi_audio_service.sh HP   # 开机保持
./orangepi_mic_test.sh                   # 录 3s 再自动回放（已验证）
# 事后回放：paplay /tmp/xbot_mic_test.wav

# LCD
sudo python3 orangepi_lcd_test.py

# STM32（先停串口 getty）
sudo systemctl stop serial-getty@ttyS2
python3 orangepi_stm32_uart_test.py
```

| 脚本 | 说明 |
|------|------|
| `orangepi_audio_setup.sh` | 默认 sink=`rk809_analog`，优先 USB 麦 source |
| `install_orangepi_audio_service.sh` | 用户 systemd 开机执行上述路由 |
| `orangepi_mic_test.sh` | USB 麦录放；产物 `/tmp/xbot_mic_test.wav` |
| `orangepi_lcd_test.py` | GC9A01 纯色轮换 |
| `orangepi_stm32_uart_test.py` | 读 `McuData`，开关雷达 5V |

**USB 麦**（`08bb:2902`）≠ 控制板 `MC` 上的 CI-03T。  
**M1C1 雷达**绿线接 **Pin22**（`TXD.9`），不是 Pin31。  
无声时先确认 Default Sink 不是 HDMI，再跑一遍 `orangepi_audio_setup.sh`。
## 相关文档

| 文档 | 内容 |
|------|------|
| [`docs/orangepi_system_setup.md`](../docs/orangepi_system_setup.md) | Overlay、节点、冒烟清单 |
| [`docs/pin_map.md`](../docs/pin_map.md) | 引脚 |
| [`docs/soc_mcu_protocol.md`](../docs/soc_mcu_protocol.md) | SOC ↔ MCU 帧 |
| [`docs/newbot_ros1_architecture.md`](../docs/newbot_ros1_architecture.md) | ROS1 移植基线 |
| [`AGENTS.md`](AGENTS.md) | 本目录 AI / 开发约定 |
