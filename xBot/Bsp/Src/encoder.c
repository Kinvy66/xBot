/**
* @file encoder.c
* @author Kinvy
* @email kinvy66@163.com
* @date: 2025/6/26 13:51
* @description: 
**/
#include "encoder.h"

void encoder_init(void)
{

}

void read_encoder(int* enc1, int* enc2)
{
    *enc1 =  -(short)__HAL_TIM_GET_COUNTER(&htim2);
    *enc2 =  (short)__HAL_TIM_GET_COUNTER(&htim3);
}