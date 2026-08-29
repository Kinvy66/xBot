/**
 * @file tb6612.hpp
 * @brief TB6612FNG single-channel H-bridge driver (pins bound at construction)
 *
 * Logic (PWM on PWMx):
 *   IN1=0 IN2=0 → coast (stop)
 *   IN1=1 IN2=0 → reverse (this project: negative speed)
 *   IN1=0 IN2=1 → forward (this project: positive speed)
 *   IN1=1 IN2=1 → short brake
 */
#ifndef XBOT_TB6612_HPP
#define XBOT_TB6612_HPP

#include "gpio_out.hpp"
#include "pwm_out.hpp"

#include <cstdint>

namespace bsp {

class Tb6612Motor {
public:
  /**
   * @param in1         TB6612 INx1 (AIN1 / BIN1)
   * @param in2         TB6612 INx2 (AIN2 / BIN2)
   * @param pwm         PWMx timer channel
   * @param invert_dir  true if this channel's forward wiring is mirrored
   */
  Tb6612Motor(GpioOut in1, GpioOut in2, PwmOut pwm, bool invert_dir = false)
      : in1_(in1), in2_(in2), pwm_(pwm), invert_dir_(invert_dir)
  {
  }

  void begin()
  {
    pwm_.start();
    coast();
  }

  /** Signed speed in timer compare units; clamped to [0, ARR]. */
  void set_speed(int16_t speed)
  {
    const uint32_t max_c = pwm_.arr();
    uint32_t duty;
    int16_t cmd = invert_dir_ ? static_cast<int16_t>(-speed) : speed;

    if (cmd == 0) {
      coast();
      return;
    }

    if (cmd > 0) {
      duty = static_cast<uint32_t>(cmd);
      if (duty > max_c) {
        duty = max_c;
      }
      /* forward: IN1=0, IN2=1 (matches newbot left / pin_map) */
      in1_.reset();
      in2_.set();
    } else {
      duty = static_cast<uint32_t>(-cmd);
      if (duty > max_c) {
        duty = max_c;
      }
      /* reverse: IN1=1, IN2=0 */
      in1_.set();
      in2_.reset();
    }

    pwm_.set_compare(duty);
  }

  void coast()
  {
    in1_.reset();
    in2_.reset();
    pwm_.set_compare(0);
  }

  void brake()
  {
    in1_.set();
    in2_.set();
    pwm_.set_compare(0);
  }

private:
  GpioOut in1_;
  GpioOut in2_;
  PwmOut pwm_;
  bool invert_dir_;
};

} // namespace bsp

#endif /* XBOT_TB6612_HPP */
