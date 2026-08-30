#!/usr/bin/env python3
"""GC9A01 1.28\" 240x240 SPI LCD smoke test (newbot pinout).

Requires: python3-periphery
  pip3 install --user python-periphery

Wiring (Orange Pi 3B):
  SPI  /dev/spidev3.0
  D/C  GPIO 128 (Pin13)
  RES  GPIO 130 (Pin15)

Usage on board:
  sudo python3 orangepi_lcd_test.py
  # or after chmod on spidev/gpio:
  python3 orangepi_lcd_test.py
"""
from __future__ import annotations

import argparse
import os
import sys
import time

DC = 128
RES = 130
X_MAX = 240
Y_MAX = 240
SPI_DEV = "/dev/spidev3.0"
SPI_HZ = 40_000_000  # keep <50MHz (audio noise / reliability)

# RGB565 colors (native); bytes sent high-byte-first after swap like newbot lcd.py
COLORS = {
    "red": 0xF800,
    "green": 0x07E0,
    "blue": 0x001F,
    "white": 0xFFFF,
    "black": 0x0000,
    "yellow": 0xFFE0,
    "cyan": 0x07FF,
    "magenta": 0xF81F,
}


def delay_ms(ms: float) -> None:
    time.sleep(ms / 1000.0)


def prepare_permissions() -> None:
    """Best-effort chmod/export; needs root or prior udev/rc.local."""
    os.system(f"chmod 666 {SPI_DEV} 2>/dev/null")
    if os.path.isdir("/sys/class/gpio"):
        os.system("chmod 777 /sys/class/gpio/export 2>/dev/null")
        for n in (DC, RES):
            path = f"/sys/class/gpio/gpio{n}"
            if not os.path.exists(path):
                os.system(f"echo {n} > /sys/class/gpio/export 2>/dev/null")
            os.system(f"chmod 666 {path}/value {path}/direction 2>/dev/null")


class SysfsGpio:
    def __init__(self, num: int):
        self.num = num
        self.base = f"/sys/class/gpio/gpio{num}"
        if not os.path.exists(self.base):
            with open("/sys/class/gpio/export", "w", encoding="ascii") as f:
                f.write(str(num))
            time.sleep(0.05)
        with open(f"{self.base}/direction", "w", encoding="ascii") as f:
            f.write("out")
        self._value = open(f"{self.base}/value", "w", encoding="ascii")

    def set(self, level: int) -> None:
        self._value.seek(0)
        self._value.write("1" if level else "0")
        self._value.flush()

    def close(self) -> None:
        try:
            self._value.close()
        except Exception:
            pass


