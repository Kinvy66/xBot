# newbot ROS1 软件架构（xBot ROS2 移植基线）

本文记录参考工程 **newbot_ws v1.1**（ROS1 Noetic）在香橙派 3B 上的包结构、节点职责与接口。xBot 将在 **ROS2** 上重新实现同等能力，并叠加定制功能；硬件连接见 [`pin_map.md`](pin_map.md)，底盘串口帧见 [`soc_mcu_protocol.md`](soc_mcu_protocol.md)。

| 项目 | 内容 |
|------|------|
| 参考源码 | `ref/newbot/1.小白机器人ROS程序/newbot_ws_v1.1/newbot_ws` |
| 原厂原理说明 | `ref/newbot/readthedocs/4_technical_principle.md` |
| 原厂版本 | newbot 1.0 / 工作空间 v1.1.0 |
| SOC | 香橙派 3B，RK3566，Ubuntu 20.04 |
| 中间件 | ROS1 Noetic（`catkin`） |
| 启动入口 | `roslaunch pkg_launch all.launch` |
| 开机脚本 | `config/rc.local` → `config/start.sh` |

xBot 与参考工程的已知差异（移植时不要 silently 丢掉）：

- 上层规划为 **ROS2**，不是再编一份 Noetic。
- STM32 **外挂 MPU6050**；原厂 `base_control` 里 IMU 发布已注释，xBot 可把 IMU 纳入上行帧或独立话题。
- 协议字段、左右轮符号、20 ms 周期以 [`soc_mcu_protocol.md`](soc_mcu_protocol.md) 为准。

---

## 1. 总体架构

全部业务在用户态。内核只提供 UART / SPI / UVC / ALSA 等现成设备节点；不需要自研 `.ko`。

三大功能块：

1. **移动**：差速底盘 + 雷达 → 里程计 / SLAM / 导航 / 定点动作 / 跟随
2. **视觉**：USB 相机 JPEG → 硬件解码 → YOLO → 跟踪可视化 → 硬件编码
3. **交互**：CI-03T 离线命令词 + 屏上表情 + 在线/离线 ASR-LLM-TTS

```
┌─────────────────────────────────────────────────────────────────┐
│                     香橙派 3B（ROS1 Noetic）                      │
│                                                                 │
│  ┌──────────────┐  /cmd_vel   ┌──────────────┐   UART2          │
│  │ move_server  │────────────►│              │  /dev/ttyS2      │
│  │ object_track │             │ base_control │◄───────────────► STM32
│  │ audio(dance) │             │  差速+PID    │   20 ms 帧      │
│  │ move_base    │             │  odom + tf   │                  │
│  └──────────────┘             └──────┬───────┘                  │
│                                      │ /asr_id                  │
│  ┌──────────────┐  /action_cmd       ▼                          │
│  │wakeup_process│◄──────────── base_control                     │
│  │ CI-03T 映射  │──────────► move_client_cmd ──► do_move        │
│  └──────┬───────┘ /tts                                          │
│         ▼                                                       │
│  ┌──────────────┐                                               │
│  │    audio     │  LCD SPI + USB 麦 + 3.5mm 喇叭 + 云端 API     │
│  └──────────────┘                                               │
│                                                                 │
│  USB cam ──► usb_camera ──► img_decode ──► rknn_yolov6          │
│  /dev/video0     JPEG          RGB             │                │
│                                                ▼                │
│                                    object_track ──► img_encode  │
│                                         ▲           JPEG 预览   │
│  UART9 /dev/ttyS9 ──► ydlidar/m1c1 ──► /scan                    │
└─────────────────────────────────────────────────────────────────┘
```

`/cmd_vel` **没有 mux**：跟随、定点动作、跳舞、`move_base` 都可以写同一话题，后写覆盖先写。ROS2 移植建议加 `twist_mux`（或等价仲裁）。

---

## 2. 包一览

路径均相对 `newbot_ws/src/`。

