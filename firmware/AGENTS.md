# firmware AGENTS

本文件约束 **STM32 固件**（`firmware/xBot`）相关改动。仓库总览见根目录 `AGENTS.md`；ROS2 / 香橙派见 `software/AGENTS.md`。

跨端事实源（只引用，不在此复制全文）：

- `docs/pin_map.md` — 引脚与板级连接
- `docs/soc_mcu_protocol.md` — SOC ↔ MCU 帧协议

---

## 范围

- **做**：`firmware/xBot` 下应用、BSP、协议、构建脚本；`firmware/tools/` 联调工具；`Core/` 里仅 `USER CODE BEGIN/END` 区段
- **不做**：`software/` 的 ROS2 节点、香橙派 overlay、改 `ref/` 参考工程、擅自改 `docs/` 中与硬件不符的事实（应先确认再改文档）
- **禁止**：手改 CubeMX 生成代码或直接改 `xBot.ioc`（见下一节）

---

## 工程与代码分层

| 目录 | 用途 | 约定 |
|------|------|------|
| `Core/` | CubeMX 生成代码 | **禁止手改生成段落**；仅允许 `USER CODE BEGIN/END`。外设/引脚/时钟/DMA/中断变更走 `.ioc` 再生 |
| `App/` | 业务、FreeRTOS 任务 | 应用入口与控制逻辑放这里 |
| `Bsp/` | 板级驱动 | 电机、编码器、IMU、雷达电源等封装 |
| `Protocol/` | 与 SOC 通信 | 帧编解码、缓冲；结构体与文档一致 |
| `scripts/` | 构建/烧录 | 保持 PowerShell 脚本可在无 IDE 下使用 |
| `firmware/tools/` | PC 联调 | SOC 控制台等；帧布局必须与 `docs/soc_mcu_protocol.md` 一致 |
| `Drivers/` / `Middlewares/` | ST / FreeRTOS | 非必要不改第三方源码 |

`CMakeLists.txt` 已 `GLOB` `App` / `Bsp` / `Protocol`；新增源文件放对应目录即可，并保证头文件在 `*/Inc`。

---

## CubeMX 生成代码（硬约束）

涉及 CubeMX 生成内容时，**AI 不得直接改这些文件**，应停下并明确告知用户：请在 STM32CubeMX 中改 `xBot.ioc`，再 Generate Code。

生成侧（整文件视为 Cube 所有，不要手补外设初始化）：

- `xBot.ioc`、`.mxproject`
- `Core/Src`、`Core/Inc` 中 **`USER CODE BEGIN/END` 之外** 的所有内容（如 `gpio.c` / `usart.c` / `dma.c` / `tim.c` / `adc.c` 的 MX_* 初始化、时钟、MSP、中断向量里 Cube 生成的部分）
- `cmake/stm32cubemx/`、`startup_stm32f103xb.s`、`STM32F103XX_FLASH.ld`
- `Drivers/`（HAL/CMSIS）以及 Cube 勾选后写入的 `Middlewares/` 文件列表

允许 AI 改的：`App/`、`Bsp/`、`Protocol/`、`scripts/`，以及 `Core/` **仅** `USER CODE BEGIN` … `USER CODE END` 块。

需要改引脚、外设、时钟、DMA、NVIC、HAL 模块开关时：

1. **不要** 在 `.c/.h` 里手写对应初始化来“先顶上”
2. 用条目列出建议的 CubeMX 改动（外设、引脚、模式、DMA、中断优先级等）
3. 提醒用户改完 `.ioc` 后 Generate Code，并核对 `USER CODE` 是否保留
4. 用户生成完成后再继续改应用层代码

引脚事实源：`xBot.ioc` + `Core/Inc/main.h`，并与 `docs/pin_map.md` 对齐。再生后仍须满足：

- 保持 `__HAL_AFIO_REMAP_SWJ_NOJTAG()`（或等价），否则 PA15 / PB3 / PB4 不可用
- I2C1 使用 PB8/PB9 时保持 `__HAL_AFIO_REMAP_I2C1_ENABLE()`
- PB4 为雷达电源 MOS；若启用，在 CubeMX 配 GPIO 宏与初始化，默认关闭

---

## SOC 通信协议（实现约束）

实现或修改 USART1 协议时遵守 `docs/soc_mcu_protocol.md`：

- `#pragma pack(1)`；`sizeof(McuData)==16`，`sizeof(CmdData)==12`
- 头 `'DA'`，尾 `"TA\r\n"`，`struct_size` = 整帧长度
- 小端；周期 **20 ms**；无合法下行约 **100 ms** 后 PWM 清零
- `encoder1`/`pwm1` = 左轮，`encoder2`/`pwm2` = 右轮；前进为正
- `enable_power` 控制雷达 5V；`asr_id` 无新命令时为 0

改字段布局必须同步：`docs/soc_mcu_protocol.md` 与后续 `software` 解析代码。

---

## FreeRTOS / 实时

- 通信与电机控制路径避免长时间关中断或阻塞
- 串口收发注意 DMA/中断与任务的数据共享（volatile、队列或临界区）
- 不要在 ISR 里做重逻辑或 `printf` 刷屏

---

## 构建与烧录

- 默认用 `scripts/build.ps1`（CubeCLT + Ninja）
- 预设：`Debug` / `Release`（见 `CMakePresets.json`）
- 烧录：`-Flash`，`-Probe StLink|JLink`
- 勿提交 `build/` 产物

---

## 风格与改动原则

- 匹配现有 HAL / 命名（`snake_case` 文件与函数、Cube 风格外设句柄）
- 只改任务相关代码；不做无关重构或大规模格式化
- 不引入未使用的中间件；不把 ROS/Linux 代码放进本树
- 参考实现可看 `ref/newbot` 原厂 STM32 工程，移植时优先适配本工程分层与 HAL，而非整文件覆盖

---

## 自检（改固件后）

- [ ] 能 `build.ps1` 编过（Debug 即可）
- [ ] 未手改 CubeMX 生成代码；外设/引脚变更已提示用户改 `.ioc` 并 Generate Code
- [ ] 引脚变更已反映到 `docs/pin_map.md`（若有）
- [ ] 协议变更已反映到 `docs/soc_mcu_protocol.md`，且左右轮/符号/帧长未 silently 改变
- [ ] SWD 仍可用；未误关 `SWJ_NOJTAG` 导致调试口丢失（除非有意）
