/**
 * @file motor_drive.h
 * @brief Board motor API (C) over TB6612 C++ drivers
 */
#ifndef XBOT_MOTOR_DRIVE_H
#define XBOT_MOTOR_DRIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bind pins / start PWM (call once before set). */
void motor_drive_init(void);

/**
 * @param left   pwm1 / encoder1 side (AIN + TIM1_CH1)
 * @param right  pwm2 / encoder2 side (BIN + TIM1_CH4)
 */
void motor_drive_set(int16_t left, int16_t right);

void motor_drive_coast(void);

#ifdef __cplusplus
}
#endif

#endif /* XBOT_MOTOR_DRIVE_H */
