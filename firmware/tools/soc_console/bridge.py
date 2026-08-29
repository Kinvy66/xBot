"""20 ms serial (or demo) loop: TX CmdData, RX McuData."""
from __future__ import annotations

import math
import threading
import time
from typing import Callable

from protocol import PERIOD_MS, CmdData, McuData, extract_mcu_frames, pack_cmd, pack_mcu

Listener = Callable[[dict], None]


class SocBridge:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._cmd = CmdData()
        self._ser = None
        self._demo = False
        self._port = ""
        self._baud = 115200
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self._listeners: list[Listener] = []
        self._rx_buf = bytearray()
        self._tx_count = 0
        self._rx_count = 0
        self._rx_times: list[float] = []
        self._last_mcu: McuData | None = None
        self._error = ""
        self._demo_enc1 = 0.0
        self._demo_enc2 = 0.0

    def add_listener(self, fn: Listener) -> None:
        with self._lock:
            self._listeners.append(fn)

    def remove_listener(self, fn: Listener) -> None:
        with self._lock:
            if fn in self._listeners:
                self._listeners.remove(fn)

    def set_cmd(self, pwm1: int | None = None, pwm2: int | None = None,
                enable_power: int | None = None) -> CmdData:
        with self._lock:
            if pwm1 is not None:
                self._cmd.pwm1 = int(pwm1)
            if pwm2 is not None:
                self._cmd.pwm2 = int(pwm2)
            if enable_power is not None:
                self._cmd.enable_power = 1 if enable_power else 0
            return CmdData(self._cmd.pwm1, self._cmd.pwm2, self._cmd.enable_power)

    def stop_motors(self) -> None:
        self.set_cmd(pwm1=0, pwm2=0)

    def snapshot(self) -> dict:
        with self._lock:
            now = time.monotonic()
            recent = [t for t in self._rx_times if now - t <= 1.0]
            self._rx_times = recent
            mcu = self._last_mcu.to_dict() if self._last_mcu else None
            return {
                "connected": self._ser is not None or self._demo,
                "demo": self._demo,
                "port": self._port,
                "baud": self._baud,
                "error": self._error,
                "tx_count": self._tx_count,
                "rx_count": self._rx_count,
                "rx_hz": len(recent),
                "cmd": self._cmd.to_dict(),
                "mcu": mcu,
            }

    def start_serial(self, port: str, baud: int = 115200) -> None:
        import serial

        self.stop(publish=False)
        ser = serial.Serial(
            port=port,
            baudrate=baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0,
            write_timeout=0.05,
        )
        with self._lock:
            self._ser = ser
            self._demo = False
            self._port = port
            self._baud = baud
            self._error = ""
            self._rx_buf.clear()
            self._tx_count = 0
            self._rx_count = 0
            self._rx_times.clear()
        self._start_thread()

    def start_demo(self) -> None:
        self.stop(publish=False)
        with self._lock:
            self._demo = True
            self._port = "demo"
            self._error = ""
            self._demo_enc1 = 0.0
            self._demo_enc2 = 0.0
        self._start_thread()

    def stop(self, publish: bool = True) -> None:
        self._stop.set()
        thread = self._thread
        if thread is not None and thread is not threading.current_thread():
            thread.join(timeout=1.0)
        self._thread = None
        self._stop.clear()
        with self._lock:
            ser = self._ser
            self._ser = None
            self._demo = False
            self._port = ""
            self._rx_times.clear()
        if ser is not None:
            try:
                ser.close()
            except Exception:
                pass
        if publish:
            self._publish({"type": "status", **self.snapshot()})

    def _drop_link(self, message: str) -> None:
        with self._lock:
            self._error = message
            ser = self._ser
            self._ser = None
            self._demo = False
            self._port = ""
            self._rx_times.clear()
        if ser is not None:
            try:
                ser.close()
            except Exception:
                pass
        self._publish({"type": "status", **self.snapshot()})
        self._stop.set()

    def _start_thread(self) -> None:
        self._stop.clear()
        self._thread = threading.Thread(target=self._loop, name="soc-bridge", daemon=True)
        self._thread.start()

    def _loop(self) -> None:
        period = PERIOD_MS / 1000.0
        next_t = time.monotonic()
        while not self._stop.is_set():
            try:
                self._tick()
            except Exception as exc:
                self._drop_link(str(exc))
                break
            next_t += period
            delay = next_t - time.monotonic()
            if delay > 0:
                time.sleep(delay)
            else:
                next_t = time.monotonic()

    def _tick(self) -> None:
        with self._lock:
            cmd = CmdData(self._cmd.pwm1, self._cmd.pwm2, self._cmd.enable_power)
            ser = self._ser
            demo = self._demo

        raw = pack_cmd(cmd.pwm1, cmd.pwm2, cmd.enable_power)
        if ser is not None:
            ser.write(raw)
            incoming = ser.read(256)
            if incoming:
                with self._lock:
                    self._rx_buf.extend(incoming)
        elif demo:
            self._demo_rx(cmd)

        with self._lock:
            self._tx_count += 1
            frames = list(extract_mcu_frames(self._rx_buf))
            for frame in frames:
                self._last_mcu = frame
                self._rx_count += 1
                self._rx_times.append(time.monotonic())

        if frames:
            snap = self.snapshot()
            snap["type"] = "telemetry"
            self._publish(snap)
        elif self._tx_count % 10 == 0:
            self._publish({"type": "status", **self.snapshot()})

    def _demo_rx(self, cmd: CmdData) -> None:
        self._demo_enc1 = cmd.pwm1 * 0.02
        self._demo_enc2 = cmd.pwm2 * 0.02
        enc1 = int(self._demo_enc1)
        enc2 = int(self._demo_enc2)
        vbat = 12400 + (cmd.enable_power * 80)
        t = time.monotonic()
        roll = math.sin(t * 0.7) * 0.45
        pitch = math.cos(t * 0.5) * 0.35
        ax = int(-math.sin(pitch) * 16384)
        ay = int(math.sin(roll) * math.cos(pitch) * 16384)
        az = int(math.cos(roll) * math.cos(pitch) * 16384)
        frame = pack_mcu(
            enc1,
            enc2,
            vbat,
            imu_ok=1,
            ax=ax,
            ay=ay,
            az=az,
            gx=int(math.cos(t * 0.7) * 40),
            gy=int(-math.sin(t * 0.5) * 30),
            gz=int(math.sin(t * 0.2) * 20),
        )
        with self._lock:
            self._rx_buf.extend(frame)

    def _publish(self, msg: dict) -> None:
        with self._lock:
            listeners = list(self._listeners)
        for fn in listeners:
            try:
                fn(msg)
            except Exception:
                pass