class Lcd:
    def __init__(self, spi_hz: int = SPI_HZ):
        try:
            from periphery import SPI
        except ImportError as e:
            raise SystemExit(
                "需要 python-periphery：pip3 install --user python-periphery"
            ) from e

        if not os.path.exists(SPI_DEV):
            raise SystemExit(f"缺少 {SPI_DEV}，请先启用 overlay spi3-m0-cs0-spidev")

        self.dc = SysfsGpio(DC)
        self.res = SysfsGpio(RES)
        self.dc.set(1)
        self.res.set(1)
        self.spi = SPI(SPI_DEV, 0, spi_hz)
        print(f"spi {SPI_DEV} mode=0 freq={spi_hz/1e6:.0f} MHz")
        self.reset()
        self.init()
        print("lcd init ok")

    def close(self) -> None:
        try:
            self.fill(0x0000)
        except Exception:
            pass
        self.spi.close()
        self.dc.close()
        self.res.close()

    def write_byte(self, data: int) -> None:
        self.spi.transfer([data & 0xFF])

    def write_index(self, index: int) -> None:
        self.dc.set(0)
        self.write_byte(index)
        self.dc.set(1)

    def write_data(self, data: int) -> None:
        self.write_byte(data)

    def reset(self) -> None:
        self.res.set(0)
        delay_ms(50)
        self.res.set(1)
        delay_ms(50)

    def set_region(self, x0: int, y0: int, x1: int, y1: int) -> None:
        self.write_index(0x2A)
        self.write_data(x0 >> 8)
        self.write_data(x0 & 0xFF)
        self.write_data(x1 >> 8)
        self.write_data(x1 & 0xFF)
        self.write_index(0x2B)
        self.write_data(y0 >> 8)
        self.write_data(y0 & 0xFF)
        self.write_data(y1 >> 8)
        self.write_data(y1 & 0xFF)
        self.write_index(0x2C)

    def write_pixels(self, x: int, y: int, w: int, h: int, buf: bytes | bytearray) -> None:
        self.set_region(x, y, x + w - 1, y + h - 1)
        chunk = 4096
        total = w * h * 2
        for i in range(0, total, chunk):
            self.spi.transfer(buf[i : i + chunk])

    def rgb565_frame(self, color: int) -> bytes:
        # Match newbot: BGR565 then swap bytes → high byte first on wire
        hi = (color >> 8) & 0xFF
        lo = color & 0xFF
        pixel = bytes((hi, lo))
        return pixel * (X_MAX * Y_MAX)

    def fill(self, color: int) -> None:
        self.write_pixels(0, 0, X_MAX, Y_MAX, self.rgb565_frame(color))

    def init(self) -> None:
        # GC9A01 init sequence from newbot lcd.py
        seq = [
            (0xEF, ()),
            (0xEB, (0x14,)),
            (0xFE, ()),
            (0xEF, ()),
            (0xEB, (0x14,)),
            (0x84, (0x40,)),
            (0x85, (0xFF,)),
            (0x86, (0xFF,)),
            (0x87, (0xFF,)),
            (0x88, (0x0A,)),
            (0x89, (0x21,)),
            (0x8A, (0x00,)),
            (0x8B, (0x80,)),
            (0x8C, (0x01,)),
            (0x8D, (0x01,)),
            (0x8E, (0xFF,)),
            (0x8F, (0xFF,)),
            (0xB6, (0x00, 0x20)),
            (0x36, (0x68,)),  # USE_HORIZONTAL=2
            (0x3A, (0x05,)),
            (0x90, (0x08, 0x08, 0x08, 0x08)),
            (0xBD, (0x06,)),
            (0xBC, (0x00,)),
            (0xFF, (0x60, 0x01, 0x04)),
            (0xC3, (0x13,)),
            (0xC4, (0x13,)),
            (0xC9, (0x22,)),
            (0xBE, (0x11,)),
            (0xE1, (0x10, 0x0E)),
            (0xDF, (0x21, 0x0C, 0x02)),
            (0xF0, (0x45, 0x09, 0x08, 0x08, 0x26, 0x2A)),
            (0xF1, (0x43, 0x70, 0x72, 0x36, 0x37, 0x6F)),
            (0xF2, (0x45, 0x09, 0x08, 0x08, 0x26, 0x2A)),
            (0xF3, (0x43, 0x70, 0x72, 0x36, 0x37, 0x6F)),
            (0xED, (0x1B, 0x0B)),
            (0xAE, (0x77,)),
            (0xCD, (0x63,)),
            (0x70, (0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03)),
            (0xE8, (0x34,)),
            (0x62, (0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70)),
            (0x63, (0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70)),
            (0x64, (0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07)),
            (0x66, (0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00)),
            (0x67, (0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98)),
            (0x74, (0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00)),
            (0x98, (0x3E, 0x07)),
            (0x35, ()),
            (0x21, ()),
            (0x11, ()),
            (0x29, ()),
        ]
        for cmd, data in seq:
            self.write_index(cmd)
            for b in data:
                self.write_data(b)
        delay_ms(120)


def main() -> int:
    parser = argparse.ArgumentParser(description="xBot GC9A01 LCD color smoke test")
    parser.add_argument(
        "--hold",
        type=float,
        default=1.0,
        help="seconds to hold each color (default 1)",
    )
    parser.add_argument(
        "--colors",
        nargs="+",
        default=["red", "green", "blue", "white", "black"],
        help="color names: " + ",".join(COLORS),
    )
    parser.add_argument("--loop", type=int, default=1, help="repeat cycles")
    parser.add_argument("--no-prepare", action="store_true", help="skip chmod/export")
    args = parser.parse_args()

    if not args.no_prepare:
        prepare_permissions()

    lcd = Lcd()
    try:
        for cycle in range(args.loop):
            print(f"cycle {cycle + 1}/{args.loop}")
            for name in args.colors:
                if name not in COLORS:
                    print(f"unknown color: {name}", file=sys.stderr)
                    continue
                color = COLORS[name]
                t0 = time.time()
                lcd.fill(color)
                dt = (time.time() - t0) * 1000
                print(f"  fill {name} 0x{color:04X}  {dt:.0f} ms")
                delay_ms(args.hold * 1000)
        print("lcd test done")
        return 0
    finally:
        lcd.close()


if __name__ == "__main__":
    sys.exit(main())
