/**
 * @file motor_drive.cpp
 * @brief Instantiate TB6612 motors with this board's pin map
 */
#include "motor_drive.h"

#include "tb6612.hpp"

#include "main.h"
#include "tim.h"

using bsp::GpioOut;
using bsp::PwmOut;
using bsp::Tb6612Motor;

/* Pin binding happens only here — driver stays hardware-agnostic. */
static Tb6612Motor s_left(
    GpioOut(AIN1_GPIO_Port, AIN1_Pin),
    GpioOut(AIN2_GPIO_Port, AIN2_Pin),
    PwmOut(&htim1, TIM_CHANNEL_1),
    false);

/* Right channel IN polarity is mirrored vs left on this board. */
static Tb6612Motor s_right(
    GpioOut(BIN1_GPIO_Port, BIN1_Pin),
    GpioOut(BIN2_GPIO_Port, BIN2_Pin),
    PwmOut(&htim1, TIM_CHANNEL_4),
    true);

extern "C" void motor_drive_init(void)
{
  s_left.begin();
  s_right.begin();
}

extern "C" void motor_drive_set(int16_t left, int16_t right)
{
  s_left.set_speed(left);
  s_right.set_speed(right);
}

extern "C" void motor_drive_coast(void)
{
  s_left.coast();
  s_right.coast();
}