| 包 | 语言 | 角色 | `all.launch` |
|----|------|------|----------------|
| `pkg_launch` | XML | 总启动、雷达选择、URDF、rviz | 入口 |
| `config` | shell / DTB | 开机、overlay、风扇 DTB、依赖安装 | 否（系统级） |
| `ai_msgs` | msg | 检测框 `Det` / `Dets` | 被依赖 |
| `base_control` | C++ | 底盘串口、里程计、电池、ASR ID | 是 |
| `newbot_urdf` | URDF | 模型与 TF 静态树 | 是（`robot_state_publisher`） |
| `lidar_sensors/*` | C++ | 雷达驱动节点 | 按 `LIDAR_TYPE` 选一个 |
| `usb_camera` | C++ | V4L2 采 MJPEG | 是 |
| `img_decode` | C++ | RK MPP JPEG→RGB + 缩放 | 是 |
| `rknn_yolov6` | C++ | NPU 检测 | 是 |
| `object_track` | C++ | 人跟随 + 叠框 | 是 |
| `img_encode` | C++ | RK MPP RGB→JPEG | 是 |
| `img_rectify` | C++ | 鱼眼去畸变 | **否**（可选） |
| `wakeup_process` | C++ | CI-03T 命令词 → 动作 / TTS | 是 |
| `audio` | Python | 表情屏、TTS、ASR、LLM、多媒体 | 是 |
| `move_action` | C++ | 精确旋转+直行 Action | 是（server + client_cmd） |
| `robot_navigation` | YAML + launch | gmapping / AMCL / move_base | **否**（`nav.launch` 另起） |
| `snowboy_wakeup` | Python | 在线热词（Snowboy） | **否**（`snowboy.launch`） |

`lidar_sensors` 下三个独立 catkin 包：`ydlidar`、`m1c1_mini`、`ldlidar_14`。

---

## 3. 启动与运行时

### 3.1 开机

`/etc/rc.local`（仓库副本 `config/rc.local`）：

1. 关热点残留、播 `boot.mp3`
2. 导出 LCD GPIO 128/130，SPI `chmod 777 /dev/spidev3.0`
3. 雷达 CTL（wPi 20）拉低
4. 以 `orangepi` 跑 `config/start.sh`

`start.sh`：等 Wi-Fi / 否则开 AP；配 `ROS_IP`；`LIDAR_TYPE`；重启 PulseAudio；设 USB 麦为默认源；`roslaunch pkg_launch all.launch`。

设备树 overlay（`/boot/orangepiEnv.txt`，与 [`pin_map.md`](pin_map.md) 一致）：

```text
overlays=spi3-m0-cs0-spidev uart2-m0 uart9-m2
```

另拷贝改过 PWM 风扇的 `rk3566-orangepi-3b-v2*.dtb`。这不是 ROS 节点，是板级使能。

### 3.2 `all.launch` 拉起的节点

顺序（`pkg_launch/launch/all.launch`）：

| 顺序 | launch | 节点名 |
|------|--------|--------|
| 1 | `audio/audio.launch` | `audio` |
| 2 | `base_control/base_control.launch` | `base_control` |
| 3 | `pkg_launch/urdf.launch` | `robot_state_publisher` |
| 4 | `usb_camera/usb_camera.launch` | `usb_camera` |
| 5 | `img_decode/img_decode.launch` | `img_decode` |
| 6 | `rknn_yolov6/rknn_yolov6.launch` | `rknn_yolov6` |
| 7 | `img_encode/img_encode.launch` | `img_encode` |
| 8 | `pkg_launch/lidar.launch` | `ydlidar_node` 或 `m1c1_mini` 等 |
| 9 | `object_track/object_track.launch` | `object_track` |
| 10 | `wakeup_process/wakeup_process.launch` | `wakeup_process` |
| 11 | `move_action/move_action.launch` | `move_server`、`move_client_cmd` |

导航不在默认开机图里。`pkg_launch/nav.launch` = `all.launch` + 三选一：空地图规划 / SLAM / 定位导航。

### 3.3 雷达选择

环境变量 `LIDAR_TYPE`：

