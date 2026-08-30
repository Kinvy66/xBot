#!/usr/bin/env python3
"""STM32 UART bring-up on Orange Pi (/dev/ttyS2) — no ROS2.

Reads McuData (29 B), sends CmdData (12 B) every 20 ms.
Sequence: listen → radar ON → radar OFF.

Protocol: docs/soc_mcu_protocol.md
Requires: pyserial  (pip3 install --user pyserial  /  apt install python3-serial)

  python3 orangepi_stm32_uart_test.py
  python3 orangepi_stm32_uart_test.py --port /dev/ttyS2 --listen 3 --radar-on 5 --radar-off 2
"""
from __future__ import annotations

import argparse
import struct
import sys
import time
from dataclasses import dataclass
from typing import Iterator

MCU_SIZE = 29
CMD_SIZE = 12
HEAD = b"DA"
TAIL = b"TA\r\n"
PERIOD_S = 0.02

_MCU = struct.Struct("<2sBhhhBBBB6h4s")
_CMD = struct.Struct("<2sBhhB4s")


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

    def summary(self) -> str:
        return (
            f"enc={self.encoder1:+6d}/{self.encoder2:+6d}  "
            f"vbat={self.vbat_mv}mV  chg={self.charger_connected}/{self.fully_charged}  "
            f"asr={self.asr_id}  imu={self.imu_ok} "
            f"a=({self.ax},{self.ay},{self.az}) g=({self.gx},{self.gy},{self.gz})"
        )


def pack_cmd(pwm1: int = 0, pwm2: int = 0, enable_power: int = 0) -> bytes:
    return _CMD.pack(
        HEAD,
        CMD_SIZE,
        max(-32768, min(32767, int(pwm1))),
        max(-32768, min(32767, int(pwm2))),
        1 if enable_power else 0,
        TAIL,
    )


def parse_mcu(buf: bytes) -> McuData | None:
    if len(buf) != MCU_SIZE:
        return None
    (
        head,
        size,
        e1,
        e2,
        vbat,
        ch,
        full,
        asr,
        imu,
        ax,
        ay,
        az,
        gx,
        gy,
        gz,
        tail,
    ) = _MCU.unpack(buf)
    if head != HEAD or size != MCU_SIZE or tail != TAIL:
        return None
    return McuData(e1, e2, vbat, ch, full, asr, imu, ax, ay, az, gx, gy, gz, buf)


def extract_frames(buf: bytearray) -> Iterator[McuData]:
    while True:
        start = buf.find(HEAD)
        if start < 0:
            if len(buf) > MCU_SIZE:
                del buf[:-1]
            return
        if start:
            del buf[:start]
        if len(buf) < MCU_SIZE:
            return
        frame = bytes(buf[:MCU_SIZE])
        parsed = parse_mcu(frame)
        if parsed is None:
            del buf[0]
            continue
        del buf[:MCU_SIZE]
        yield parsed


def phase(ser, rx: bytearray, enable_power: int, seconds: float, label: str) -> int:
    print(f"\n=== {label} (enable_power={enable_power}, {seconds:.1f}s) ===")
    t_end = time.monotonic() + seconds
    n = 0
    last_print = 0.0
    cmd = pack_cmd(0, 0, enable_power)
    while time.monotonic() < t_end:
        t0 = time.monotonic()
        ser.write(cmd)
        chunk = ser.read(64)
        if chunk:
            rx.extend(chunk)
        for mcu in extract_frames(rx):
            n += 1
            now = time.monotonic()
            if now - last_print >= 0.2 or n <= 3:
                print(f"[{n:4d}] {mcu.summary()}")
                last_print = now
        dt = time.monotonic() - t0
        sleep = PERIOD_S - dt
        if sleep > 0:
            time.sleep(sleep)
    print(f"--- {label}: got {n} valid McuData frames ---")
    return n


def main() -> int:
    ap = argparse.ArgumentParser(description="xBot STM32 UART test (no ROS)")
    ap.add_argument("--port", default="/dev/ttyS2")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--listen", type=float, default=3.0, help="seconds radar OFF, only RX")
    ap.add_argument("--radar-on", type=float, default=5.0, help="seconds enable_power=1")
    ap.add_argument("--radar-off", type=float, default=2.0, help="seconds enable_power=0")
    args = ap.parse_args()

    try:
        import serial
    except ImportError as e:
        raise SystemExit(
            "需要 pyserial：sudo apt install -y python3-serial"
        ) from e

    print(f"open {args.port} {args.baud} 8N1")
    print("note: if console=ttyS2 is in cmdline, communication may be noisy — see orangepi_system_setup.md")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.01)
    except serial.SerialException as e:
        print(f"open failed: {e}", file=sys.stderr)
        return 1

    rx = bytearray()
    try:
        ser.reset_input_buffer()
        n1 = phase(ser, rx, 0, args.listen, "LISTEN (radar off)")
        n2 = phase(ser, rx, 1, args.radar_on, "RADAR ON")
        n3 = phase(ser, rx, 0, args.radar_off, "RADAR OFF")
    finally:
        # ensure power off on exit
        try:
            for _ in range(5):
                ser.write(pack_cmd(0, 0, 0))
                time.sleep(PERIOD_S)
        except Exception:
            pass
        ser.close()

    total = n1 + n2 + n3
    if total == 0:
        print(
            "\nFAIL: no McuData. Check: STM32 powered+firmware, TX/RX crossed, "
            "GND common, 115200, console not on ttyS2, sizeof frame=29.",
            file=sys.stderr,
        )
        return 2
    print(f"\nOK: total {total} frames. Radar ON got {n2}, OFF got {n3}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
