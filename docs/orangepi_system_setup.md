# 香橙派 3B 系统配置与外设点亮

本文记录 xBot 在 **Orange Pi 1.0.8 Jammy（Linux 6.6.0-rc5-rockchip-rk356x）** 上已验证的系统配置、设备节点差异，以及 bring-up 命令。硬件引脚见 [`pin_map.md`](pin_map.md)；底盘协议见 [`soc_mcu_protocol.md`](soc_mcu_protocol.md)。

| 项目 | 内容 |
|------|------|
| SOC | 香橙派 3B，RK3566 |
| 镜像 | Orange Pi 1.0.8 Jammy |
| 内核 | `6.6.0-rc5-rockchip-rk356x` |
| ROS | ROS2 Humble（与 Ubuntu 22.04 对齐） |
| 验证日期 | 2026-08-30 |
| 外设冒烟 | Overlay / SPI / UART2·9 / 相机 / **喇叭+USB麦** / STM32 / **M1C1 雷达** / LCD 已点亮；电机与 ROS2 节点待测 |

与 newbot 原厂文档的差异以**本机实测为准**（尤其串口设备名、相机 `/dev/video*`）。

---

## 1. 设备树 Overlay

### 1.1 查可用 overlay

```bash
ls /boot/dtb/rockchip/overlay/ | grep -E 'spi3|uart2|uart9'
cat /boot/orangepiEnv.txt
```

本镜像实测存在：

```text
rk356x-spi3-m0-cs0-spidev.dtbo
rk356x-uart9-m2.dtbo
```

**没有** `uart2-m0` 的 dtbo。UART2 已在主板 DTB 中默认打开（`/dev/ttyS2`、Pin8/10 为 ALT1）。

`overlay_prefix=rk356x` 已存在时，`overlays=` 里**不要**再写 `rk356x-` 前缀。

### 1.2 写入配置

`/boot/orangepiEnv.txt` 应包含（在已有字段后追加一行即可）：

```text
verbosity=1
bootlogo=true
extraargs=cma=128M
overlay_prefix=rk356x
rootdev=UUID=...
rootfstype=ext4
overlays=spi3-m0-cs0-spidev uart9-m2
```

命令示例：

```bash
# 确认没有 overlays= 行后再追加；若已有则改成下面这一行，勿重复
sudo sh -c 'echo "overlays=spi3-m0-cs0-spidev uart9-m2" >> /boot/orangepiEnv.txt'
cat /boot/orangepiEnv.txt
sudo reboot
```

newbot 旧文档写的是 `overlays=spi3-m0-cs0-spidev uart2-m0 uart9-m2`。本镜像**不要**写不存在的 `uart2-m0`。

### 1.3 重启后验收

```bash
grep overlays /boot/orangepiEnv.txt
ls -l /dev/ttyS2 /dev/spidev3.0 /dev/video0
ls -l /dev/ttyS*
gpio readall
dmesg | grep -iE 'overlay|spidev|fe6d0000|ttyS|spi3|failed'
```

预期 pinmux（`gpio readall`）：

| 物理脚 | 信号 | Mode |
|--------|------|------|
| 8 / 10 | RXD.2 / TXD.2 | ALT1（UART2） |
| 19 / 21 / 23 | SPI3_TXD / RXD / CLK | ALT4 |
| 22 / 29 | TXD.9 / RXD.9 | ALT4（UART9） |

已知现象：

- `spi_master spi3: Failed to create SPI device for .../spi_dev@0`：主 DTB 与 overlay 可能冲突，但 `/dev/spidev3.0` 仍可出现；Pin24 `SPI3_CS1` 可能仍为 IN，LCD 异常时再查片选。
- WiFi `brcmfmac` firmware 缺失：与串口/相机无关，可后处理。

---

## 2. 设备节点对照（本镜像）

