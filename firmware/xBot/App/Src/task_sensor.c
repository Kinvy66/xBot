/**
 * @file task_sensor.c
 * @brief Battery ADC @ ~100 ms; IMU read (init done in main)
 */
#include "task_sensor.h"

#include "app_state.h"
#include "imu_drive.h"
#include "adc.h"
#include "cmsis_os.h"
#include "main.h"

#define SENSOR_PERIOD_MS  100U
#define VBAT_AVG_N        8U
#define VBAT_DIV_SCALE    2U

static uint16_t adc_read_mv(uint32_t channel)
{
  ADC_ChannelConfTypeDef cfg = {0};
  uint32_t raw;

  cfg.Channel = channel;
  cfg.Rank = ADC_REGULAR_RANK_1;
  cfg.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &cfg) != HAL_OK) {
    return 0;
  }
  if (HAL_ADC_Start(&hadc1) != HAL_OK) {
    return 0;
  }
  if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
    return 0;
  }
  raw = HAL_ADC_GetValue(&hadc1);
  (void)HAL_ADC_Stop(&hadc1);

  return (uint16_t)((raw * 3300U) / 4095U);
}

void task_sensor(void *argument)
{
  uint32_t tick = osKernelGetTickCount();
  uint32_t sum_vbat = 0;
  uint32_t sum_vusb = 0;
  uint8_t n = 0;
  (void)argument;

  for (;;) {
    uint16_t pin_mv = adc_read_mv(ADC_CHANNEL_4);
    uint16_t vusb_mv = adc_read_mv(ADC_CHANNEL_5);

    sum_vbat += (uint32_t)pin_mv * VBAT_DIV_SCALE;
    sum_vusb += vusb_mv;
    n++;

    if (n >= VBAT_AVG_N) {
      int16_t vbat = (int16_t)(sum_vbat / VBAT_AVG_N);
      uint8_t charger = (sum_vusb / VBAT_AVG_N >= 500U) ? 1U : 0U;
      app_state_set_power(vbat, charger, 0U);
      sum_vbat = 0;
      sum_vusb = 0;
      n = 0;
    }

    /* IMU was initialized in main(); only sample here — never block ADC */
    if (imu_drive_ok()) {
      ImuSample s;
      AppImuState st;
      if (imu_drive_read(&s) == 0) {
        st.ax = s.ax;
        st.ay = s.ay;
        st.az = s.az;
        st.gx = s.gx;
        st.gy = s.gy;
        st.gz = s.gz;
        st.temp_raw = s.temp_raw;
        st.ok = 1;
        app_state_set_imu(&st);
      } else {
        st.ax = st.ay = st.az = 0;
        st.gx = st.gy = st.gz = 0;
        st.temp_raw = 0;
        st.ok = 0;
        app_state_set_imu(&st);
      }
    } else {
      AppImuState st = {0};
      app_state_set_imu(&st);
    }

    tick += SENSOR_PERIOD_MS;
    osDelayUntil(tick);
  }
}
