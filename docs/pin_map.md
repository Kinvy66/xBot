# xBot 硬件引脚对应表

xBot 复现 [newbot（小白机器人）](https://newbot.readthedocs.io) 硬件；底盘 MCU / 香橙派 / 外设接线与参考项目一致。差异：STM32 上**额外外挂** MPU6050 模块；上层软件规划迁到 **ROS2**（硬件连接不变）。

依据：`firmware/xBot` CubeMX / 代码，以及 `ref/newbot` 组装说明、原厂 IO 定义与 ROS 节点配置。

SOC ↔ STM32 串口帧协议见 [`soc_mcu_protocol.md`](soc_mcu_protocol.md)。  
原厂 ROS1 节点与话题见 [`newbot_ros1_architecture.md`](newbot_ros1_architecture.md)（xBot 将在 ROS2 重实现）。  
香橙派 overlay / 设备节点 / 点亮命令见 [`orangepi_system_setup.md`](orangepi_system_setup.md)。

| 项目 | 内容 |
|------|------|
| 底盘 MCU | STM32F103C8Tx（LQFP48） |
| 上层 SOC | 香橙派 3B（RK3566） |
| 硬件开源 | [oshwhub.com/lw95/newbot](https://oshwhub.com/lw95/newbot) |
| MCU 定义来源 | `Core/Inc/main.h`、`gpio.c`、`tim.c`、`usart.c`、`adc.c`、`stm32f1xx_hal_msp.c` |
| SOC 定义来源 | `ref/newbot/readthedocs/5_how_to_assemble.md`、`lcd.py`、`base_control.launch`、雷达 launch |

备注：MCU 侧 `HAL_MspInit()` 执行 `__HAL_AFIO_REMAP_SWJ_NOJTAG()`，关闭 JTAG、保留 SWD，释放 `PA15` / `PB3` / `PB4`。

---

## 0. 系统互连总览

```
┌──────────────────────────────┐         ┌──────────────────────────────┐
│  香橙派 3B (RK3566 / ROS2)   │         │  STM32F103 底盘控制板         │
│                              │  5V/GND │                              │
│  40Pin：电源 + UART2         ├────────►│  供电 / USART1 (PA9/PA10)    │
│  40Pin：SPI3 + GPIO 液晶屏   │         │                              │
│  40Pin：UART9 + GPIO 雷达    │         │  PB4 → MOS → 雷达 5V 电源    │
│  3.5mm 耳机 → 功放 / 喇叭    │         │  CI-03T / 电机 / 编码器 / ADC│
│  USB 摄像头 / USB 麦克风     │         │  外挂 MPU6050 (PB6/PB7)      │
└──────────────────────────────┘         └──────────────────────────────┘
```

设备树 overlay（`/boot/orangepiEnv.txt`）。  
**Orange Pi 1.0.8 Jammy / 内核 6.6** 实测（无 `uart2-m0` dtbo；UART2 已在主 DTB 打开）：

```text
overlays=spi3-m0-cs0-spidev uart9-m2
```

| 外设 | Linux 设备（本镜像） | Overlay / 来源 |
|------|----------------------|----------------|
| 与 STM32 通信 | `/dev/ttyS2` | 主 DTB 默认 UART2 |
| 激光雷达串口 | **`/dev/ttyS0`**（UART9 `fe6d0000`） | `uart9-m2`；newbot 旧文档为 `/dev/ttyS9` |
| 圆形液晶屏 SPI | `/dev/spidev3.0` | `spi3-m0-cs0-spidev` |
| USB 相机（HBVCAM） | **`/dev/video3`**（采集） | UVC；`/dev/video0` 为 RGA，勿用 |

联调底盘前勿让 `console=ttyS2` 占用串口。细节与命令见 [`orangepi_system_setup.md`](orangepi_system_setup.md)。

---

# 一、香橙派 3B（40Pin + 外设）

物理脚以 Orange Pi 3B `gpio readall` / `lcd.py` 注释为准。  
线色以组装文档习惯标注（实际以线束为准）。

## 1. 电源与底板串口（↔ STM32）

控制板右侧接口丝印：`5V` / `GND` / `RX1` / `TX1`。  
交叉连接：SOC 发送 → MCU 接收，SOC 接收 ← MCU 发送。

| 功能 | 香橙派信号 | 40Pin | 控制板线标 | STM32 | 说明 |
|------|------------|-------|------------|-------|------|
| 供电 +5V | 5V | 4 | 5V（红） | 控制板 5V 轨 | 香橙派电源 |
| 地 | GND | 6 | GND（黑） | GND | 共地 |
| SOC → MCU | TXD.2 | 10 | → RX1 | PA10 `USART1_RX` | `/dev/ttyS2` 发送 |
| SOC ← MCU | RXD.2 | 8 | ← TX1 | PA9 `USART1_TX` | `/dev/ttyS2` 接收 |

波特率：115200 8N1。ROS / ROS2 底盘节点设备节点：`/dev/ttyS2`。

```
香橙派 TXD.2 (Pin10) ──► 控制板 RX1 ──► STM32 PA10 (USART1_RX)
香橙派 RXD.2 (Pin8)  ◄── 控制板 TX1 ◄── STM32 PA9  (USART1_TX)
香橙派 5V/GND (Pin4/6) ── 控制板 5V/GND
```

## 2. 1.28 寸 SPI 圆形液晶屏

总线：`SPI3`（`/dev/spidev3.0`），Mode 0；D/C、RES 为 GPIO。  
软件：`gpio128` = D/C，`gpio130` = RES（非 UART 功能，仅借 TXD.7 焊盘作 GPIO）。

| 屏信号 | 香橙派信号 | 40Pin | Linux / 编号 | 线色（组装文档） |
|--------|------------|-------|--------------|------------------|
| VCC | 3.3V | 17 | — | 白 |
| GND | GND | 25 | — | 蓝 |
| MOSI | SPI3_MOSI / SPI3_TXD | 19 | `spidev3.0` | 金黄 |
| SCLK | SPI3_CLK | 23 | `spidev3.0` | 绿 |
| MISO | SPI3_MISO | 21 | — | **NC 不接** |
| D/C | GPIO4_A0 | 13 | sysfs `gpio128`（wPi 7） | 黑 |
| RES | TXD.7（作 GPIO） | 15 | sysfs `gpio130`（wPi 8） | 红 |
| CS | SPI3_CS0 | 24 | 由 spidev 片选 | 板上走线 / 按屏线束 |

分辨率：240×240。

## 3. 激光雷达（UART + 电源）

串口接到香橙派；**5V 电源**走控制板，由 STM32 `PB4` 经 MOS 开关。

### 3.1 40Pin 信号线

| 雷达信号 | 香橙派信号 | 40Pin | Linux | 说明 |
|----------|------------|-------|-------|------|
| TX（数据） | RXD.9 | 29 | **`/dev/ttyS0`**（本镜像）；旧文档 `/dev/ttyS9` | 雷达 → SOC；以 `dmesg` 中 `fe6d0000` 为准 |
| RX / CTL | TXD.9 或 GPIO3_D4 | **22** 或 31 | UART9 TX / wPi 20 | **M1C1：绿线接 Pin22（TXD.9）**；YDLIDAR：绿线接 Pin31 CTL |
| GND | GND | 30 | — | 信号地 |

### 3.2 雷达电源（控制板接口）

电机朝下时，雷达座从左到右习惯线序：红 / 黑 / 金黄 / 绿。

| 引脚 | 功能 | 说明 |
|------|------|------|
| 5V（红） | 雷达电源 | 负极经 MOS，由 STM32 `PB4`（`LiDarSwitch`）控制通断 |
| TX（黑） | 串口数据 | → 香橙派 RXD.9（Pin29） |
| GND（金黄） | 地 | |
| CTL（绿） | 转速 / UART RX | YDLIDAR → Pin31 CTL；**M1C1 → Pin22 TXD.9**（发 `A5 F0`） |

常用类型：`YDLIDAR`、`M1C1_MINI`（`LIDAR_TYPE`）；USB 版可用 `/dev/ttyUSB0`。

## 4. 音频 / 摄像头 / 其它（非 40Pin）

| 外设 | 连接方式 | 说明 |
|------|----------|------|
| 喇叭 | 香橙派 3.5mm → 控制板 `HP`/`SP` + 功放 | Pulse sink **`rk809_analog`**（ALSA card2）；`Playback Mux` HP/SPK；已验证 |
| USB 麦克风 | 香橙派 USB | C-Media **`08bb:2902`**；Pulse 默认 source；`orangepi_mic_test.sh` 已验证 |
| CI-03T 咪头 | 控制板 `MC` | 离线语音，不经香橙派（与 USB 麦无关） |
| USB 摄像头 | 香橙派 USB（经上壳 USB 转接板） | HBVCAM：采集 **`/dev/video3`**，MJPG 1280×720；勿用 RGA 的 `/dev/video0` |
| 散热风扇 | 香橙派 PWM 风扇座 | 设备树 `pwm-fan` |

## 5. 香橙派 40Pin 已用脚速查

| 物理脚 | 信号 | 用途 |
|--------|------|------|
| 4 | 5V | 主板供电 |
| 6 | GND | 共地 |
| 8 | RXD.2 | ← STM32 USART1_TX |
| 10 | TXD.2 | → STM32 USART1_RX |
| 13 | GPIO4_A0 | 液晶屏 D/C |
| 15 | TXD.7（GPIO） | 液晶屏 RES |
| 17 | 3.3V | 液晶屏 VCC |
| 19 | SPI3_MOSI | 液晶屏 MOSI |
| 21 | SPI3_MISO | 屏侧 NC |
| 23 | SPI3_CLK | 液晶屏 CLK |
| 25 | GND | 液晶屏 GND |
| 22 | TXD.9 | 雷达 RX（**M1C1** 绿线） |
| 29 | RXD.9 | 雷达 TX（数据） |
| 30 | GND | 雷达地 |
| 31 | GPIO3_D4 | 雷达 CTL（**YDLIDAR**） |

其余 40Pin 脚当前未用，可扩展。

---

# 二、STM32F103 底盘控制板

## 1. TB6612 电机驱动

双路直流电机：通道 A（左轮）、通道 B（右轮）。  
PWM：`TIM1` CH1 / CH4；方向脚为普通 GPIO。  
参考原厂：`ref/newbot/.../USER/pwm.c`（`pwm_ouput_tb6612`）。

| TB6612 脚 | MCU 引脚 | 端口宏 | 方向 | 说明 |
|-----------|----------|--------|------|------|
| PWMA | PA8 | `PWMA_Pin` / `PWMA_GPIO_Port` | 输出 (AF) | TIM1_CH1，左轮油门 |
| AIN1 | PB15 | `AIN1_Pin` / `AIN1_GPIO_Port` | 推挽输出 | 左轮方向 |
| AIN2 | PB12 | `AIN2_Pin` / `AIN2_GPIO_Port` | 推挽输出 | 左轮方向 |
| PWMB | PA11 | `PWMB_Pin` / `PWMB_GPIO_Port` | 输出 (AF) | TIM1_CH4，右轮油门 |
| BIN1 | PA12 | `BIN1_Pin` / `BIN1_GPIO_Port` | 推挽输出 | 右轮方向 |
| BIN2 | PA15 | `BIN2_Pin` / `BIN2_GPIO_Port` | 推挽输出 | 右轮方向；上电默认 Low |

### 1.1 方向控制关系

```
正转（pwm > 0）：
  左轮：AIN2=1, AIN1=0；TIM1_CCR1 = |pwm|
  右轮：BIN1=1, BIN2=0；TIM1_CCR4 = |pwm|

反转（pwm < 0）：
  左轮：AIN2=0, AIN1=1；TIM1_CCR1 = |pwm|
  右轮：BIN1=0, BIN2=1；TIM1_CCR4 = |pwm|
```

---

## 2. 正交编码器（×2）

左轮：`TIM3`；右轮：`TIM2`。

| 信号 | 引脚 | 端口宏 | 外设 | 方向 | 说明 |
|------|------|--------|------|------|------|
| EN_A1 | PA6 | `EN_A1_Pin` / `EN_A1_GPIO_Port` | TIM3_CH1 | 输入 | 左轮编码器 A |
| EN_B1 | PA7 | `EN_B1_Pin` / `EN_B1_GPIO_Port` | TIM3_CH2 | 输入 | 左轮编码器 B |
| EN_A2 | PA0 | `EN_A2_Pin` / `EN_A2_GPIO_Port` | TIM2_CH1 | 输入 | 右轮编码器 A |
| EN_B2 | PA1 | `EN_B2_Pin` / `EN_B2_GPIO_Port` | TIM2_CH2 | 输入 | 右轮编码器 B |

---

## 3. 电源采样 ADC（ADC1）

| 信号 | 引脚 | ADC 通道 | 方向 | 说明 |
|------|------|----------|------|------|
| ADC_VBAT | PA4 | ADC1_IN4 | 模拟输入 | 电池电压（分压） |
| ADC_VUSB | PA5 | ADC1_IN5 | 模拟输入 | USB / 充电器检测 |

MSP 同时配置 PA4/PA5；Cube 常规转换通道当前为 `ADC_CHANNEL_5`（PA5）。

---

## 4. USART1 ↔ 香橙派

115200 8N1；RX 使用 `DMA1_Channel5`。  
`printf` 经 `RetargetInit(&huart1)` 重定向到 USART1。  
帧协议（`McuData` / `CmdData`，20 ms）：见 [`soc_mcu_protocol.md`](soc_mcu_protocol.md)。

| MCU 信号 | 引脚 | 方向 | 对端 |
|----------|------|------|------|
| USART1_TX | PA9 | 输出 (AF) | → 香橙派 RXD.2（40Pin-8） |
| USART1_RX | PA10 | 输入 | ← 香橙派 TXD.2（40Pin-10） |

---

## 5. USART2 ↔ CI-03T 离线语音

115200 8N1。控制板另有 CI 烧录口：`5V RX0 TX0 GND`（按烧录键后再上电/复位）。

| MCU 信号 | 引脚 | 方向 | 板级 |
|----------|------|------|------|
| USART2_TX | PA2 | 输出 (AF) | → CI-03T RX（`CI_RX1`） |
| USART2_RX | PA3 | 输入 | ← CI-03T TX（`CI_TX1`） |

---

## 6. MPU6050（xBot 外挂模块）

相对原版 newbot 整机，xBot **在 STM32 上额外外挂** MPU6050。  
使用 **I2C1 默认脚**（`hi2c1`，400 kHz，PB6/PB7，无 remap）；驱动对齐 `ref/MPU_Test` / `ref/47-硬件I2C-MPU6050`。

| 信号 | 引脚 | 端口宏 / 句柄 | 方向 | 说明 |
|------|------|---------------|------|------|
| MPU_SCL | PB6 | `MPU_SCL_*` / I2C1_SCL | 开漏 (AF) | 接模块 SCL |
| MPU_SDA | PB7 | `MPU_SDA_*` / I2C1_SDA | 开漏 (AF) | 接模块 SDA |

模块另接 3.3V / GND（与 MCU 共地）；默认 7-bit 地址 `0x68`（AD0=GND）。**不要**开启 I2C1 remap（否则脚会跑到 PB8/PB9）。

---

## 7. 激光雷达电源开关

| 信号 | 引脚 | 方向 | 说明 |
|------|------|------|------|
| LiDarSwitch | PB4 | 推挽输出 | 控制雷达 5V MOS；Cube 宏 `Lida_Pin`（标签 `Lida`），高电平开，默认关 |

上电自检：`task_chassis_init()` 内开约 2 s 再关；之后由 SOC `CmdData.enable_power` 控制。保持 `SWJ_NOJTAG`。

---

## 8. LED / SWD / 控制板其它座子

| 信号 | 引脚 | 说明 |
|------|------|------|
| LED | PB3 | 状态灯；**低电平亮**；上电后应拉高熄灭 |
| SWDIO | PA13 | SWD |
| SWCLK | PA14 | SWD |
| 烧录座 | 3V3 / IO / CLK / GND | SWD；烧录时可只接 IO/CLK/GND |
| 电池 | G / VB | 接电池转接板 |
| 雷达电源座 | − / 5V | 负极受 PB4 MOS 控制 |
| SP / HP / MC | 音频座 | 喇叭、耳机回路、CI 麦克风 |

---

## 附录 A. STM32 端口汇总

| 端口 | 已用引脚 | 所属模块 |
|------|----------|----------|
| GPIOA | PA0, PA1 | 编码器 TIM2（右） |
| GPIOA | PA2, PA3 | USART2（CI-03T） |
| GPIOA | PA4, PA5 | ADC（VBAT / VUSB） |
| GPIOA | PA6, PA7 | 编码器 TIM3（左） |
| GPIOA | PA8, PA11 | TB6612 PWM（TIM1） |
| GPIOA | PA9, PA10 | USART1（香橙派） |
| GPIOA | PA12, PA15 | TB6612 BIN1 / BIN2 |
| GPIOA | PA13, PA14 | SWD |
| GPIOB | PB3 | LED |
| GPIOB | PB4 | 雷达电源（`Lida_Pin`） |
| GPIOB | PB6, PB7 | 外挂 MPU6050（I2C1） |
| GPIOB | PB12, PB15 | TB6612 AIN2 / AIN1 |

---

## 附录 B. 宏定义速查（`Core/Inc/main.h`）

```c
#define EN_A2_Pin               GPIO_PIN_0
#define EN_A2_GPIO_Port         GPIOA
#define EN_B2_Pin               GPIO_PIN_1
#define EN_B2_GPIO_Port         GPIOA
#define EN_A1_Pin               GPIO_PIN_6
#define EN_A1_GPIO_Port         GPIOA
#define EN_B1_Pin               GPIO_PIN_7
#define EN_B1_GPIO_Port         GPIOA
#define AIN2_Pin                GPIO_PIN_12
#define AIN2_GPIO_Port          GPIOB
#define AIN1_Pin                GPIO_PIN_15
#define AIN1_GPIO_Port          GPIOB
#define PWMA_Pin                GPIO_PIN_8
#define PWMA_GPIO_Port          GPIOA
#define PWMB_Pin                GPIO_PIN_11
#define PWMB_GPIO_Port          GPIOA
#define BIN1_Pin                GPIO_PIN_12
#define BIN1_GPIO_Port          GPIOA
#define BIN2_Pin                GPIO_PIN_15
#define BIN2_GPIO_Port          GPIOA
#define LED_Pin                 GPIO_PIN_3
#define LED_GPIO_Port           GPIOB
#define Lida_Pin                GPIO_PIN_4
#define Lida_GPIO_Port          GPIOB
#define MPU_SCL_Pin             GPIO_PIN_6
#define MPU_SCL_GPIO_Port       GPIOB
#define MPU_SDA_Pin             GPIO_PIN_7
#define MPU_SDA_GPIO_Port       GPIOB
```
