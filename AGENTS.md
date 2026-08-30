# xBot AGENTS

本文件是仓库**总约定**。改固件看 `firmware/AGENTS.md`；改香橙派 / ROS2 看 `software/AGENTS.md`。细节事实以 `docs/` 为准，勿在多处复制互相打架的副本。

---

## 项目是什么

双端移动机器人：

| 端 | 硬件 | 代码 |
|----|------|------|
| SOC | 香橙派 3B（RK3566）+ ROS2 Humble | `software/` |
| MCU | STM32F103 底盘控制板 | `firmware/xBot` |

SOC ↔ MCU：USART，帧协议见 `docs/soc_mcu_protocol.md`（`McuData` 29 B / `CmdData` 12 B，20 ms）。

原厂参考在 `ref/newbot`（只读对照，不要当主工程改）。

---

## 目录与职责

| 路径 | 职责 | 细则 |
|------|------|------|
| `docs/` | 引脚、协议、板载配置、ROS1 架构 | 跨端事实源 |
| `firmware/` | STM32 固件、PC `soc_console` | `firmware/AGENTS.md` |
| `software/` | ROS2 包、板级冒烟脚本 | `software/AGENTS.md` |
| `ref/` | 原厂 / 第三方参考 | **禁止当作产品代码修改或整树覆盖进主工程** |

---

## 跨端事实源（改代码前先读）

| 文档 | 内容 |
|------|------|
| `docs/pin_map.md` | SOC 40Pin、MCU 外设、雷达电源 PB4、M1C1 vs YDLIDAR 接线 |
| `docs/soc_mcu_protocol.md` | 帧布局、左右轮、`enable_power`、超时清 PWM |
| `docs/orangepi_system_setup.md` | Jammy overlay、设备节点、外设冒烟与检查表 |
| `docs/newbot_ros1_architecture.md` | 原厂 ROS1 话题/节点（ROS2 移植基线） |

改引脚、协议字段或设备节点时：**固件 / 软件 / 文档同一提交意图内对齐**，不要 silently 改一端。

---

## 全局硬约束

1. **CubeMX**：禁止手改生成段；外设/引脚变更走 `.ioc` → Generate Code（见 `firmware/AGENTS.md`）。
2. **协议**：`#pragma pack(1)`；头 `DA`、尾 `TA\r\n`；改布局必须同步 `docs/soc_mcu_protocol.md` 与两端解析代码。
3. **Jammy 设备名**（勿沿用 newbot 旧名）：雷达 `/dev/ttyS0`、相机 `/dev/video3`、STM32 `/dev/ttyS2`；喇叭 sink `rk809_analog`。
4. **`ref/`**：只参考；移植时适配本仓库分层与节点名，禁止整文件覆盖。
5. **文档**：未在板上确认前，不把猜测写成 `docs/` 事实；确认后先改文档再改依赖代码（或同一改动内一起改）。

---

## 开发工作流（建议）

1. 板级点亮：`docs/orangepi_system_setup.md` + `software/scripts/`（**不依赖 ROS2**）
2. 底盘联调：停 `serial-getty@ttyS2`；`orangepi_stm32_uart_test.py` 或 `firmware/tools/soc_console`
3. ROS2：Windows `sync-to-orangepi.ps1` → 板上 aarch64 `colcon build`（勿拷 x86 `install/`）
4. 固件：`firmware/xBot/scripts/build.ps1`（CubeCLT + Ninja）

---

## AI / 协作者路由

| 用户意图大致落在… | 打开 |
|-------------------|------|
| 电机、IMU、USART 协议、Cube、烧录 | `firmware/AGENTS.md` |
| ROS2 节点、overlay、LCD/麦/雷达脚本 | `software/AGENTS.md` |
| 引脚对不对、帧长多少、板上命令 | `docs/*` |
| 原厂怎么做的 | `ref/newbot` + `docs/newbot_ros1_architecture.md` |

只改任务相关代码；不做无关重构；不提交密钥与 `build/` 产物。
