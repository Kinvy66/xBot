# SOC ↔ STM32 通信协议

xBot 复现 newbot 底盘协议。物理链路见 [`pin_map.md`](pin_map.md)；本文件整理帧格式、时序与语义。

| 项目 | 内容 |
|------|------|
| 物理接口 | USART1（MCU）↔ UART2（香橙派 `/dev/ttyS2`） |
| 波特率 | 115200 8N1，无流控 |
| 字节序 | 小端（STM32 / RK3566 均为 LE） |
| 对齐 | `#pragma pack(1)`，无填充 |
| 周期 | **20 ms**（50 Hz） |
| 参考源码 | MCU：`ref/newbot/.../USER/usart.h`、`MAIN/main.c`；SOC：`base_control/src/base_control.h`、`uart.cpp` |
| 原厂说明 | `ref/newbot/readthedocs/4_technical_principle.md` §底盘控制节点 |
| ROS1 节点 | [`newbot_ros1_architecture.md`](newbot_ros1_architecture.md) 中 `base_control` |

---

## 1. 通信模型

```
香橙派 base_control                STM32 底盘固件
        │                                │
        │◄──── McuData (29 B) ──────────│  每 20 ms 主动上报
        │                                │
        │──── CmdData  (12 B) ─────────►│  每 20 ms 下发控制
        │                                │
```

- **上行 `McuData`**：MCU → SOC。SysTick 周期（20 ms）内读编码器、IMU、填状态，再整帧写出 USART1。
- **下行 `CmdData`**：SOC → MCU。ROS 侧读完一帧 `McuData` 后做差速/PID，再发一帧 `CmdData`。
- 帧定界：尾部固定 **`T` `A` `\r` `\n`**（`0x54 0x41 0x0D 0x0A`）；收端看到该四字节即认为一帧结束，再校验头与长度。

---

## 2. 公共帧封装

两帧共用同一套头尾：

| 偏移 | 字段 | 类型 | 值 / 含义 |
|------|------|------|-----------|
| 0 | `head1` | `uint8` | `'D'` = `0x44` |
| 1 | `head2` | `uint8` | `'A'` = `0x41` |
| 2 | `struct_size` | `uint8` | **整帧字节数**（含头尾） |
| 3 … N−5 | payload | — | 见下节 |
| N−4 | `end1` | `uint8` | `'T'` = `0x54` |
| N−3 | `end2` | `uint8` | `'A'` = `0x41` |
| N−2 | `end3` | `uint8` | `'\r'` = `0x0D` |
| N−1 | `end4` | `uint8` | `'\n'` = `0x0A` |

校验（MCU / SOC 两侧一致）：

1. `head1/head2 == 'D''A'`
2. `struct_size == 本帧实际长度`
3. 长度等于约定结构体大小（`McuData`=29 或 `CmdData`=12）

非法帧丢弃，不更新控制量。

---

## 3. 上行帧 `McuData`（MCU → SOC）

**总长 29 字节**，`struct_size = 29`（`0x1D`）。相对原厂 16 B 帧，在 `asr_id` 与帧尾之间插入 MPU6050 原始量（xBot 扩展）。

| 偏移 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0–2 | 头 + `struct_size` | — | `'D''A'` + `29` |
| 3–4 | `encoder1` | `int16` | 本周期左轮脉冲增量（软件约定，见 §5） |
| 5–6 | `encoder2` | `int16` | 本周期右轮脉冲增量 |
| 7–8 | `vbat_mv` | `int16` | 电池电压，单位 **mV** |
| 9 | `charger_connected` | `uint8` | `1`=充电器/USB 接入，`0`=未接 |
| 10 | `fully_charged` | `uint8` | `1`=充满，`0`=未满 |
| 11 | `asr_id` | `uint8` | 离线语音命令 ID；无新命令时为 **0** |
| 12 | `imu_ok` | `uint8` | `1`=本周期 MPU 采样有效，`0`=无效/未就绪 |
| 13–14 | `ax` | `int16` | 加速度计 X 原始值（±2 g，16384 ≈ 1 g） |
| 15–16 | `ay` | `int16` | 加速度计 Y |
| 17–18 | `az` | `int16` | 加速度计 Z |
| 19–20 | `gx` | `int16` | 陀螺仪 X 原始值（±250 °/s） |
| 21–22 | `gy` | `int16` | 陀螺仪 Y |
| 23–24 | `gz` | `int16` | 陀螺仪 Z |
| 25–28 | 尾 | — | `'T''A''\r''\n'` |

### 3.1 十六进制布局示例

