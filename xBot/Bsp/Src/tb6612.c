/**
* @file tb6612.c
* @author Kinvy
* @email kinvy66@163.com
* @date: 2025/6/26 13:52
* @description: 
**/
#include "tb6612.h"
#include "tim.h"

void tb6612_init()
{
    AIN1_L();
    AIN2_L();
    BIN1_L();
    BIN2_L();
}

void tb6612_pwm_output(int pwm1, int pwm2)
{
    //控制方向
    if(pwm1<0)
    {
        pwm1 = -pwm1;//变为正值
        AIN2_L();
        AIN1_H();
        // GPIO_ResetBits(GPIOB,GPIO_Pin_12);
        // GPIO_SetBits(GPIOB,GPIO_Pin_15);
    }
    else
    {
        AIN1_L();
        AIN2_H();
        // GPIO_SetBits(GPIOB,GPIO_Pin_12);
        // GPIO_ResetBits(GPIOB,GPIO_Pin_15);
    }


    if(pwm2<0)
    {
        pwm2 = -pwm2;//变为正值
        BIN1_L();
        BIN2_H();
        // GPIO_ResetBits(GPIOA,GPIO_Pin_12);
        // GPIO_SetBits(GPIOA,GPIO_Pin_15);
    }
    else
    {
        BIN1_H();
        BIN2_L();
        // GPIO_SetBits(GPIOA,GPIO_Pin_12);
        // GPIO_ResetBits(GPIOA,GPIO_Pin_15);
    }


    //输出到电机
    // TIM_SetCompare1(TIM1,pwm1); //左侧
    // TIM_SetCompare4(TIM1,pwm2); //右侧
    // __HAL_TIM_SET_COUNTER();
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pwm1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, pwm1);

}

