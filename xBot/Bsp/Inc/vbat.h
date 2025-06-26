/**
* @file bat.h
* @author Kinvy
* @email kinvy66@163.com
* @date: 2025/6/26 
* @description: 
**/

#ifndef __BAT_H
#define __BAT_H
#include "main.h"
#include "adc.h"

void bat_init(void);
int get_bat_voltage();

#endif //__BAT_H
