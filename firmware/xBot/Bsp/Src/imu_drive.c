/**
 * @file imu_drive.c
 * @brief Board IMU wrapper — init before FreeRTOS (see main USER CODE)
 */
#include "imu_drive.h"
#include "mpu6050.h"

static uint8_t s_ok;

int imu_drive_init(void)
{
  s_ok = 0;
  MPU6050_Init();
  if (MPU6050_ReadID() != 0U) {
    s_ok = 1;
    return 0;
  }
  return -1;
}

int imu_drive_ok(void)
{
  return (s_ok != 0U) ? 1 : 0;
}

int imu_drive_read(ImuSample *out)
{
  int16_t acc[3];
  int16_t gyro[3];
  int16_t temp;

  if (out == NULL || s_ok == 0U) {
    return -1;
  }

  MPU6050_ReadAcc(acc);
  MPU6050_ReadGyro(gyro);
  MPU6050_ReadTempRaw(&temp);

  out->ax = acc[0];
  out->ay = acc[1];
  out->az = acc[2];
  out->gx = gyro[0];
  out->gy = gyro[1];
  out->gz = gyro[2];
  out->temp_raw = temp;
  return 0;
}
