/**
* @file tb6612.h
* @author Kinvy
* @email kinvy66@163.com
* @date: 2025/6/26 
* @description: 
**/

#ifndef __TB6612_H
#define __TB6612_H
#include "main.h"

#define AIN1_H()    HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_SET)
#define AIN1_L()    HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_RESET)
#define AIN2_H()    HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_SET)
#define AIN2_L()    HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET)

#define BIN1_H()    HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_SET)
#define BIN1_L()    HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_RESET)
#define BIN2_H()    HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_SET)
#define BIN2_L()    HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_RESET)

void tb6612_init(void);
void tb6612_pwm_output(int pwm1, int pwm2);

#endif //__TB6612_H
