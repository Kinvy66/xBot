"""Web SOC console: simulate 香橙派 base_control over USART1."""
from __future__ import annotations

import argparse
import asyncio
import sys
import threading
import webbrowser
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles

HERE = Path(__file__).resolve().parent
STATIC = HERE / "static"
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from pydantic import BaseModel

from bridge import SocBridge

class ConnectBody(BaseModel):
    port: str = ""
    baud: int = 115200
    demo: bool = False


bridge = SocBridge()
_loop: asyncio.AbstractEventLoop | None = None
_clients: list[asyncio.Queue] = []
_clients_lock = threading.Lock()


def _on_bridge(msg: dict) -> None:
    loop = _loop
    if loop is None:
        return
    loop.call_soon_threadsafe(_fanout, msg)


def _fanout(msg: dict) -> None:
    with _clients_lock:
        queues = list(_clients)
    for q in queues:
        if q.full():
            try:
                q.get_nowait()
            except asyncio.QueueEmpty:
                pass
        try:
            q.put_nowait(msg)
        except asyncio.QueueFull:
            pass


@asynccontextmanager
async def lifespan(_app: FastAPI):
    global _loop
    _loop = asyncio.get_running_loop()
    bridge.add_listener(_on_bridge)
    yield
    bridge.remove_listener(_on_bridge)
    bridge.stop()
    _loop = None


app = FastAPI(title="xBot SOC Console", lifespan=lifespan)
app.mount("/static", StaticFiles(directory=STATIC), name="static")


@app.get("/")
def index():
    return FileResponse(STATIC / "index.html")


@app.get("/api/ports")
def list_ports():
    try:
        from serial.tools import list_ports
    except ImportError:
        return JSONResponse({"ports": [], "error": "pyserial not installed"}, status_code=500)
    ports = [
        {
            "device": p.device,
            "name": p.name,
            "description": p.description or "",
            "hwid": p.hwid or "",
        }
        for p in list_ports.comports()
    ]
    return {"ports": ports}


@app.get("/api/state")
def state():
    return bridge.snapshot()


@app.post("/api/connect")
def connect(body: ConnectBody):
    try:
        if body.demo:
            bridge.start_demo()
        else:
            port = body.port.strip()
            if not port:
                return JSONResponse({"ok": False, "error": "missing port"}, status_code=400)
            bridge.start_serial(port, body.baud)
    except Exception as exc:
        return JSONResponse({"ok": False, "error": str(exc)}, status_code=400)
    snap = {"type": "status", **bridge.snapshot()}
    _on_bridge(snap)
    return {"ok": True, **snap}


@app.post("/api/disconnect")
def disconnect():
    bridge.stop_motors()
    bridge.stop()
    snap = {"type": "status", **bridge.snapshot()}
    _on_bridge(snap)
    return {"ok": True, **snap}


@app.websocket("/ws")
async def ws_endpoint(ws: WebSocket):
    await ws.accept()
    queue: asyncio.Queue = asyncio.Queue(maxsize=8)
    with _clients_lock:
        _clients.append(queue)
    await ws.send_json({"type": "status", **bridge.snapshot()})

    async def pump():
        while True:
            payload = await queue.get()
            await ws.send_json(payload)

    pump_task = asyncio.create_task(pump())
    try:
        while True:
            msg = await ws.receive_json()
            _handle_client_msg(msg)
    except WebSocketDisconnect:
        pass
    except Exception:
        pass
    finally:
        pump_task.cancel()
        with _clients_lock:
            if queue in _clients:
                _clients.remove(queue)
            empty = len(_clients) == 0
        if empty:
            bridge.stop_motors()


def _handle_client_msg(msg: dict) -> None:
    kind = msg.get("type")
    if kind in ("set", "cmd"):
        bridge.set_cmd(
            pwm1=msg.get("pwm1"),
            pwm2=msg.get("pwm2"),
            enable_power=msg.get("enable_power"),
        )
    elif kind in ("stop", "estop"):
        bridge.stop_motors()
        if msg.get("enable_power") is not None:
            bridge.set_cmd(enable_power=msg.get("enable_power"))


def main() -> None:
    parser = argparse.ArgumentParser(description="xBot SOC web console (simulate 香橙派)")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=18080)
    parser.add_argument("--demo", action="store_true", help="start with simulated MCU (no serial)")
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()

    if args.demo:
        bridge.start_demo()

    url = f"http://{args.host}:{args.port}/"
    if not args.no_browser:
        threading.Timer(0.6, lambda: webbrowser.open(url)).start()

    import uvicorn

    print(f"xBot SOC console: {url}", file=sys.stderr)
    uvicorn.run(app, host=args.host, port=args.port, log_level="info")


if __name__ == "__main__":
    main()
