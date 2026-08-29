/**
 * @file imu_drive.h
 * @brief Board IMU API (C) — MPU6050 on I2C1 PB6/PB7
 */
#ifndef XBOT_IMU_DRIVE_H
#define XBOT_IMU_DRIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t gx;
  int16_t gy;
  int16_t gz;
  int16_t temp_raw;
} ImuSample;

/** Probe + configure. Prefer calling once from main before osKernelStart. */
int imu_drive_init(void);

int imu_drive_ok(void);

int imu_drive_read(ImuSample *out);

#ifdef __cplusplus
}
#endif

#endif /* XBOT_IMU_DRIVE_H */
