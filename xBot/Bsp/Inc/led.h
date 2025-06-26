/**
* @file led.h
* @author Kinvy
* @email kinvy66@163.com
* @date: 2025/6/26 
* @description: 
**/

#ifndef __LED_H
#define __LED_H

#include "main.h"

#define LED_ON()       HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET)
#define LED_OFF()      HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET)
#define LED_TOGGLE()    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin)

#endif //__LED_H
