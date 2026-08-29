/**
 * @file mpu6050.h
 * @brief MPU6050 (same API/logic as ref/MPU_Test)
 */
#ifndef XBOT_MPU6050_H
#define XBOT_MPU6050_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPU6050_ADDRESS           (0x68U << 1)

#define MPU6050_RA_SMPLRT_DIV     0x19
#define MPU6050_RA_CONFIG         0x1A
#define MPU6050_RA_GYRO_CONFIG    0x1B
#define MPU6050_RA_ACCEL_CONFIG   0x1C
#define MPU6050_ACC_OUT           0x3B
#define MPU6050_RA_TEMP_OUT_H     0x41
#define MPU6050_GYRO_OUT          0x43
#define MPU6050_RA_PWR_MGMT_1     0x6B
#define MPU6050_RA_WHO_AM_I       0x75

void MPU6050_WriteReg(uint8_t reg_add, uint8_t reg_dat);
void MPU6050_ReadData(uint8_t reg_add, uint8_t *Read, uint8_t num);
void MPU6050_Init(void);
uint8_t MPU6050_ReadID(void);
void MPU6050_ReadAcc(int16_t *accData);
void MPU6050_ReadGyro(int16_t *gyroData);
void MPU6050_ReadTempRaw(int16_t *tempData);

#ifdef __cplusplus
}
#endif

#endif /* XBOT_MPU6050_H */
