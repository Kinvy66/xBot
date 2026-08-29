# xBot 固件（STM32）

底盘 MCU 固件：STM32F103C8 + HAL + FreeRTOS。硬件复现 newbot，额外外挂 MPU6050；与香橙派通过 USART1 通信（协议见仓库 `docs/`）。

| 项目 | 内容 |
|------|------|
| MCU | STM32F103C8Tx（LQFP48） |
| 工程路径 | `firmware/xBot/` |
| CubeMX | `xBot.ioc` |
| 构建 | CMake + Ninja + `arm-none-eabi-gcc`（建议 STM32CubeCLT） |
| RTOS | FreeRTOS（CMSIS-RTOS v2） |
| 调试口 | SWD（PA13/PA14）；已 `SWJ_NOJTAG`，释放 PA15/PB3/PB4 |

跨端文档（勿在固件树内复制协议全文）：

- [引脚表](../docs/pin_map.md)
- [SOC ↔ MCU 协议](../docs/soc_mcu_protocol.md)

AI 约束见同目录 [AGENTS.md](AGENTS.md)。

---

## 目录结构

```text
firmware/
  README.md          # 本文件
  AGENTS.md          # 固件侧 AI / 协作约定
  tools/soc_console/ # PC 模拟 SOC：Web 发 CmdData / 看 McuData
  xBot/              # CubeMX + CMake 工程
    xBot.ioc
    CMakeLists.txt
    CMakePresets.json
    Core/            # Cube 生成：启动、时钟、外设初始化
    App/             # 应用逻辑（任务、业务）
    Bsp/             # 板级驱动（TB6612 C++ 等）
    Protocol/        # soc_protocol + soc_link（SOC 串口帧）
    scripts/
      build.ps1      # 配置 / 编译 / 烧录
    build/           # 本地构建产物（已 gitignore）
```

用户代码优先放在 `App/`、`Bsp/`、`Protocol/`。`Core/` 中仅在 `USER CODE` 区段改动；外设引脚以 CubeMX / `main.h` 为准。

FreeRTOS：`Core/freertos.c` 只创建启动线程并调用 `app_main()`；工作任务在 `App/` 创建：

| 任务 | 文件 | 周期 | 职责 |
|------|------|------|------|
| appMain（Cube） | `freertos.c` → `app_main()` | 一次性 | 创建下列任务后退出 |
| chassis | `task_chassis.c` | 20 ms | SOC 协议、电机、编码器 |
| sensor | `task_sensor.c` | 100 ms | 电池 ADC（预留 MPU） |
| status | `task_status.c` | ~250 ms | LED 指示 |

电机驱动（TB6612，非 TB6621）：

- `Bsp/Inc/tb6612.hpp` — `Tb6612Motor`，构造时绑定 `GpioOut` + `PwmOut`
- `Bsp/Src/motor_drive.cpp` — 本板左右轮引脚实例化；C API：`motor_drive_init` / `motor_drive_set`

---

## SOC 通信（已实现）

- 帧定义：`Protocol/Inc/soc_protocol.h`（`McuData` 29B / `CmdData` 12B）
- USART1 链路：`Protocol/Src/soc_link.c`（DMA + IDLE 收命令，任务内发状态）
- 20 ms 循环：`App/Src/app.c` → `app_chassis_tick()`（心跳停 PWM、编码器、电机）
- 文档：[`docs/soc_mcu_protocol.md`](../docs/soc_mcu_protocol.md)

USART1 专用于二进制协议，勿再把 `printf` 重定向到 USART1。

PC 上可用 `tools/soc_console` 代替香橙派做联调（浏览器控制 PWM / 雷达电源，显示编码器与电压）。详见 [tools/soc_console/README.md](tools/soc_console/README.md)。

---

## 环境要求

- Windows（脚本为 PowerShell）
- [STM32CubeCLT](https://www.st.com/en/development-tools/stm32cubeclt.html)（含 CMake、Ninja、GNU Arm 工具链、CubeProgrammer）
- 可选：环境变量 `STM32CUBECLT_PATH` 指向 CLT 根目录
- 烧录器：ST-Link 或 SEGGER J-Link

也可用 CLion 打开 `firmware/xBot`，使用预设 `Debug` / `Release`。

---

## 编译

脚本位于 `firmware/xBot/scripts/build.ps1`，内部会 `cd` 到工程根 `firmware/xBot`。  
**路径取决于你当前所在目录**：

```powershell
# 若在仓库根目录 xBot/
.\firmware\xBot\scripts\build.ps1

# 若已在 firmware\xBot\（常见）
.\scripts\build.ps1

# Release
.\scripts\build.ps1 -BuildType Release

# 指定 CubeCLT 根目录
.\scripts\build.ps1 -CltRoot "C:\ST\STM32CubeCLT_x.y.z"
```

产物：

- `firmware/xBot/build/Debug/xBot.elf`（及 `.hex` / `.bin`）
- `firmware/xBot/build/Release/...`

等价手动命令：

```powershell
cd firmware\xBot
cmake --preset Debug
cmake --build --preset Debug -j
```

---

## 烧录

在 `firmware\xBot` 下：

```powershell
# ST-Link（默认探针）
.\scripts\build.ps1 -Flash

# J-Link
.\scripts\build.ps1 -Flash -Probe JLink

# 指定 J-Link 可执行文件
.\scripts\build.ps1 -Flash -Probe JLink -JLinkExe "C:\Program Files\SEGGER\JLink\JLink.exe"
```

在仓库根目录则用 `.\firmware\xBot\scripts\build.ps1 ...`。

- ST-Link：CubeProgrammer CLI，SWD 写入 ELF 后复位
- J-Link：拉起 `JLinkGDBServerCL` + `arm-none-eabi-gdb` 下载；若 CLion 正在占用探针，先停止调试

控制板烧录座：`3V3` / `IO`(SWDIO) / `CLK`(SWCLK) / `GND`（烧录时 3V3 可省略，由电池供电）。

---

## 外设职责（摘要）

| 外设 | 用途 |
|------|------|
| TIM1 CH1/CH4 | 左右轮 PWM（TB6612） |
| TIM2 / TIM3 | 编码器 |
| USART1 | ↔ 香橙派（协议帧） |
| USART2 | ↔ CI-03T 离线语音 |
| ADC1 | 电池 / USB 采样 |
| I2C1（PB8/PB9 重映射） | 外挂 MPU6050 |
| PB4 | 雷达 5V MOS（`Lida_Pin`；`enable_power` + 上电自检转约 2 s） |

细节以 `docs/pin_map.md` 为准。

---

## 与 software 的边界

- 本目录只负责 MCU 固件；ROS2 在 `software/`
- 串口帧字段、周期、符号约定以 `docs/soc_mcu_protocol.md` 为唯一事实源
- 改协议须同步 MCU 实现与 `software` 侧解析，并更新 `docs/`
