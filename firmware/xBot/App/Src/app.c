/**
 * @file app.c
 * @brief app_main: spawn worker tasks (keep Core/freertos.c thin)
 */
#include "app.h"

#include "app_state.h"
#include "board_led.h"
#include "task_chassis.h"
#include "task_sensor.h"
#include "task_status.h"

#include "cmsis_os.h"

#define CHASSIS_STACK_BYTES  (384U * 4U)
#define SENSOR_STACK_BYTES   (512U * 4U)
#define STATUS_STACK_BYTES   (256U * 4U)

static osThreadId_t create_task(osThreadFunc_t fn, const char *name,
                                uint32_t stack_bytes, osPriority_t prio)
{
  const osThreadAttr_t attr = {
      .name = name,
      .stack_size = stack_bytes,
      .priority = prio,
  };
  return osThreadNew(fn, NULL, &attr);
}

void app_main(void)
{
  osThreadId_t chassis_id;
  osThreadId_t sensor_id;
  osThreadId_t status_id;

  app_state_init();
  task_chassis_init();

  chassis_id = create_task(task_chassis, "chassis", CHASSIS_STACK_BYTES,
                           osPriorityAboveNormal);
  sensor_id = create_task(task_sensor, "sensor", SENSOR_STACK_BYTES,
                          osPriorityNormal);
  status_id = create_task(task_status, "status", STATUS_STACK_BYTES,
                          osPriorityLow);

  /* If status failed (heap), fall back to chassis-only LED via toggle in loop.
   * Fast double-blink here means create failed — visible even if polarity unclear. */
  if (chassis_id == NULL || sensor_id == NULL || status_id == NULL) {
    for (;;) {
      board_led_toggle();
      osDelay(100);
      board_led_toggle();
      osDelay(100);
      board_led_toggle();
      osDelay(600);
    }
  }
}
