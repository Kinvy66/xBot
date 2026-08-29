/**
 * @file app.h
 * @brief Application entry — create worker tasks from app_main
 */
#ifndef XBOT_APP_H
#define XBOT_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Boot entry run by the single Cube-created RTOS thread.
 * Initializes app state, creates chassis / sensor / status tasks, then exits.
 */
void app_main(void);

#ifdef __cplusplus
}
#endif

#endif /* XBOT_APP_H */
