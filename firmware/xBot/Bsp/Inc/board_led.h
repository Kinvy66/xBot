/**
 * @file board_led.h
 * @brief Board status LED (PB3)
 *
 * Polarity is board-dependent; prefer board_led_toggle() for heartbeats.
 * If the LED is active-low: on()=LOW, off()=HIGH.
 */
#ifndef XBOT_BOARD_LED_H
#define XBOT_BOARD_LED_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BOARD_LED_ACTIVE_LOW
#define BOARD_LED_ACTIVE_LOW  1
#endif

static inline void board_led_on(void)
{
#if BOARD_LED_ACTIVE_LOW
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
#else
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
#endif
}

static inline void board_led_off(void)
{
#if BOARD_LED_ACTIVE_LOW
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
#else
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
#endif
}

static inline void board_led_toggle(void)
{
  HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}

#ifdef __cplusplus
}
#endif

#endif /* XBOT_BOARD_LED_H */
