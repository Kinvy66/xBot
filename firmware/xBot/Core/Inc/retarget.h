/**
 * @file retarget.h
 * @brief
 * @date 2026/6/11
 * @author Kinvy
 * @details 
 */

#ifndef XBOT_RETARGET_H
#define XBOT_RETARGET_H

#include "stm32f1xx_hal.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>

void RetargetInit(UART_HandleTypeDef *huart);

int _isatty(int fd);

int _write(int fd, char *ptr, int len);

int _close(int fd);

int _lseek(int fd, int ptr, int dir);

int _read(int fd, char *ptr, int len);

int _fstat(int fd, struct stat *st);


#endif //XBOT_RETARGET_H
