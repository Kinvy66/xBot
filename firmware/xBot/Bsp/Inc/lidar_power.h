/**
 * @file lidar_power.h
 * @brief LiDAR 5V rail switch (PB4 MOS, Cube label Lida)
 *
 * High = power on. Default after MX_GPIO_Init is off.
 */
#ifndef XBOT_LIDAR_POWER_H
#define XBOT_LIDAR_POWER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void lidar_power_set(uint8_t enable);

/** Brief on for audible/visible spin, then off. Call once at boot. */
void lidar_power_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* XBOT_LIDAR_POWER_H */
