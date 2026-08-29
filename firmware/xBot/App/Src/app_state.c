/**
 * @file app_state.c
 * @brief Shared state with short critical sections
 */
#include "app_state.h"
#include "main.h"

static AppPowerState s_power;
static volatile uint8_t s_link_ok;

void app_state_init(void)
{
  s_power.vbat_mv = 0;
  s_power.charger_connected = 0;
  s_power.fully_charged = 0;
  s_link_ok = 0;
}

void app_state_set_power(int16_t vbat_mv, uint8_t charger_connected,
                         uint8_t fully_charged)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  s_power.vbat_mv = vbat_mv;
  s_power.charger_connected = charger_connected;
  s_power.fully_charged = fully_charged;
  if (!primask) {
    __enable_irq();
  }
}

void app_state_get_power(AppPowerState *out)
{
  uint32_t primask;
  if (out == NULL) {
    return;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  *out = s_power;
  if (!primask) {
    __enable_irq();
  }
}

void app_state_set_link_ok(uint8_t ok)
{
  s_link_ok = (ok != 0U) ? 1U : 0U;
}

uint8_t app_state_get_link_ok(void)
{
  return s_link_ok;
}
