/**
 * @file gpio_out.hpp
 * @brief HAL GPIO pin binding (port + pin), hardware-agnostic for drivers
 */
#ifndef XBOT_GPIO_OUT_HPP
#define XBOT_GPIO_OUT_HPP

#include "stm32f1xx_hal.h"

namespace bsp {

struct GpioOut {
  GPIO_TypeDef *port{nullptr};
  uint16_t pin{0};

  constexpr GpioOut() = default;
  constexpr GpioOut(GPIO_TypeDef *p, uint16_t pin_mask) : port(p), pin(pin_mask) {}

  void write(bool high) const
  {
    if (port == nullptr) {
      return;
    }
    HAL_GPIO_WritePin(port, pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }

  void set() const { write(true); }
  void reset() const { write(false); }
};

} // namespace bsp

#endif /* XBOT_GPIO_OUT_HPP */
