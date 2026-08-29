/**
 * @file task_chassis.h
 * @brief 20 ms chassis / SOC protocol task
 */
#ifndef XBOT_TASK_CHASSIS_H
#define XBOT_TASK_CHASSIS_H

#include "cmsis_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Peripheral bring-up for chassis (encoders, PWM, UART link). Call once before tasks. */
void task_chassis_init(void);

void task_chassis(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* XBOT_TASK_CHASSIS_H */
