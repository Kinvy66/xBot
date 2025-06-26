/**
* @file encoder.h
* @author Kinvy
* @email kinvy66@163.com
* @date: 2025/6/26 
* @description: 
**/

#ifndef __ENCODER_H
#define __ENCODER_H

#include "main.h"
#include "tim.h"

void encoder_init(void);
void read_encoder(int* enc1, int* enc2);

#endif //__ENCODER_H
