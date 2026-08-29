/**
 * @file lidar_power.c
 * @brief PB4 MOS for LiDAR 5V (Cube pin label: Lida)
 */
#include "lidar_power.h"

#include "main.h"
#include "cmsis_os.h"

#ifndef LIDAR_SELFTEST_MS
#define LIDAR_SELFTEST_MS  2000U
#endif

void lidar_power_set(uint8_t enable)
{
  HAL_GPIO_WritePin(Lida_GPIO_Port, Lida_Pin,
                    enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void lidar_power_selftest(void)
{
  lidar_power_set(1);
  osDelay(LIDAR_SELFTEST_MS);
  lidar_power_set(0);
}
