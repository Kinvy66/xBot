/**
 * @file task_chassis.c
 * @brief SOC protocol + motor + encoder @ 20 ms
 */
#include "task_chassis.h"

#include "app_state.h"
#include "motor_drive.h"
#include "soc_link.h"
#include "soc_protocol.h"

#include "main.h"
#include "tim.h"
#include "usart.h"

static McuData s_mcu_data;
static int16_t s_pwm1;
static int16_t s_pwm2;
static uint8_t s_enable_power;
static uint8_t s_last_enable_power;
static uint8_t s_heartbeat_miss;
static uint8_t s_asr_rx_byte;

static void lidar_power_apply(uint8_t enable)
{
  (void)enable; /* PB4 pending CubeMX GPIO */
}

static void read_encoders(int16_t *enc1, int16_t *enc2)
{
  *enc1 = -(int16_t)__HAL_TIM_GET_COUNTER(&htim2);
  *enc2 = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  __HAL_TIM_SET_COUNTER(&htim3, 0);
}

void task_chassis_init(void)
{
  soc_protocol_init_mcu_data(&s_mcu_data);
  s_pwm1 = 0;
  s_pwm2 = 0;
  s_enable_power = 0;
  s_last_enable_power = 0;
  s_heartbeat_miss = SOC_PROTO_HEARTBEAT_MISS;

  (void)HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  (void)HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);

  motor_drive_init();

  soc_link_init(&huart1);
  soc_link_start();
  (void)HAL_UART_Receive_IT(&huart2, &s_asr_rx_byte, 1);
}

void task_chassis(void *argument)
{
  uint32_t tick = osKernelGetTickCount();
  (void)argument;

  for (;;) {
    CmdData cmd;
    int16_t enc1;
    int16_t enc2;
    AppPowerState power;
    uint8_t got_cmd;

    got_cmd = (uint8_t)soc_link_take_cmd(&cmd);
    if (got_cmd) {
      s_pwm1 = cmd.pwm1;
      s_pwm2 = cmd.pwm2;
      s_enable_power = cmd.enable_power;
      s_heartbeat_miss = 0;
      app_state_set_link_ok(1);
    } else if (s_heartbeat_miss < 255U) {
      s_heartbeat_miss++;
    }

    if (s_heartbeat_miss >= SOC_PROTO_HEARTBEAT_MISS) {
      s_pwm1 = 0;
      s_pwm2 = 0;
      app_state_set_link_ok(0);
    }

    if (s_enable_power != s_last_enable_power) {
      lidar_power_apply(s_enable_power);
      s_last_enable_power = s_enable_power;
    }

    read_encoders(&enc1, &enc2);
    s_mcu_data.encoder1 = enc1;
    s_mcu_data.encoder2 = enc2;
    s_mcu_data.asr_id = soc_link_take_asr_id();

    app_state_get_power(&power);
    s_mcu_data.vbat_mv = power.vbat_mv;
    s_mcu_data.charger_connected = power.charger_connected;
    s_mcu_data.fully_charged = power.fully_charged;

    {
      AppImuState imu;
      app_state_get_imu(&imu);
      s_mcu_data.imu_ok = imu.ok;
      s_mcu_data.ax = imu.ax;
      s_mcu_data.ay = imu.ay;
      s_mcu_data.az = imu.az;
      s_mcu_data.gx = imu.gx;
      s_mcu_data.gy = imu.gy;
      s_mcu_data.gz = imu.gz;
    }

    motor_drive_set(s_pwm1, s_pwm2);
    (void)soc_link_send_mcu_data(&s_mcu_data);

    tick += SOC_PROTO_PERIOD_MS;
    osDelayUntil(tick);
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2) {
    soc_link_push_asr_id(s_asr_rx_byte);
    (void)HAL_UART_Receive_IT(&huart2, &s_asr_rx_byte, 1);
  }
}