| 用途 | 硬件 | Linux 节点 | 备注 |
|------|------|------------|------|
| ↔ STM32 | UART2 `fe660000` | **`/dev/ttyS2`** | 主 DTB 默认开启 |
| ↔ 激光雷达 | UART9 `fe6d0000` | **`/dev/ttyS0`** | 不是 newbot 的 `/dev/ttyS9` |
| LCD SPI | SPI3 | **`/dev/spidev3.0`** | |
| LCD D/C | GPIO4_A0 | sysfs **gpio128** | |
| LCD RES | TXD.7 作 GPIO | sysfs **gpio130** | |
| 雷达 CTL | GPIO3_D4 | wPi **20** / GPIO 124 | |
| USB 相机 | HBVCAM UVC | **`/dev/video3`**（采集） | `video0` 是 RGA，勿用 |
| USB 麦 | C-Media PCM2902 | PulseAudio / ALSA | `08bb:2902`；录放已验证 |
| 喇叭 | RK809 → 3.5mm | PulseAudio sink | |

确认 UART9 映射（本机实测）：

```bash
# status 应为 okay
cat /proc/device-tree/serial@fe6d0000/status

# dmesg 关键：
# fe6d0000.serial: ttyS0 at MMIO 0xfe6d0000 ...
dmesg | grep -iE 'fe6d0000|ttyS'
```

ROS / 脚本中雷达口写 **`/dev/ttyS0`**，相机写 **`/dev/video3`**（插拔后编号可能变，以 `v4l2-ctl --list-devices` 为准）。

---

## 3. 设备权限（临时）

重启后可能需重新设置：

```bash
sudo chmod 666 /dev/spidev3.0 /dev/ttyS0 /dev/ttyS2
```

LCD GPIO（与 newbot `rc.local` 类似）：

```bash
sudo chmod 777 /sys/class/gpio/export
echo 128 | sudo tee /sys/class/gpio/export
echo 130 | sudo tee /sys/class/gpio/export
sudo chmod 666 /sys/class/gpio/gpio128/value /sys/class/gpio/gpio128/direction
sudo chmod 666 /sys/class/gpio/gpio130/value /sys/class/gpio/gpio130/direction
```

正式环境建议用 udev 规则，避免每次 `chmod`。用户加入 `dialout` / `video` 组也可减少串口/相机权限问题：

```bash
sudo usermod -aG dialout,video orangepi
# 重新登录后生效
```

---

## 4. 串口 Console 与 ttyS2

当前系统常见占用：

1. **`serial-getty@ttyS2`**（自动登录 shell）——必须先停，否则串口被多开、读会报 `multiple access`
2. cmdline **`console=ttyS2,1500000`**——与底盘协议 **115200** 冲突；长期应去掉

联调前立刻释放端口（本次有效，重启后 getty 可能再起来）：

```bash
sudo systemctl stop serial-getty@ttyS2
# 可选：开机不再占口
sudo systemctl disable serial-getty@ttyS2
```

去掉 kernel console（需改 env 并 reboot）：

```bash
cat /proc/cmdline | tr ' ' '\n' | grep console
sudo nano /boot/orangepiEnv.txt   # 按镜像说明改 console / extraargs
sudo reboot
```

改完后确认 cmdline 无 `console=ttyS2`；板载调试串口不可用，继续用 SSH。
---

## 5. 外设冒烟命令

### 5.1 USB 相机

```bash
lsusb
# 期望：058f:3822 ... HBVCAM CAMERA

v4l2-ctl --list-devices
# 期望：HBVCAM CAMERA → /dev/video3 /dev/video4
# video0=RGA，video1/2=VPU，不要当相机用

v4l2-ctl -d /dev/video3 --list-formats-ext
# 期望：MJPG 1280x720 @ 30fps（及 1280x800 等）

# 抓一帧到当前目录 test.jpg
v4l2-ctl -d /dev/video3 \
  --set-fmt-video=width=1280,height=720,pixelformat=MJPG \
  --stream-mmap --stream-count=1 --stream-to=test.jpg
ls -lh test.jpg
file test.jpg
```

