/**
 * @file pwm_out.hpp
 * @brief HAL timer PWM channel binding
 */
#ifndef XBOT_PWM_OUT_HPP
#define XBOT_PWM_OUT_HPP

#include "stm32f1xx_hal.h"

namespace bsp {

struct PwmOut {
  TIM_HandleTypeDef *htim{nullptr};
  uint32_t channel{0};

  constexpr PwmOut() = default;
  constexpr PwmOut(TIM_HandleTypeDef *t, uint32_t ch) : htim(t), channel(ch) {}

  void start() const
  {
    if (htim != nullptr) {
      (void)HAL_TIM_PWM_Start(htim, channel);
    }
  }

  uint32_t arr() const
  {
    if (htim == nullptr) {
      return 0;
    }
    return __HAL_TIM_GET_AUTORELOAD(htim);
  }

  void set_compare(uint32_t ccr) const
  {
    if (htim == nullptr) {
      return;
    }
    __HAL_TIM_SET_COMPARE(htim, channel, ccr);
  }
};

} // namespace bsp

#endif /* XBOT_PWM_OUT_HPP */
