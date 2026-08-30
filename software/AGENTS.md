# software AGENTS

本文件约束 **香橙派 / ROS2 Humble**（`software/`）相关改动。仓库总览见根目录 `AGENTS.md`；STM32 固件见 `firmware/AGENTS.md`。

跨端事实源（只引用，不在此复制全文）：

- `docs/pin_map.md` — 引脚与板级连接
- `docs/soc_mcu_protocol.md` — SOC ↔ MCU 帧协议
- `docs/orangepi_system_setup.md` — Jammy 镜像设备节点、overlay、外设冒烟
- `docs/newbot_ros1_architecture.md` — 原厂 ROS1 行为（移植基线）

---

## 范围

- **做**：`software/src` ROS2 包、`software/scripts` 板级脚本、本目录 README；与 SOC 侧协议解析一致的代码
- **不做**：`firmware/` 固件与 CubeMX；手改 `ref/`；在未确认硬件前改 `docs/` 中的引脚/节点事实
- **联调脚本**：优先放 `software/scripts/`，保持**不依赖 ROS2**（外设点亮先于 `colcon`）

---

## 本镜像设备节点（硬约束）

Jammy 6.6 / 香橙派 3B 以 `docs/orangepi_system_setup.md` 为准，常见坑：

| 用途 | 正确节点 | 勿用 |
|------|----------|------|
| STM32 | `/dev/ttyS2` | — |
| 雷达 UART9 | `/dev/ttyS0` | newbot 的 `/dev/ttyS9` |
| 相机采集 | `/dev/video3`（HBVCAM） | RGA `/dev/video0` |
| LCD | `/dev/spidev3.0` + gpio128/130 | — |
| 喇叭 | Pulse sink `rk809_analog`（ALSA card **2**） | HDMI card0 |
| USB 麦 | Pulse **source**（`08bb:2902`） | 控制板 `MC`（那是 CI-03T） |

Overlay：`overlays=spi3-m0-cs0-spidev uart9-m2`（**不要**写不存在的 `uart2-m0`）。

雷达接线：

- **M1C1**：绿线 → **Pin22** `TXD.9`；黑线 → Pin29；电源 `CmdData.enable_power`
- **YDLIDAR**：绿线 → Pin31 CTL

---

## ROS2 包约定

- 工作空间在板上一般为 `~/xbot_ws`；Windows 用 `scripts/sync-to-orangepi.ps1` 同步 `src/` + `scripts/`
- 包名 `xbot_*`；消息/服务命名尽量对齐 `docs/newbot_ros1_architecture.md` 中的话题，便于逐项移植
- 底盘帧：`McuData` 29 B / `CmdData` 12 B，与 `docs/soc_mcu_protocol.md` 一致；改布局必须同步文档与固件
- 真机二进制在 **aarch64** 上 `colcon build`；勿把 x86 VM 的 `install/` 拷到板子

---

## 脚本

| 脚本 | 用途 |
|------|------|
| `sync-to-orangepi.ps1` | PC → 板：`src` + `scripts` |
| `orangepi_audio_setup.sh` | RK809 3.5mm + USB 麦默认源 |
| `install_orangepi_audio_service.sh` | 用户 systemd 开机音频路由 |
| `orangepi_mic_test.sh` | USB 麦录放冒烟 |
| `orangepi_lcd_test.py` | GC9A01 刷色 |
| `orangepi_stm32_uart_test.py` | 收 McuData / 雷达电源 |

板载联调前：`sudo systemctl stop serial-getty@ttyS2`（否则占 STM32 口）。

---

## 风格与改动原则

- 匹配现有包布局与命名；只改任务相关代码
- 外设点亮脚本用 bash/Python + 系统工具，避免为冒烟引入 ROS 依赖
- 不把固件/Cube 代码放进本树；参考实现可看 `ref/newbot`，移植时按本工程节点与设备名适配

---

## 自检（改 software 后）

- [ ] 设备节点与 `docs/orangepi_system_setup.md` 一致（尤其 `ttyS0` / `video3` / `rk809_analog`）
- [ ] 协议字段变更已同步 `docs/soc_mcu_protocol.md` 与固件侧
- [ ] 板载脚本仍可在无 ROS 环境下单独跑通冒烟项
- [ ] `sync-to-orangepi.ps1` 能推到板；板上 `colcon` 产物为 aarch64
