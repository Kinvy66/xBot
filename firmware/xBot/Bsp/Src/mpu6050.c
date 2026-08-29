/**
 * @file mpu6050.c
 * @brief Copied from ref/MPU_Test (proven on PB6/PB7 I2C1)
 */
#include "mpu6050.h"
#include "i2c.h"

#define I2C_TIMEOUT_MS  100U

static void i2c_wait_ready(void)
{
  uint32_t t0 = HAL_GetTick();
  while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY) {
    if ((HAL_GetTick() - t0) > I2C_TIMEOUT_MS) {
      break;
    }
  }
}

void MPU6050_WriteReg(uint8_t reg_add, uint8_t reg_dat)
{
  HAL_StatusTypeDef st;

  st = HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDRESS, reg_add, I2C_MEMADD_SIZE_8BIT,
                         &reg_dat, 1, I2C_TIMEOUT_MS);
  if (st != HAL_OK) {
    (void)HAL_I2C_DeInit(&hi2c1);
    MX_I2C1_Init();
    (void)HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDRESS, reg_add, I2C_MEMADD_SIZE_8BIT,
                            &reg_dat, 1, I2C_TIMEOUT_MS);
  }
  i2c_wait_ready();
}

void MPU6050_ReadData(uint8_t reg_add, uint8_t *Read, uint8_t num)
{
  HAL_StatusTypeDef st;

  st = HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDRESS, reg_add, I2C_MEMADD_SIZE_8BIT,
                        Read, num, I2C_TIMEOUT_MS);
  if (st != HAL_OK) {
    (void)HAL_I2C_DeInit(&hi2c1);
    MX_I2C1_Init();
    (void)HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDRESS, reg_add, I2C_MEMADD_SIZE_8BIT,
                           Read, num, I2C_TIMEOUT_MS);
  }
  i2c_wait_ready();
}

void MPU6050_Init(void)
{
  HAL_Delay(100);
  MPU6050_WriteReg(MPU6050_RA_PWR_MGMT_1, 0x00);
  MPU6050_WriteReg(MPU6050_RA_SMPLRT_DIV, 0x07);
  MPU6050_WriteReg(MPU6050_RA_CONFIG, 0x06);
  MPU6050_WriteReg(MPU6050_RA_ACCEL_CONFIG, 0x01);
  MPU6050_WriteReg(MPU6050_RA_GYRO_CONFIG, 0x18);
  HAL_Delay(200);
}

uint8_t MPU6050_ReadID(void)
{
  uint8_t id = 0;

  MPU6050_ReadData(MPU6050_RA_WHO_AM_I, &id, 1);
  return (id == 0x68U) ? 1U : 0U;
}

void MPU6050_ReadAcc(int16_t *accData)
{
  uint8_t buf[6];

  MPU6050_ReadData(MPU6050_ACC_OUT, buf, 6);
  accData[0] = (int16_t)((buf[0] << 8) | buf[1]);
  accData[1] = (int16_t)((buf[2] << 8) | buf[3]);
  accData[2] = (int16_t)((buf[4] << 8) | buf[5]);
}

void MPU6050_ReadGyro(int16_t *gyroData)
{
  uint8_t buf[6];

  MPU6050_ReadData(MPU6050_GYRO_OUT, buf, 6);
  gyroData[0] = (int16_t)((buf[0] << 8) | buf[1]);
  gyroData[1] = (int16_t)((buf[2] << 8) | buf[3]);
  gyroData[2] = (int16_t)((buf[4] << 8) | buf[5]);
}

void MPU6050_ReadTempRaw(int16_t *tempData)
{
  uint8_t buf[2];

  MPU6050_ReadData(MPU6050_RA_TEMP_OUT_H, buf, 2);
  *tempData = (int16_t)((buf[0] << 8) | buf[1]);
}