| 值 | 节点 | 设备 |
|----|------|------|
| `YDLIDAR` 或空 | `ydlidar_node` | `/dev/ttyS9`，115200 |
| `M1C1_MINI` | `m1c1_mini` | `/dev/ttyS9` |
| `M1C1_MINI_TTYUSB` | `m1c1_mini` | USB 转串口（`m1c1_mini_ttyusb.launch`） |

`ld14.launch` 存在，未挂进 `lidar.launch`。

---

## 4. 全局接口目录

ROS2 移植时优先保持 **话题名与语义**，便于对照联调。

### 4.1 话题

| 话题 | 类型 | 发布 | 订阅 | 说明 |
|------|------|------|------|------|
| `/cmd_vel` | `geometry_msgs/Twist` | `move_server`、`object_track`、`audio`、`move_base` | `base_control` | `linear.x` m/s，`angular.z` rad/s |
| `/odom` | `nav_msgs/Odometry` | `base_control` | 导航、`move_server`（经 TF） | `odom` → `base_footprint` |
| `/tf` | TF | `base_control`（odom）、`robot_state_publisher` | 全局 | 见 §9 |
| `/battery` | `sensor_msgs/BatteryState` | `base_control` ~1 Hz | 可选 | 电压、充放电状态 |
| `/robot_state` | `std_msgs/Int16MultiArray` | `base_control` ~1 Hz | `audio` | 8 个 int16，见下 |
| `/joint_states` | `sensor_msgs/JointState` | `base_control`（代码里发布已注释） | URDF | `l_wheel_joint` / `r_wheel_joint` |
| `/plot` | `std_msgs/Float32MultiArray` | `base_control`（调试用，默认注释） | rqt_plot | PID 目标/实际 |
| `/asr_id` | `std_msgs/Int32` | `base_control` | `wakeup_process` | MCU 上报的 CI-03T ID，0 表示无命令 |
| `/tts` | `std_msgs/String` | `wakeup_process`、`base_control`、找桩客户端 | `audio` | 文本；可选 `#key` 后缀 |
| `/enable_wakeup` | `std_msgs/Bool` | `audio` | `wakeup_process` | TTS/录音时关唤醒，防自唤醒 |
| `/enable_lidar` | `std_msgs/Bool` | `wakeup_process` | `base_control`、`object_track`、`m1c1_mini` | true：MCU 开雷达 5V |
| `/enable_tracking` | `std_msgs/Bool` | `wakeup_process` | `object_track` | 人跟随开关 |
| `/enable_camera` | `std_msgs/Bool` | （预留） | `usb_camera` | 默认使能 |
| `/reset` | `std_msgs/Bool` | 外部 | `base_control` | true 清零位姿与累计编码器 |
| `/action_cmd` | `std_msgs/Float32MultiArray` | `wakeup_process` | `move_client_cmd` | `[mode, angle_deg, distance_m]` |
| `/scan` | `sensor_msgs/LaserScan` | 雷达节点 | `object_track`、costmap | `frame_id=laser_link` |
| `/image_raw/compressed` | `sensor_msgs/CompressedImage` | `usb_camera` | `img_decode`、`audio`（拍照） | 1280×720 MJPEG |
| `/camera/image_raw` | `sensor_msgs/Image` | `img_decode` | `rknn_yolov6`、`audio` | RGB，默认 0.5 缩放 → 640×360 |
| `/camera/image_det` | `sensor_msgs/Image` | `rknn_yolov6` | `object_track` | 画了检测框 |
| `/ai_msg_det` | `ai_msgs/Dets` | `rknn_yolov6` | `object_track` | 检测列表 |
| `/camera/image_det_track` | `sensor_msgs/Image` | `object_track` | `img_encode` | 跟踪可视化 |
| `/camera/image_det_track/compressed` | `sensor_msgs/CompressedImage` | `img_encode` | 远程预览 | 由 `sub_image_topic+"/compressed"` 生成 |
| `/camera/image_rect` | `sensor_msgs/Image` | `img_rectify` | 未进默认图 | 鱼眼校正 |

`/robot_state.data`（长度必须为 8）：

