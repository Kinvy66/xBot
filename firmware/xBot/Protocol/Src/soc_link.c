/**
 * @file soc_link.c
 * @brief USART1 idle-DMA RX + TX for SOC protocol frames
 */
#include "soc_link.h"
#include <string.h>

#define SOC_LINK_RX_BUF_SIZE  64U

static UART_HandleTypeDef *s_huart;
static uint8_t s_rx_dma_buf[SOC_LINK_RX_BUF_SIZE];

static CmdData s_cmd;
static volatile uint8_t s_cmd_ready;

static volatile uint8_t s_asr_id;
static volatile uint8_t s_asr_ready;

static void soc_link_restart_rx(void)
{
  if (s_huart == NULL) {
    return;
  }
  (void)HAL_UARTEx_ReceiveToIdle_DMA(s_huart, s_rx_dma_buf, SOC_LINK_RX_BUF_SIZE);
  /* Disable half-transfer IRQ noise on F1 DMA */
  if (s_huart->hdmarx != NULL) {
    __HAL_DMA_DISABLE_IT(s_huart->hdmarx, DMA_IT_HT);
  }
}

void soc_link_init(UART_HandleTypeDef *huart)
{
  s_huart = huart;
  s_cmd_ready = 0;
  s_asr_ready = 0;
  memset(&s_cmd, 0, sizeof(s_cmd));
}

void soc_link_start(void)
{
  soc_link_restart_rx();
}

int soc_link_take_cmd(CmdData *out)
{
  uint32_t primask;
  int ready;

  if (out == NULL) {
    return 0;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  ready = s_cmd_ready;
  if (ready) {
    *out = s_cmd;
    s_cmd_ready = 0;
  }
  if (!primask) {
    __enable_irq();
  }
  return ready;
}

HAL_StatusTypeDef soc_link_send_mcu_data(const McuData *data)
{
  if (s_huart == NULL || data == NULL) {
    return HAL_ERROR;
  }
  return HAL_UART_Transmit(s_huart, (uint8_t *)data, sizeof(McuData), SOC_PROTO_PERIOD_MS);
}

void soc_link_push_asr_id(uint8_t id)
{
  s_asr_id = id;
  s_asr_ready = 1;
}

uint8_t soc_link_take_asr_id(void)
{
  uint32_t primask;
  uint8_t id = 0;

  primask = __get_PRIMASK();
  __disable_irq();
  if (s_asr_ready) {
    id = s_asr_id;
    s_asr_ready = 0;
  }
  if (!primask) {
    __enable_irq();
  }
  return id;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  CmdData cmd;

  if (huart != s_huart) {
    return;
  }

  if (soc_protocol_parse_cmd(s_rx_dma_buf, Size, &cmd)) {
    s_cmd = cmd;
    s_cmd_ready = 1;
  }

  soc_link_restart_rx();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == s_huart) {
    __HAL_UART_CLEAR_OREFLAG(huart);
    soc_link_restart_rx();
  }
}
