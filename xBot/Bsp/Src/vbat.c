/**
* @file bat.c
* @author Kinvy
* @email kinvy66@163.com
* @date: 2025/6/26 17:02
* @description: 
**/
#include "vbat.h"
#define R1_R2_DIV_R2 2

void bat_init(void)
{
    HAL_ADC_Start(&hadc1);
}

int get_bat_voltage()
{
    int voltage = 0;
    uint16_t adc_value = HAL_ADC_GetValue(&hadc1);
    voltage = adc_value * 3.3 * 1000 / 4096 * R1_R2_DIV_R2;
    return voltage;
}