/**
 * @file soc_link.h
 * @brief USART1 transport for SOC protocol (RX CmdData / TX McuData)
 */
#ifndef XBOT_SOC_LINK_H
#define XBOT_SOC_LINK_H

#include "soc_protocol.h"
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

void soc_link_init(UART_HandleTypeDef *huart);
void soc_link_start(void);

/** @return 1 if a new valid CmdData was taken since last call */
int soc_link_take_cmd(CmdData *out);

HAL_StatusTypeDef soc_link_send_mcu_data(const McuData *data);

/** Push one ASR byte from USART2 (CI-03T); consumed once by take_asr_id */
void soc_link_push_asr_id(uint8_t id);

/** @return asr_id if new since last take, else 0 */
uint8_t soc_link_take_asr_id(void);

#ifdef __cplusplus
}
#endif

#endif /* XBOT_SOC_LINK_H */