| 下标 | 含义 |
|------|------|
| 0 | `encoder1` 左轮本周期脉冲 |
| 1 | `encoder2` 右轮 |
| 2 | `vbat_mv` |
| 3 | 充电器接入 |
| 4 | 充满 |
| 5 | `pwm1` |
| 6 | `pwm2` |
| 7 | 雷达电源使能 |

`/action_cmd`：`mode==1` 为精确移动；`angle` 度（左正右负）；`distance` 米（前进正、后退负）。`mode==2` 跳舞已注释，实际跳舞在 `audio` 里直接发 `/cmd_vel`。

`/tts` 载荷：`回复文本` 或 `回复文本#key`。`key` 来自 `wakeup_process/cfg/asr.cfg` 的键，或特殊值 `noshow`。

### 4.2 服务

| 服务 | 类型 | 提供 | 调用 | 说明 |
|------|------|------|------|------|
| `/stop_scan` | `std_srvs/Empty` | `ydlidar_node` | `wakeup_process` | 停转；M1C1 不调用 |
| `/start_scan` | `std_srvs/Empty` | `ydlidar_node` | `wakeup_process` | 开转；调速前必须先 stop |

### 4.3 Action

| Action | 类型 | Server | Client | 说明 |
|--------|------|--------|--------|------|
| `/do_move` | `move_action/DoMove` | `move_server` | `move_client_cmd` | 先转到 `goal_angle`（rad），再走到 `goal_distance`（m） |
| `/move_base` | `move_base_msgs/MoveBase` | `move_base` | 导航测试脚本、找桩客户端 | 仅导航 launch |

`DoMove` 定义（`move_action/action/DoMove.action`）：

- Goal：`goal_angle`、`goal_distance`、`angular_speed`、`linear_speed`（后两项必须 > 0）
- Feedback：当前已转角度、已走路程
- Result：`result_flag`（成功/取消/失败）、实际角度与距离

客户端默认：`angular_speed=1.0` rad/s，`linear_speed=0.08` m/s，超时 30 s。

### 4.4 自定义消息

`ai_msgs/Det.msg`：

```
uint16 x1, y1, x2, y2
float32 conf
string cls_name
uint16 cls_id
uint16 obj_id
```

`ai_msgs/Dets.msg`：`std_msgs/Header header` + `Det[] dets`。

---

## 5. 节点详解

### 5.1 `base_control`

**职责**：SOC ↔ STM32 唯一桥梁。读 `McuData`，写 `CmdData`，积分里程计，轮速 PID。

**设备**：`/dev/ttyS2`，115200（launch 覆盖；头文件默认曾写 `/dev/ttyS3`，以 launch 为准）。

**周期**：与 MCU 对齐，约 20 ms 收一帧再回一帧。无合法通信时 MCU 侧约 100 ms 清 PWM（见协议文档）。

**运动学**（encoder1 左、encoder2 右，前进为正）：

```
delta_m   = (enc2 + enc1) / 2 / pulses_per_m
delta_rad = (enc2 - enc1) / wheel_distance_m / pulses_per_m
target_left  = (v - w * L/2) * dt * pulses_per_m
target_right = (v + w * L/2) * dt * pulses_per_m
```

`pulses_per_m`、轮距由 dynamic_reconfigure 标定：

| 参数 | 默认 | 含义 |
|------|------|------|
| `wheel_circumference_mm` | 142 | 轮周长 |
| `pulses_per_wheel_turn_around` | 1431 | 一轮脉冲 |
| `pulses_per_robot_turn_around` | 2309 | 整车转 360° 脉冲 |
| `max_pwm` | 6400 | PID 限幅 |
| `pid_p/i/d` | 50 / 20 / 20 | 位置式 PID，左右轮各一套 |

方向切换时重置积分，避免饱和。目标脉冲为 0 时 PWM 强制 0（制动）。

**其它行为**：

- `asr_id != 0` 时转发 `/asr_id`（MCU 无新命令时为 0）。
- 充电状态边沿播报：断开 / 已连接 / 已充满 → `/tts`。
- 未充电且电压 &lt; 3.5 V：约每 180 s 报一次低电。
- IMU / 磁力计发布代码保留但注释；xBot 可接 MPU6050 后重新打开或改协议字段。