若 `lsusb` 无相机且 `dmesg` 有 `usb usb6-port1: unable to enumerate`：换口/换线/查壳内 USB 转接，属硬件问题。

可选预览：

```bash
ffplay -f v4l2 -input_format mjpeg -video_size 1280x720 -i /dev/video3
```

### 5.2 音频（喇叭 / 耳机 / USB 麦）

#### 本镜像要点（Jammy 6.6 实测）

| ALSA | 设备 | 说明 |
|------|------|------|
| **card 0** | HDMI | 默认 Pulse sink `AudioCodec-Playback` 往往指这里（描述却可能显示 Headphone） |
| **card 2** | Analog **RK809** | **3.5mm 耳机孔**；机器人喇叭也走此 Codec → 控制板功放 |

MP3 可用 `play`（需 `sox` + `libsox-fmt-mp3`），**不能**用 `aplay` 直接播 MP3。

```bash
sudo apt install -y sox libsox-fmt-mp3
aplay -l
# card 0 = HDMI；card 2 = Analog RK809
```

#### 手动切到 3.5mm（耳机）

```bash
amixer -c 2 set Master 100% unmute
amixer -c 2 set 'Playback Mux' HP          # 耳机；喇叭用 SPK
pactl load-module module-alsa-sink device=hw:2,0 sink_name=rk809_analog \
  sink_properties=device.description=RK809_3.5mm
pactl set-default-sink rk809_analog
pactl set-sink-volume rk809_analog 100%
pactl set-sink-mute rk809_analog 0

speaker-test -D pulse -t sine -f 440 -c 2 -l 1
play ~/test.mp3
```

`Playback Mux`：

| 值 | 用途 |
|----|------|
| `HP` | 香橙派 3.5mm 插耳机调试 |
| `SPK` | 3.5mm → 控制板 HP/SP → 功放 → 喇叭 |

#### 仓库脚本（推荐）

源码：`software/scripts/orangepi_audio_setup.sh`  
开机安装：`software/scripts/install_orangepi_audio_service.sh`

在香橙派上（同步 `software/scripts` 后）：

```bash
cd ~/xbot_ws/src/../scripts 2>/dev/null || cd ~/xbot_ws/../software/scripts
# 若用 sync 脚本只拷了 src，请把 scripts 一并拷到板子，例如：
# scp -r F:\Rep\EE\xBot\software\scripts orangepi@IP:~/xbot_ws/

chmod +x orangepi_audio_setup.sh install_orangepi_audio_service.sh
./orangepi_audio_setup.sh HP              # 立即生效：耳机
# ./orangepi_audio_setup.sh SPK           # 机器人喇叭

# 安装为用户 systemd 开机任务（默认 HP；喇叭机用 SPK）
./install_orangepi_audio_service.sh HP
# ./install_orangepi_audio_service.sh SPK
```

安装后会：

- 复制 setup 到 `~/.local/bin/orangepi_audio_setup.sh`
- 启用 `~/.config/systemd/user/xbot-audio.service`
- 尽量 `loginctl enable-linger`（无桌面登录也能起用户服务）

检查：

```bash
systemctl --user status xbot-audio.service
pactl info | grep Default
# Default Sink 应为 rk809_analog
```

#### USB 麦克风（C-Media PCM2902）— 已验证 2026-08-30

与控制板 `MC` 座上的 **CI-03T 离线语音麦**不是同一路：CI 只给 STM32；云端 / 本地录音走香橙派 **USB 麦**。

| 检查 | 命令 / 期望 |
|------|-------------|
| USB 枚举 | `lsusb` 见 `08bb:2902`（C-Media） |
| ALSA | `arecord -l` 有 USB Audio 卡 |
| Pulse 默认源 | `orangepi_audio_setup.sh` 优先选 USB 源并 unmute |
| 录放 | `orangepi_mic_test.sh` 录完后耳机/喇叭能听到自己的声音 |

