"""SOC ↔ MCU binary frames (docs/soc_mcu_protocol.md)."""
from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Iterator

MCU_SIZE = 29
CMD_SIZE = 12
HEAD = b"DA"
TAIL = b"TA\r\n"
PERIOD_MS = 20

# DA + size + enc1/2 + vbat + ch/full/asr + imu_ok + ax..gz + TA\r\n
_MCU_STRUCT = struct.Struct("<2sBhhhBBBB6h4s")
_CMD_STRUCT = struct.Struct("<2sBhhB4s")


def _clamp_i16(value: int) -> int:
    return max(-32768, min(32767, int(value)))


@dataclass(frozen=True)
class McuData:
    encoder1: int
    encoder2: int
    vbat_mv: int
    charger_connected: int
    fully_charged: int
    asr_id: int
    imu_ok: int
    ax: int
    ay: int
    az: int
    gx: int
    gy: int
    gz: int
    raw: bytes

    def to_dict(self) -> dict:
        return {
            "encoder1": self.encoder1,
            "encoder2": self.encoder2,
            "vbat_mv": self.vbat_mv,
            "charger_connected": int(self.charger_connected),
            "fully_charged": int(self.fully_charged),
            "asr_id": self.asr_id,
            "imu_ok": int(self.imu_ok),
            "ax": self.ax,
            "ay": self.ay,
            "az": self.az,
            "gx": self.gx,
            "gy": self.gy,
            "gz": self.gz,
            "hex": self.raw.hex(" ").upper(),
        }


@dataclass
class CmdData:
    pwm1: int = 0
    pwm2: int = 0
    enable_power: int = 0

    def packed(self) -> bytes:
        pwm1 = _clamp_i16(self.pwm1)
        pwm2 = _clamp_i16(self.pwm2)
        enable = 1 if self.enable_power else 0
        return _CMD_STRUCT.pack(HEAD, CMD_SIZE, pwm1, pwm2, enable, TAIL)

    def to_dict(self) -> dict:
        raw = self.packed()
        return {
            "pwm1": _clamp_i16(self.pwm1),
            "pwm2": _clamp_i16(self.pwm2),
            "enable_power": 1 if self.enable_power else 0,
            "hex": raw.hex(" ").upper(),
        }


def pack_cmd(pwm1: int, pwm2: int, enable_power: int = 0) -> bytes:
    return CmdData(pwm1, pwm2, enable_power).packed()


def pack_mcu(
    encoder1: int = 0,
    encoder2: int = 0,
    vbat_mv: int = 0,
    charger_connected: int = 0,
    fully_charged: int = 0,
    asr_id: int = 0,
    imu_ok: int = 0,
    ax: int = 0,
    ay: int = 0,
    az: int = 0,
    gx: int = 0,
    gy: int = 0,
    gz: int = 0,
) -> bytes:
    return _MCU_STRUCT.pack(
        HEAD,
        MCU_SIZE,
        _clamp_i16(encoder1),
        _clamp_i16(encoder2),
        _clamp_i16(vbat_mv),
        1 if charger_connected else 0,
        1 if fully_charged else 0,
        int(asr_id) & 0xFF,
        1 if imu_ok else 0,
        _clamp_i16(ax),
        _clamp_i16(ay),
        _clamp_i16(az),
        _clamp_i16(gx),
        _clamp_i16(gy),
        _clamp_i16(gz),
        TAIL,
    )


def parse_mcu_frame(buf: bytes) -> McuData | None:
    if len(buf) != MCU_SIZE:
        return None
    (
        head,
        size,
        enc1,
        enc2,
        vbat,
        charger,
        full,
        asr,
        imu_ok,
        ax,
        ay,
        az,
        gx,
        gy,
        gz,
        tail,
    ) = _MCU_STRUCT.unpack(buf)
    if head != HEAD or size != MCU_SIZE or tail != TAIL:
        return None
    return McuData(
        enc1, enc2, vbat, charger, full, asr, imu_ok, ax, ay, az, gx, gy, gz, buf
    )


def extract_mcu_frames(buffer: bytearray) -> Iterator[McuData]:
    """Pull complete McuData frames from a rolling RX buffer."""
    while True:
        start = buffer.find(HEAD)
        if start < 0:
            if len(buffer) > MCU_SIZE:
                del buffer[:-1]
            return
        if start > 0:
            del buffer[:start]
        if len(buffer) < MCU_SIZE:
            return
        frame = bytes(buffer[:MCU_SIZE])
        parsed = parse_mcu_frame(frame)
        if parsed is None:
            del buffer[0]
            continue
        del buffer[:MCU_SIZE]
        yield parsed


if __name__ == "__main__":
    cmd = pack_cmd(1200, -1200, 1)
    assert cmd == bytes.fromhex("44 41 0C B0 04 50 FB 01 54 41 0D 0A"), cmd.hex()
    raw = pack_mcu(-24, 100, 12000, 1, 0, 0, 1, 10, 20, 16384, -1, 2, 3)
    assert len(raw) == MCU_SIZE == _MCU_STRUCT.size
    mcu = parse_mcu_frame(raw)
    assert mcu is not None
    assert mcu.encoder1 == -24 and mcu.encoder2 == 100 and mcu.vbat_mv == 12000
    assert mcu.imu_ok == 1 and mcu.az == 16384 and mcu.gz == 3
    buf = bytearray(b"xx" + raw + raw[:7])
    frames = list(extract_mcu_frames(buf))
    assert len(frames) == 1 and len(buf) == 7
    print("protocol ok", MCU_SIZE)
