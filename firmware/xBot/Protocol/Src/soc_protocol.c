/**
 * @file soc_protocol.c
 * @brief Frame validate / init for SOC ↔ MCU protocol
 */
#include "soc_protocol.h"
#include <string.h>

void soc_protocol_init_mcu_data(McuData *data)
{
  if (data == NULL) {
    return;
  }
  memset(data, 0, sizeof(*data));
  data->head1 = SOC_PROTO_HEAD1;
  data->head2 = SOC_PROTO_HEAD2;
  data->struct_size = (uint8_t)sizeof(McuData);
  data->end1 = SOC_PROTO_END1;
  data->end2 = SOC_PROTO_END2;
  data->end3 = SOC_PROTO_END3;
  data->end4 = SOC_PROTO_END4;
}

int soc_protocol_parse_cmd(const uint8_t *buf, uint16_t len, CmdData *out)
{
  const CmdData *cmd;

  if (buf == NULL || out == NULL) {
    return 0;
  }
  if (len != sizeof(CmdData)) {
    return 0;
  }
  if (buf[0] != SOC_PROTO_HEAD1 || buf[1] != SOC_PROTO_HEAD2) {
    return 0;
  }
  if (buf[2] != len) {
    return 0;
  }

  cmd = (const CmdData *)buf;
  if (cmd->end1 != SOC_PROTO_END1 || cmd->end2 != SOC_PROTO_END2 ||
      cmd->end3 != SOC_PROTO_END3 || cmd->end4 != SOC_PROTO_END4) {
    return 0;
  }

  memcpy(out, cmd, sizeof(CmdData));
  return 1;
}