```
偏移:  00 01 02  03 04  05 06  07 08  09 0A 0B  0C  0D 0E  0F 10  11 12  13 14  15 16  17 18  19 1A 1B 1C
字段:  D  A  1D  enc1   enc2   vbat   ch fu as  ok  ax     ay     az     gx     gy     gz     T  A  \r \n
例:    44 41 1D  E8 FF  64 00  D0 2E  01 00 00  01  0A 00  14 00  00 40  FF FF  02 00  03 00  54 41 0D 0A
含义:  头     29  -24    100    12000  充电 无ASR IMU有效  ax=10 ay=20 az=16384 gx=-1 gy=2 gz=3  尾
```

### 3.2 字段生成（原厂固件行为 + xBot IMU）

| 字段 | 来源 |
|------|------|
| `encoder1` / `encoder2` | 每 20 ms 读定时器并清零；**左** `encoder1 = -TIM3.CNT`，**右** `encoder2 = TIM2.CNT`（与 `pin_map`：TIM3=左、TIM2=右；符号校正前进为正） |
| `vbat_mv` | PA4 ADC，约 200 ms 滑动平均（20 次 × 主循环 10 ms） |
| `charger_connected` | 原厂用 PA5 数字电平平均判断 USB/充电接入 |
| `fully_charged` | 原厂用 PB3 充电状态脚平均判断满电 |
| `asr_id` | USART2 收到 CI-03T 单字节则填入本周期；否则置 0（只上报一次） |
| `imu_ok` / `ax`…`gz` | xBot：`sensor_task` 读 MPU6050（I2C2），经 `app_state` 写入本帧；`imu_ok=0` 时其余 IMU 字段置 0 |

> 注：xBot 将 PB3 用作 LED、PA5 作模拟采样时，充电两字节的采样实现可改，但**帧字段布局保持不变**（相对本文件约定的 29 B 扩展帧）。

---

## 4. 下行帧 `CmdData`（SOC → MCU）

**总长 12 字节**，`struct_size = 12`（`0x0C`）。

| 偏移 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0–2 | 头 + `struct_size` | — | `'D''A'` + `12` |
| 3–4 | `pwm1` | `int16` | 左轮油门 PWM（有符号） |
| 5–6 | `pwm2` | `int16` | 右轮油门 PWM（有符号） |
| 7 | `enable_power` | `uint8` | `1`=开雷达 5V，`0`=关（MCU `PB4` MOS） |
| 8–11 | 尾 | — | `'T''A''\r''\n'` |

### 4.1 十六进制布局示例

```
偏移:  00 01 02  03 04  05 06  07  08 09 0A 0B
字段:  D  A  0C  pwm1   pwm2   en  T  A  \r \n
例:    44 41 0C  B0 04  50 FB  01  54 41 0D 0A
含义:  头     12  1200   -1200  开雷达     尾
```

### 4.2 PWM 语义

| 符号 | 含义 |
|------|------|
| `pwm > 0` | 该侧轮前进 |
| `pwm < 0` | 该侧轮后退 |
| `pwm = 0` | 停止（MCU 侧对应方向脚 + 占空比清零） |

幅值由 SOC 位置式 PID 计算，并限幅到 `max_pwm`（dynamic_reconfigure，默认与电机/TIM1 ARR 匹配，原厂 TIM1 周期约 6400）。  
MCU 侧：`pwm_ouput_tb6612(pwm1, pwm2)` → TIM1_CH1 / CH4 + AIN/BIN 方向脚。

### 4.3 雷达电源

`enable_power` 映射自 ROS 话题 `/enable_lidar`（`std_msgs/Bool`）。  
MCU 在 `0→1` / `1→0` 边沿做软开关（渐开/渐关 PWM 驱动 MOS），避免冲击。

---

## 5. 编码器 / 差速约定

软件约定（与 `base_control` 一致）：

| 名称 | 角色 | MCU 采样 |
|------|------|----------|
| `encoder1` / `pwm1` / `target1` | **左轮** | `-TIM3`（EN_A1/B1） |
| `encoder2` / `pwm2` / `target2` | **右轮** | `TIM2`（EN_A2/B2） |

正脉冲表示该轮朝前进方向转动。SOC 里程计：

```text
delta_m   = (encoder2 + encoder1) * 0.5 / pulses_per_meter
delta_rad = (encoder2 - encoder1) / wheel_distance_m / pulses_per_meter

target1 = (v - w * L/2) * dt * pulses_per_meter   // 左
target2 = (v + w * L/2) * dt * pulses_per_meter   // 右
```

