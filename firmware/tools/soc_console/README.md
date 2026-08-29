# xBot SOC 控制台

在 PC 上模拟香橙派 `base_control`：按协议每 20 ms 向 MCU 发送 `CmdData`，并显示 MCU 上报的 `McuData`。协议见仓库 `docs/soc_mcu_protocol.md`。

## 运行

在本目录执行（会使用 `.venv`，缺少依赖时自动安装）：

```powershell
cd firmware\tools\soc_console
.\run.ps1
```

浏览器打开 http://127.0.0.1:18080/ （默认会自动打开）。

```powershell
.\run.ps1 --demo          # 无板子，模拟 MCU
.\run.ps1 --no-browser
.\run.ps1 --port 9000
```

不要用系统里的 `python server.py`：那个解释器没有安装 fastapi。首次也可手动：

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python server.py
```

## 接线

USB 转串口 ↔ 控制板 USART1（交叉、共地）：

| USB-UART | 控制板 |
|----------|--------|
| TX       | RX1（STM32 PA10） |
| RX       | TX1（STM32 PA9） |
| GND      | GND |

波特率 115200 8N1。连接后工具会持续下发 12 字节 `CmdData`（MCU 约 100 ms 无命令会停 PWM）。急停或关掉页面会把 PWM 清零。