冒烟（推荐）：

```bash
cd ~/xbot_ws/scripts
chmod +x orangepi_audio_setup.sh orangepi_mic_test.sh
./orangepi_audio_setup.sh HP          # 设默认 sink=rk809_analog、USB source
./orangepi_mic_test.sh                # 对着麦说话约 3s，随后自动回放
# ./orangepi_mic_test.sh --seconds 5 --mux SPK
```

脚本把录音写到 **`/tmp/xbot_mic_test.wav`**。事后单独回放：

```bash
bash ~/xbot_ws/scripts/orangepi_audio_setup.sh HP   # 若刚开机、sink 还是 HDMI
paplay /tmp/xbot_mic_test.wav
# 或：paplay --device=rk809_analog /tmp/xbot_mic_test.wav
# 或：play /tmp/xbot_mic_test.wav
```

手动录放等价：

```bash
lsusb | grep -i 08bb
pactl list short sources
# Default Source 应为 USB（名称含 usb / C-Media），不要是 *.monitor
parecord --file-format=wav /tmp/mic.wav &
sleep 3; kill %1
paplay /tmp/mic.wav
```

无声排查：插紧 USB；先跑 `orangepi_audio_setup.sh`；`pactl set-source-mute … 0`；确认 Default Sink 是 **`rk809_analog`**（不是 HDMI `AudioCodec-Playback`）；`Playback Mux` 选 `HP`（耳机）或 `SPK`（喇叭）。

### 5.3 激光雷达串口

电源由 STM32 `PB4`（`CmdData.enable_power`）控制；未开电源时读口为空属正常。

**M1C1_Mini** 需要全双工 UART（不要按 YDLIDAR 只接 CTL）：

| 线 | 接法 |
|----|------|
| 红 5V | 控制板雷达电源（`enable_power`） |
| 黑 TX | **Pin29** `RXD.9`（数据入 SOC） |
| 绿 RX | **Pin22** `TXD.9`（SOC 发 `A5 F0` / `A5 F5`） |
| GND | GND |

YDLIDAR 绿线才接 Pin31 CTL；M1C1 绿线接 Pin31 时能转也可能**无点云**。

```bash
# 先保持 enable_power=1（见 §5.5），再：
stty -F /dev/ttyS0 115200 raw -echo
printf '\xa5\xf0' > /dev/ttyS0          # M1C1 开转
timeout 2 cat /dev/ttyS0 | xxd | head   # 期望有数据；流中可见 M1C1_Mini / AA 55
```

开电源后应有二进制流。

### 5.4 LCD SPI

```bash
ls -l /dev/spidev3.0
# D/C=gpio128 RES=gpio130；用户态 GC9A01，无内核屏驱动
```

仓库测屏脚本（纯色轮换，不依赖 OpenCV）：

```bash
# 同步 scripts 后
pip3 install --user python-periphery
cd ~/xbot_ws/scripts
sudo python3 orangepi_lcd_test.py              # 红→绿→蓝→白→黑，每色约 1s
sudo python3 orangepi_lcd_test.py --hold 2 --loop 2
sudo python3 orangepi_lcd_test.py --colors red blue
```

成功时屏上应依次铺满对应颜色；终端打印 `lcd init ok` / `fill red ...`。  
若花屏/全黑：查 SPI overlay、接线、Pin24 CS，以及 `dmesg` 中 spi 报错。

### 5.5 STM32（`/dev/ttyS2`）

不需要 ROS2。用仓库脚本按协议收 `McuData`、发 `CmdData`（开/关雷达电源）：

```bash
sudo apt install -y python3-serial
# 见 §4：先停 getty，再开串口
sudo systemctl stop serial-getty@ttyS2
sudo chmod 666 /dev/ttyS2
python3 ~/xbot_ws/scripts/orangepi_stm32_uart_test.py
# 默认：听 3s → 雷达开 5s → 关 2s；每 20ms 收发
python3 ~/xbot_ws/scripts/orangepi_stm32_uart_test.py --listen 2 --radar-on 6 --radar-off 2
```

