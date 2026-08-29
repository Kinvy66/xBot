/**
 * @file task_status.c
 * @brief LED blink from link / power state
 */
#include "task_status.h"

#include "app_state.h"
#include "board_led.h"
#include "cmsis_os.h"

#define STATUS_PERIOD_MS  500U

void task_status(void *argument)
{
  (void)argument;

  for (;;) {
    AppPowerState power;
    uint8_t link_ok = app_state_get_link_ok();

    app_state_get_power(&power);

    if (!link_ok) {
      /* Waiting for SOC: 1 Hz toggle (works for either LED polarity) */
      board_led_toggle();
      osDelay(STATUS_PERIOD_MS);
    } else if (power.vbat_mv > 0 && power.vbat_mv < 6800) {
      board_led_on();
      osDelay(80);
      board_led_off();
      osDelay(STATUS_PERIOD_MS);
    } else {
      board_led_on();
      osDelay(50);
      board_led_off();
      osDelay(STATUS_PERIOD_MS);
    }
  }
}