其中 `v`、`w` 来自 `/cmd_vel`（`geometry_msgs/Twist`）。

---

## 6. 时序与安全

| 项目 | 行为 |
|------|------|
| MCU 上报周期 | SysTick = 20 ms，固定发完整 `McuData` |
| SOC 控制周期 | 约 20 ms：阻塞读一帧 `McuData` → PID → 发 `CmdData` |
| 心跳停机 | MCU 若连续 **≥5 个周期（约 100 ms）** 未收到合法 `CmdData`，强制 `pwm1=pwm2=0` |
| 换向清积分 | SOC 在目标速度过零换向时重置该侧 PID 积分，减轻冲击 |

---

## 7. C 结构体定义（两端一致）

```c
#pragma pack(1)

typedef struct
{
    unsigned char head1;              /* 'D' */
    unsigned char head2;              /* 'A' */
    unsigned char struct_size;        /* 29 */

    short encoder1;
    short encoder2;

    short vbat_mv;
    unsigned char charger_connected;
    unsigned char fully_charged;

    unsigned char asr_id;

    unsigned char imu_ok;
    short ax;
    short ay;
    short az;
    short gx;
    short gy;
    short gz;

    unsigned char end1;               /* 'T' */
    unsigned char end2;               /* 'A' */
    unsigned char end3;               /* '\r' */
    unsigned char end4;               /* '\n' */
} McuData;                            /* sizeof == 29 */

typedef struct
{
    unsigned char head1;              /* 'D' */
    unsigned char head2;              /* 'A' */
    unsigned char struct_size;        /* 12 */

    short pwm1;
    short pwm2;
    unsigned char enable_power;

    unsigned char end1;               /* 'T' */
    unsigned char end2;               /* 'A' */
    unsigned char end3;               /* '\r' */
    unsigned char end4;               /* '\n' */
} CmdData;                            /* sizeof == 12 */

#pragma pack()
```

SOC 侧静态初始化示例：

```c
CmdData cmd_data = {'D', 'A', sizeof(CmdData), 0, 0, 0, 'T', 'A', '\r', '\n'};
```

---

## 8. 与 ROS / ROS2 的话题映射（参考）

协议本身与 ROS 版本无关；newbot ROS1 映射如下，迁 ROS2 时保持同名语义即可。

| 方向 | 话题 | 消息 | 与帧字段关系 |
|------|------|------|----------------|
| SOC←MCU | `/odom`、TF | `nav_msgs/Odometry` | 由 `encoder1/2` 积分 |
| SOC←MCU | 电池话题 | `sensor_msgs/BatteryState` | `vbat_mv`、`charger_*`、`fully_charged` |
| SOC←MCU | `/asr_id` | `std_msgs/Int32` | `asr_id != 0` 时发布 |
| SOC←MCU | `/imu/data_raw`（规划） | `sensor_msgs/Imu` | `imu_ok` 时由 `ax`…`gz` 换算 |
| SOC←MCU | `/robot_state` 等 | 多数组 | 调试用编码器/PWM/电压 |
| SOC→MCU | `/cmd_vel` | `geometry_msgs/Twist` | → PID → `pwm1/pwm2` |
| SOC→MCU | `/enable_lidar` | `std_msgs/Bool` | → `enable_power` |

串口参数：`dev=/dev/ttyS2`，`buad=115200`（launch 中拼写为 `buad`）。

---

## 9. `asr_id` 说明

- 来源：CI-03T 经 USART2 发往 MCU 的**单字节**命令号。
- MCU 透传到 `McuData.asr_id`；无新数据则该周期为 0。
- SOC `wakeup_process` 将 ID（从 1 起）映射到命令词表（前进/后退/转向等），再驱动导航或 TTS。
- 具体 ID↔词条以 CI-03T 固件/词条表为准，不属于本串口帧扩展字段。

---

## 10. 实现检查清单（xBot）

- [ ] 两端结构体 `#pragma pack(1)`，`sizeof(McuData)==29`，`sizeof(CmdData)==12`
- [ ] 帧头 `'DA'`、帧尾 `"TA\r\n"`，`struct_size` 等于整帧长度
- [ ] 20 ms 周期；MCU 心跳超时停 PWM
- [ ] `pwm` / `encoder` 符号：前进为正；左右与差速公式一致
- [ ] `enable_power` 控制雷达 MOS（`PB4`）
- [ ] `imu_ok` / `ax`…`gz` 与 MPU6050 采样一致；失败时 `imu_ok=0`
- [ ] 小端序；勿在中间插入对齐填充