成功时打印编码器 / 电压 / IMU 等；`enable_power=1` 时 STM32 `PB4` 开雷达 5V。  
也可在 PC 用 `firmware/tools/soc_console`（USB-UART ↔ 控制板 USART1）。

板上直接看原始字节：

```bash
stty -F /dev/ttyS2 115200 raw -echo
timeout 2 cat /dev/ttyS2 | xxd | head
# 期望周期性出现头 44 41 ('DA') 与尾 54 41 0d 0a ('TA\r\n')，帧长 29
```

---

## 6. ROS2 源码同步与板上编译

物理机推送 `software/src`（不要拷 x86 的 `install/`）：

```powershell
cd F:\Rep\EE\xBot
.\software\scripts\sync-to-orangepi.ps1
# 或指定 IP：
# .\software\scripts\sync-to-orangepi.ps1 -HostName 192.168.1.14
```

香橙派：

```bash
sudo apt install -y python3-colcon-common-extensions   # 若尚未装全
source /opt/ros/humble/setup.bash
cd ~/xbot_ws
colcon build --symlink-install --parallel-workers 1
source install/setup.bash
ros2 run xbot_bringup bringup_node
file install/xbot_bringup/lib/xbot_bringup/bringup_node
# 应为 ARM aarch64
```

开发机（Ubuntu 22 虚拟机）只验证 Humble 能编过；真机产物必须在 aarch64 上 `colcon build`。

---

## 7. Bring-up 检查表

状态列以本仓库 2026-08-30 板载实测为准。

| 步骤 | 检查项 | 通过标准 | 状态 |
|------|--------|----------|------|
| 1 | Overlay | `spi3-m0-cs0-spidev uart9-m2`；pinmux ALT 正确 | OK |
| 2 | SPI | `/dev/spidev3.0` | OK |
| 3 | UART2 | `/dev/ttyS2`；Pin8/10 ALT1 | OK |
| 4 | UART9 | `fe6d0000` → **`/dev/ttyS0`**；Pin22/29 ALT4 | OK |
| 5 | 相机 | HBVCAM → **`/dev/video3`**；能抓图 | OK |
| 6 | 喇叭 | sink=`rk809_analog`；`Playback Mux` HP/SPK；`play`/`paplay` 有声 | OK |
| 6b | USB 麦 | `08bb:2902`；`orangepi_mic_test.sh` 录放有声 | OK |
| 7 | Console / getty | 联调底盘前 `stop serial-getty@ttyS2` | 临时处理；长期去 cmdline |
| 8 | STM32 | `orangepi_stm32_uart_test.py` 约 50 Hz `McuData` | OK |
| 9 | 雷达 M1C1 | `enable_power`；绿线 **Pin22**；`ttyS0` 有数据 | OK |
| 10 | LCD | `orangepi_lcd_test.py` 刷色 | OK |
| 11 | 电机 / 编码器 | 小 PWM，编码器变化 | 待测 |
| 12 | ROS2 | 板上 `colcon` + `ros2 run`（aarch64） | 待业务节点 |

---

## 8. 相关文档

| 文档 | 内容 |
|------|------|
| [`pin_map.md`](pin_map.md) | 引脚与互连 |
| [`soc_mcu_protocol.md`](soc_mcu_protocol.md) | SOC ↔ MCU 帧 |
| [`newbot_ros1_architecture.md`](newbot_ros1_architecture.md) | 原厂 ROS1 节点（ROS2 移植基线） |
| `firmware/tools/soc_console/README.md` | PC 模拟 SOC 联调 |
| `software/README.md` | ROS2 工作空间与脚本入口 |
| `software/scripts/sync-to-orangepi.ps1` | 源码/脚本同步到香橙派 |
| `software/scripts/orangepi_mic_test.sh` | USB 麦录放冒烟 |
