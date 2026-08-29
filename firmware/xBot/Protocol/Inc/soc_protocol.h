/**
 * @file soc_protocol.h
 * @brief SOC ↔ STM32 frame structs (docs/soc_mcu_protocol.md)
 */
#ifndef XBOT_SOC_PROTOCOL_H
#define XBOT_SOC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOC_PROTO_HEAD1           ((uint8_t)'D')
#define SOC_PROTO_HEAD2           ((uint8_t)'A')
#define SOC_PROTO_END1            ((uint8_t)'T')
#define SOC_PROTO_END2            ((uint8_t)'A')
#define SOC_PROTO_END3            ((uint8_t)'\r')
#define SOC_PROTO_END4            ((uint8_t)'\n')

#define SOC_PROTO_MCU_DATA_SIZE   16U
#define SOC_PROTO_CMD_DATA_SIZE   12U

#define SOC_PROTO_PERIOD_MS       20U
#define SOC_PROTO_HEARTBEAT_MISS  5U   /* 5 * 20ms ≈ 100ms → PWM stop */

#pragma pack(push, 1)

typedef struct
{
  uint8_t head1;
  uint8_t head2;
  uint8_t struct_size;

  int16_t encoder1;
  int16_t encoder2;

  int16_t vbat_mv;
  uint8_t charger_connected;
  uint8_t fully_charged;

  uint8_t asr_id;

  uint8_t end1;
  uint8_t end2;
  uint8_t end3;
  uint8_t end4;
} McuData;

typedef struct
{
  uint8_t head1;
  uint8_t head2;
  uint8_t struct_size;

  int16_t pwm1;
  int16_t pwm2;
  uint8_t enable_power;

  uint8_t end1;
  uint8_t end2;
  uint8_t end3;
  uint8_t end4;
} CmdData;

#pragma pack(pop)

_Static_assert(sizeof(McuData) == SOC_PROTO_MCU_DATA_SIZE, "McuData size");
_Static_assert(sizeof(CmdData) == SOC_PROTO_CMD_DATA_SIZE, "CmdData size");

void soc_protocol_init_mcu_data(McuData *data);
int soc_protocol_parse_cmd(const uint8_t *buf, uint16_t len, CmdData *out);

#ifdef __cplusplus
}
#endif

#endif /* XBOT_SOC_PROTOCOL_H */