**ROS2 注意**：`tf::TransformBroadcaster` → `tf2_ros`；`dynamic_reconfigure` → 参数 + 可选 `rqt_reconfigure`；`sensor_msgs/BatteryState` 字段有微调，对照 ROS2 定义。

---

### 5.2 `wakeup_process`（节点名 `asr_process`）

**职责**：把 CI-03T 的数字 ID 映射成 TTS、雷达电源、跟随、精确移动。

配置 `wakeup_process/cfg/asr.cfg`，**行号必须与芯片固件 ID 一致**：

```
key=口语命令词（可用|分隔）@回复语
```

ID 换算：`id_index = hex2dec(asr_id) - 1`。`hex2dec` 把数值按十六进制两位写成字符再当十进制读（例如固件发 `0x11` 对应第 11 行，不是十进制 17）。改命令词必须同时改 CI-03T 固件。

仅当 `/enable_wakeup == true` 时处理 `/asr_id`。

**本节点直接执行的 key**（不经过 `audio` 业务，只发 TTS 回复）：

| key | 行为 |
|-----|------|
| `open_lidar` / `high_lidar` / `medium_lidar` / `low_lidar` | GPIO20 调 CTL（M1C1 无效）；`/enable_lidar=true`；YDLIDAR 先 `stop_scan` 再 `start_scan` |
| `off_lidar` | `stop_scan`；GPIO20=0；`/enable_lidar=false` |
| `tracking_person` / `cancel_tracking` | `/enable_tracking` |
| `stop` | 关跟随 + `action_cmd` 距离角度为 0 |
| `reboot` | `sync && reboot` |
| `forward_N` / `backward_N` | N 厘米 → 米，发 `action_cmd` |
| `left_N` / `right_N` | N 度（左正右负），发 `action_cmd` |

其余 key（音量、拍照、聊天等）只拼 `回复#key` 给 `/tts`，由 `audio` 执行。

---

### 5.3 `audio`

**职责**：人机交互中枢。Python 单节点，主线程跑 TTS/命令队列，另线程刷 LCD 表情。

**硬件**：`lcd.py` → `/dev/spidev3.0` + GPIO 128/130；录音 PyAudio（USB 麦）；播放 `play`/`sox` + PulseAudio（RK809 3.5mm）。

**`/tts` 处理**：

1. 按 `#` 拆出 `key`
2. 多数命令 `killall play/ffplay` 并退出镜子/状态屏
3. `emoji.set_display_cmd(key)` 切表情（与 `image/` 目录名对应，如微笑、睡觉）
4. 按 `key` 跑业务（见 §6）
5. 有道在线 TTS（SQLite 缓存），失败则 Kaldi 本地 TTS，再失败 `espeak`
6. 播报前 `/enable_wakeup=false`，结束后再打开

**大模型对话**（key=`chatgpt`，「小白小白」）：录音 → sherpa-onnx Paraformer → 讯飞 Spark Lite。意图分支：含「画」文生图；含「看/面前」拍照+图生文+邮件；含「眼睛变成」改瞳色；含「唱/播放」搜歌名播网易云；否则短对话。

**开机**：后台加载 Kaldi TTS/ASR（约数十秒）；播欢迎词。

**ROS2 注意**：这是最大的单体，建议拆成 `hmi`（LCD/表情）、`voice`（ASR/TTS）、`dialog`（LLM 技能），用 ROS2 接口替换全局 `os.environ` 里的云密钥。

---

### 5.4 图像流水线

按订阅者数量做 **按需订阅**：下游无人订阅则断开上游，省 CPU/NPU。

```
usb_camera          /dev/video0 MJPEG 1280×720
    → /image_raw/compressed
img_decode          RK MPP 解码 + RGA 缩放 scale=0.5
    → /camera/image_raw          640×360 RGB
rknn_yolov6         yolov6n_85.rknn，conf/nms=0.3
    → /camera/image_det
    → /ai_msg_det                COCO 80 类
object_track        最大 person 框；雷达投影测距
    → /camera/image_det_track
    → /cmd_vel                   仅 enable_tracking 时
img_encode          JPEG q=80
    → /camera/image_det_track/compressed
```

