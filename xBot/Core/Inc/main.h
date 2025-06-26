/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
uint32_t DWT_Delay_Init(void);
void DWT_Delay_us(volatile uint32_t microseconds);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ENC_A2_Pin GPIO_PIN_0
#define ENC_A2_GPIO_Port GPIOA
#define ENC_B2_Pin GPIO_PIN_1
#define ENC_B2_GPIO_Port GPIOA
#define BAT_Pin GPIO_PIN_4
#define BAT_GPIO_Port GPIOA
#define VUSB_Pin GPIO_PIN_5
#define VUSB_GPIO_Port GPIOA
#define ENC_A1_Pin GPIO_PIN_6
#define ENC_A1_GPIO_Port GPIOA
#define ENC_B1_Pin GPIO_PIN_7
#define ENC_B1_GPIO_Port GPIOA
#define AIN2_Pin GPIO_PIN_12
#define AIN2_GPIO_Port GPIOB
#define AIN1_Pin GPIO_PIN_15
#define AIN1_GPIO_Port GPIOB
#define PWMA_Pin GPIO_PIN_8
#define PWMA_GPIO_Port GPIOA
#define PWMB_Pin GPIO_PIN_11
#define PWMB_GPIO_Port GPIOA
#define BIN1_Pin GPIO_PIN_12
#define BIN1_GPIO_Port GPIOA
#define BIN2_Pin GPIO_PIN_15
#define BIN2_GPIO_Port GPIOA
#define LED_Pin GPIO_PIN_3
#define LED_GPIO_Port GPIOB
#define LiDAR_EN_Pin GPIO_PIN_4
#define LiDAR_EN_GPIO_Port GPIOB
#define MPU_SCL_Pin GPIO_PIN_8
#define MPU_SCL_GPIO_Port GPIOB
#define MPU_SDA_Pin GPIO_PIN_9
#define MPU_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define HAL_Delay_us(us)   DWT_Delay_us(us)
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
