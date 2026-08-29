/**
 * @file app_state.h
 * @brief Shared app state between tasks (power, IMU, link health)
 */
#ifndef XBOT_APP_STATE_H
#define XBOT_APP_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  int16_t vbat_mv;
  uint8_t charger_connected;
  uint8_t fully_charged;
} AppPowerState;

typedef struct
{
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t gx;
  int16_t gy;
  int16_t gz;
  int16_t temp_raw;
  uint8_t ok;
} AppImuState;

void app_state_init(void);

void app_state_set_power(int16_t vbat_mv, uint8_t charger_connected,
                         uint8_t fully_charged);
void app_state_get_power(AppPowerState *out);

void app_state_set_imu(const AppImuState *in);
void app_state_get_imu(AppImuState *out);

/** chassis updates after each SOC period; status task reads for LED */
void app_state_set_link_ok(uint8_t ok);
uint8_t app_state_get_link_ok(void);

#ifdef __cplusplus
}
#endif

#endif /* XBOT_APP_STATE_H */