`usb_camera`：V4L2 `V4L2_PIX_FMT_MJPEG`，无订阅者不采图；会轮询 `video0/1/2`。

`img_rectify`：鱼眼 `fisheye.yml`，默认不启动。`audio` 拍照路径里校正开关为 `False`。

**跟随控制**（`object_track`）：

- 角速度：`kp_angular * (图像中心 x − 目标中心 x)`，限幅 `max_angular`（launch 里 3.0）
- 线速度：雷达点投影到相机，取目标框附近距离；`kp_linear * (d − keep_distance)`，`keep_distance=0.3 m`，限幅 0.3 m/s
- 无目标则停；关跟随时清零速度

---

### 5.5 雷达节点

三种实现都发布 `/scan`，`frame_id=laser_link`。

**YDLIDAR（X2 一类三角雷达）**：SDK 用户态；`isSingleChannel=true`；`support_motor_dtr=true`；转速主要靠 GPIO CTL，launch 里 `frequency` 注释为无效。提供 `/start_scan` `/stop_scan`。

**M1C1_MINI**：订阅 `/enable_lidar`，串口 `A5 F0` 开转、`A5 F5` 停；插值到 800 点；量程 0.1–8 m。不提供 scan 服务。

**LD14**：乐动协议，默认口 `/dev/ttyS9`，需 remap 到 `/scan`。

电源始终由 STM32 `PB4` 经 `/enable_lidar` → `CmdData.enable_power` 控制。

---

### 5.6 `move_server` / `move_client_cmd`

闭环看 `odom`→`base_footprint` 的 TF，50 Hz 发 `/cmd_vel`。

1. 若 `goal_angle ≠ 0`：P 控制旋转（kp=5），输出限幅到 `angular_speed`，最小 0.5 rad/s；误差 &lt; 1° 连续 5 周期则结束旋转
2. 再按 `goal_distance` 直行，线速度同样限幅；公差 1 cm
3. Preempt：停车并返回 `result_flag=0`

`move_client_find_charger.cpp`：订 `/visualization_marker`，发 `/ar_pose` 与 `move_base`，**未进入默认 launch**，属于实验性回充。

---

### 5.7 `robot_state_publisher` + URDF

`newbot_urdf/urdf/newbot_urdf.urdf`。`joint_state_publisher` 在 urdf.launch 里注释掉了，轮子关节角默认不转。TF 静态部分（相机、雷达相对 `base_footprint`）仍由 URDF 提供，跟随依赖 `camera_link`↔`laser_link`。

---

### 5.8 导航栈（可选）

标准 ROS1 Navigation：

| launch | 内容 |
|--------|------|
| `robot_slam.launch` | `gmapping` + `move_base` |
| `robot_navigation.launch` | `map_server` + `amcl` + `move_base` |
| `blank_map_move_base.launch` | 空白地图 + `map`≡`odom` 静态 TF，测规划 |
| `explore.launch` | `explore_lite` 边界探索 |

`move_base`：全局 `global_planner/GlobalPlanner`，局部默认 DWA（可切 TEB）；`move_forward_only` 默认禁后退。

与机器人相关的关键限制（`config/robot/`）：

- `robot_radius: 0.07`（直径约 12 cm）
- `inflation_radius: 0.2`
- DWA `max_vel_x: 0.10`，`max_vel_theta: 2.0`

建图：`rosrun map_server map_saver -f map`（在包内 `maps/`）。

ROS2 对应：**Nav2** + **slam_toolbox**（或 Nav2 slam）+ AMCL 定位，参数需按差速小车重调，不能直接拷 YAML。

---

### 5.9 其它包

**`snowboy_wakeup`**：可选热词节点，`snowboy.launch` = `all.launch` + snowboy。默认交互走 CI-03T「小白你好 / 小白小白」，不是 Snowboy。

**`config/install.sh`**：ROS Noetic、YDLIDAR SDK、Python 依赖、PulseAudio 默认设备、overlay、拷 DTB。xBot 镜像/脚本应对齐设备节点，不必复刻 NFS 路径。

---

## 6. 离线命令词一览（`asr.cfg`）

行序 = CI-03T ID。执行列：W = `wakeup_process`，A = `audio`。

| key | 典型说法 | 执行 |
|-----|----------|------|
| `wakeup_uni` | 小白你好 | A：表情/提示，唤醒态 |
| `chatgpt` | 小白小白 | A：录音 + ASR + LLM |
| `ip_address` | 显示 IP | A：读 wlan0/eth0 |
| `scan` | 扫二维码 | A：相机扫码连 Wi-Fi |
| `forget_wifi_con` | 忘记网络 | A：删 Wi-Fi 并 reboot（无 IP 则 AP） |
| `stop` | 停止 | W：停跟随 + 清动作 |
| `forward_*` / `backward_*` | 前进/后退 N 厘米 | W：`do_move` |
| `left_*` / `right_*` | 左转/右转 N 度 | W：`do_move` |
| `voice_up` / `voice_down` | 音量 | A：Pulse ±10%（0–150） |
| `sing` | 唱歌 | A：本地 `sound/music` 随机 |
| `dance` | 跳舞 | A：放歌 + `/cmd_vel` 舞步 |
| `smile` | 微笑 | A：表情 |
| `charge` | 回去充电 | 仅 TTS（找桩未接默认图） |
| `good_night` / `good_morning` | 睡觉/早安 | A：表情 |
| `photo` | 拍照 | A：抓 JPEG，可选发邮件 |
| `tracking_person` / `cancel_tracking` | 跟我走 / 取消 | W |
| `high/medium/low/off_lidar` | 雷达档位 | W |
| `reboot` | 重启 | W |
| `mirror_mode` | 镜子模式 | A：圆屏镜像预览 |
| `dis_state_mode` | 显示状态 | A：电压/编码器/PWM/雷达/麦 |
| `quit` | 退出 | A：退预览类模式 |
| `wakeup_exit` | 退下 | A：TTS，不打断逻辑略特殊 |
| `cur_time` / `weather` | 时间/天气 | A |
| `sound` / `mute` | 开声/静音 | A：音量 100 / 0 |
| `open_lcd` / `close_lcd` | 开关屏 | A：表情模块 |

---

## 7. TF 树（默认开机）

```
odom
 └── base_footprint          ← base_control 积分
      └── （URDF）base_link 及外壳
           ├── camera_link
           └── laser_link
```

导航时增加 `map`：

- SLAM：`gmapping` 发 `map`→`odom`
- 纯规划测试：`static_transform_publisher` 使 `map`≡`odom`
- 定位：`amcl` 发 `map`→`odom`

---

## 8. 给 xBot ROS2 的移植建议

保持这些 **契约**，上层技能可以重写：

1. 底盘：`/cmd_vel`、`/odom`、`odom`→`base_footprint`、`/enable_lidar`、`/asr_id`、`/battery`；帧布局不变。
2. 雷达：`/scan` + `laser_link`。
3. 视觉：可继续「压缩采图 → 解码 → 推理 → 编码」，但消息类型用 ROS2 `sensor_msgs`；检测框可沿用 `ai_msgs` 或改 `vision_msgs/Detection2DArray`。
4. 语音：建议把「命令 ID → 技能」做成一张表 + 一个 dispatcher，避免再把逻辑堆进单个 Python 文件。
5. 导航：Nav2；DWA 参数按 12 cm 差速车重标定。
6. 增加 `cmd_vel` 仲裁、IMU（MPU6050）话题、以及规划中的定制功能，不要改 MCU 帧长/符号除非同步改文档与固件。

原厂实现里可视为 **非必须复刻** 的部分：Snowboy、讯飞/有道具体厂商、本地 mp3 曲库、找桩客户端、`img_rectify` 独立节点、硬编码云密钥。

必须在板级继续保证的（非 ROS）：`uart2-m0`、`uart9-m2`、`spi3-m0-cs0-spidev`，以及 LCD GPIO / 雷达 CTL 的用户态权限。
