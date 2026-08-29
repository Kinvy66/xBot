const $ = (id) => document.getElementById(id);

const state = {
  ws: null,
  connected: false,
  maxPwm: 2000,
  pwm1: 0,
  pwm2: 0,
  enablePower: 0,
  keys: new Set(),
  histEnc1: [],
  histEnc2: [],
  histVbat: [],
  histLen: 150,
};

function clamp(v, lo, hi) {
  return Math.max(lo, Math.min(hi, v));
}

function usingKeys() {
  return state.keys.size > 0;
}

function mixFromKeys() {
  const m = state.maxPwm;
  let v = 0;
  let w = 0;
  const k = state.keys;
  if (k.has("w") || k.has("arrowup")) v += 1;
  if (k.has("s") || k.has("arrowdown")) v -= 1;
  if (k.has("a") || k.has("arrowleft")) w -= 1;
  if (k.has("d") || k.has("arrowright")) w += 1;
  if (k.has("q")) {
    return { pwm1: -m, pwm2: m };
  }
  if (k.has("e")) {
    return { pwm1: m, pwm2: -m };
  }
  const turn = w * m * 0.7;
  return {
    pwm1: clamp(Math.round(v * m - turn), -m, m),
    pwm2: clamp(Math.round(v * m + turn), -m, m),
  };
}

function desiredCmd() {
  if (usingKeys()) {
    return { ...mixFromKeys(), enable_power: state.enablePower };
  }
  return { pwm1: state.pwm1, pwm2: state.pwm2, enable_power: state.enablePower };
}

function sendCmd() {
  const cmd = desiredCmd();
  if (state.ws && state.ws.readyState === WebSocket.OPEN) {
    state.ws.send(JSON.stringify({ type: "set", ...cmd }));
  }
  $("pwm1Val").textContent = cmd.pwm1;
  $("pwm2Val").textContent = cmd.pwm2;
}

function setSlidersFromState() {
  $("pwm1").value = String(state.pwm1);
  $("pwm2").value = String(state.pwm2);
  $("pwm1Val").textContent = String(state.pwm1);
  $("pwm2Val").textContent = String(state.pwm2);
}

function applyMaxPwm(max) {
  state.maxPwm = max;
  $("maxPwmVal").textContent = String(max);
  for (const id of ["pwm1", "pwm2"]) {
    const el = $(id);
    el.min = String(-max);
    el.max = String(max);
  }
  state.pwm1 = clamp(state.pwm1, -max, max);
  state.pwm2 = clamp(state.pwm2, -max, max);
  setSlidersFromState();
}

async function refreshPorts() {
  const res = await fetch("/api/ports");
  const data = await res.json();
  const sel = $("port");
  const prev = sel.value;
  sel.innerHTML = "";
  for (const p of data.ports || []) {
    const opt = document.createElement("option");
    opt.value = p.device;
    opt.textContent = p.description ? `${p.device}  ${p.description}` : p.device;
    sel.appendChild(opt);
  }
  if (![...sel.options].some((o) => o.value === prev) && sel.options.length) {
    sel.selectedIndex = 0;
  } else if (prev) {
    sel.value = prev;
  }
}

function connectWs() {
  if (state.ws) {
    state.ws.close();
  }
  const proto = location.protocol === "https:" ? "wss" : "ws";
  const ws = new WebSocket(`${proto}://${location.host}/ws`);
  state.ws = ws;
  ws.onmessage = (ev) => {
    const msg = JSON.parse(ev.data);
    onTelemetry(msg);
  };
  ws.onclose = () => {
    if (state.ws === ws) {
      setTimeout(connectWs, 800);
    }
  };
}

async function apiConnect(demo) {
  const body = demo
    ? { demo: true }
    : { port: $("port").value, baud: Number($("baud").value) || 115200 };
  const res = await fetch("/api/connect", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  const data = await res.json();
  if (!res.ok) {
    $("errLine").textContent = data.error || "连接失败";
    applyStatus(data);
    return;
  }
  applyStatus(data);
  sendCmd();
}

async function apiDisconnect() {
  const res = await fetch("/api/disconnect", { method: "POST" });
  const data = await res.json().catch(() => ({}));
  applyStatus({ connected: false, demo: false, port: "", rx_hz: 0, tx_count: 0, rx_count: 0, error: data.error || "", ...data });
}

function applyStatus(msg) {
  const connected = !!msg.connected;
  state.connected = connected;
  $("linkPill").textContent = msg.demo
    ? "演示模式"
    : connected
      ? `${msg.port || ""}  ${msg.rx_hz || 0} Hz`
      : "未连接";
  $("linkPill").classList.toggle("ok", connected);
  document.querySelector(".mark").classList.toggle("on", connected);

  if (msg.rx_hz != null) $("rxHz").textContent = String(msg.rx_hz);
  if (msg.tx_count != null) $("txCount").textContent = String(msg.tx_count);
  if (msg.rx_count != null) $("rxCount").textContent = String(msg.rx_count);
  $("errLine").textContent = msg.error || "";

  if (msg.cmd) {
    $("txHex").textContent = "TX  " + (msg.cmd.hex || "");
  }
}

const ACCEL_LSB = 16384; /* ±2 g */
const GYRO_LSB = 16.4; /* ±2000 °/s */
const att = { roll: 0, pitch: 0, yaw: 0, t: 0 };

function fmt1(v) {
  return (Math.round(v * 10) / 10).toFixed(1);
}

function updateImu(mcu) {
  $("imuOk").textContent = mcu.imu_ok ? "ok" : "fail";
  $("ax").textContent = String(mcu.ax);
  $("ay").textContent = String(mcu.ay);
  $("az").textContent = String(mcu.az);
  $("gx").textContent = String(mcu.gx);
  $("gy").textContent = String(mcu.gy);
  $("gz").textContent = String(mcu.gz);
  $("axg").textContent = fmt1(mcu.ax / ACCEL_LSB);
  $("ayg").textContent = fmt1(mcu.ay / ACCEL_LSB);
  $("azg").textContent = fmt1(mcu.az / ACCEL_LSB);
  $("gxd").textContent = fmt1(mcu.gx / GYRO_LSB);
  $("gyd").textContent = fmt1(mcu.gy / GYRO_LSB);
  $("gzd").textContent = fmt1(mcu.gz / GYRO_LSB);

  if (!mcu.imu_ok) {
    return;
  }

  const now = performance.now() / 1000;
  const dt = att.t ? Math.min(0.08, Math.max(0.005, now - att.t)) : 0.02;
  att.t = now;

  const accRoll = Math.atan2(mcu.ay, mcu.az);
  const accPitch = Math.atan2(-mcu.ax, Math.hypot(mcu.ay, mcu.az));
  const gx = ((mcu.gx / GYRO_LSB) * Math.PI) / 180;
  const gy = ((mcu.gy / GYRO_LSB) * Math.PI) / 180;
  const gz = ((mcu.gz / GYRO_LSB) * Math.PI) / 180;
  const a = 0.96;
  att.roll = a * (att.roll + gx * dt) + (1 - a) * accRoll;
  att.pitch = a * (att.pitch + gy * dt) + (1 - a) * accPitch;
  att.yaw += gz * dt;

  const rollDeg = (att.roll * 180) / Math.PI;
  const pitchDeg = (att.pitch * 180) / Math.PI;
  const yawDeg = (att.yaw * 180) / Math.PI;
  $("roll").textContent = `${fmt1(rollDeg)}°`;
  $("pitch").textContent = `${fmt1(pitchDeg)}°`;
  $("yaw").textContent = `${fmt1(yawDeg)}°`;

  const rig = $("bot3d");
  if (rig) {
    rig.style.transform =
      `rotateY(${yawDeg}deg) rotateX(${pitchDeg}deg) rotateZ(${-rollDeg}deg)`;
  }
}

function onTelemetry(msg) {
  applyStatus(msg);
  const mcu = msg.mcu;
  if (!mcu) {
    return;
  }
  $("vbat").textContent = String(mcu.vbat_mv);
  $("enc1").textContent = String(mcu.encoder1);
  $("enc2").textContent = String(mcu.encoder2);
  $("flags").textContent =
    `${mcu.charger_connected ? "充电" : "未充"} / ${mcu.fully_charged ? "满" : "未满"} / ${mcu.asr_id}`;
  $("rxHex").textContent = "RX  " + (mcu.hex || "");
  updateImu(mcu);

  pushHist(state.histEnc1, mcu.encoder1);
  pushHist(state.histEnc2, mcu.encoder2);
  pushHist(state.histVbat, mcu.vbat_mv);
  drawChart();
}

function pushHist(arr, v) {
  arr.push(v);
  if (arr.length > state.histLen) {
    arr.shift();
  }
}

function drawChart() {
  const canvas = $("chart");
  const ctx = canvas.getContext("2d");
  const w = canvas.width;
  const h = canvas.height;
  ctx.clearRect(0, 0, w, h);
  ctx.strokeStyle = "#d5dce6";
  ctx.beginPath();
  ctx.moveTo(0, h / 2);
  ctx.lineTo(w, h / 2);
  ctx.stroke();
  strokeSeries(ctx, state.histEnc1, "#0d8f63", 80);
  strokeSeries(ctx, state.histEnc2, "#2563eb", 80);
  strokeSeries(ctx, state.histVbat, "#b57912", 16000, false);
}

function strokeSeries(ctx, data, color, scale, bipolar = true) {
  if (data.length < 2) {
    return;
  }
  const w = ctx.canvas.width;
  const h = ctx.canvas.height;
  ctx.beginPath();
  ctx.strokeStyle = color;
  ctx.lineWidth = 1.5;
  data.forEach((v, i) => {
    const x = (i / (state.histLen - 1)) * w;
    let y;
    if (bipolar) {
      y = h / 2 - (v / scale) * (h / 2 - 8);
    } else {
      y = h - 8 - (v / scale) * (h - 16);
    }
    y = clamp(y, 1, h - 1);
    if (i === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  });
  ctx.stroke();
}

function bindPad() {
  for (const btn of $("pad").querySelectorAll("button")) {
    const key = btn.dataset.key;
    const down = (ev) => {
      ev.preventDefault();
      state.keys.add(key);
      btn.classList.add("active");
      sendCmd();
    };
    const up = () => {
      state.keys.delete(key);
      btn.classList.remove("active");
      sendCmd();
    };
    btn.addEventListener("pointerdown", down);
    btn.addEventListener("pointerup", up);
    btn.addEventListener("pointerleave", up);
  }
}

function isTypingTarget(el) {
  return el && (el.tagName === "INPUT" || el.tagName === "SELECT" || el.tagName === "TEXTAREA");
}

window.addEventListener("keydown", (ev) => {
  if (isTypingTarget(ev.target)) {
    return;
  }
  const key = ev.key.toLowerCase();
  if (key === " ") {
    ev.preventDefault();
    state.pwm1 = 0;
    state.pwm2 = 0;
    state.keys.clear();
    setSlidersFromState();
    if (state.ws && state.ws.readyState === WebSocket.OPEN) {
      state.ws.send(JSON.stringify({ type: "estop" }));
    }
    return;
  }
  if (["w", "a", "s", "d", "q", "e", "arrowup", "arrowdown", "arrowleft", "arrowright"].includes(key)) {
    ev.preventDefault();
    state.keys.add(key);
    sendCmd();
  }
});

window.addEventListener("keyup", (ev) => {
  const key = ev.key.toLowerCase();
  state.keys.delete(key);
  sendCmd();
});

$("refreshPorts").addEventListener("click", () => refreshPorts());
$("btnConnect").addEventListener("click", () => apiConnect(false));
$("btnDemo").addEventListener("click", () => apiConnect(true));
$("btnDisconnect").addEventListener("click", () => apiDisconnect());
$("btnStop").addEventListener("click", () => {
  state.pwm1 = 0;
  state.pwm2 = 0;
  state.keys.clear();
  setSlidersFromState();
  if (state.ws && state.ws.readyState === WebSocket.OPEN) {
    state.ws.send(JSON.stringify({ type: "estop" }));
  }
});
$("lidar").addEventListener("change", () => {
  state.enablePower = $("lidar").checked ? 1 : 0;
  sendCmd();
});
$("maxPwm").addEventListener("input", () => {
  applyMaxPwm(Number($("maxPwm").value));
  sendCmd();
});
$("pwm1").addEventListener("input", () => {
  state.pwm1 = Number($("pwm1").value);
  sendCmd();
});
$("pwm2").addEventListener("input", () => {
  state.pwm2 = Number($("pwm2").value);
  sendCmd();
});

applyMaxPwm(2000);
refreshPorts();
connectWs();
bindPad();
$("btnResetAtt").addEventListener("click", () => {
  att.yaw = 0;
  $("yaw").textContent = "0.0°";
  const rig = $("bot3d");
  if (rig) {
    const rollDeg = (att.roll * 180) / Math.PI;
    const pitchDeg = (att.pitch * 180) / Math.PI;
    rig.style.transform =
      `rotateY(0deg) rotateX(${pitchDeg}deg) rotateZ(${-rollDeg}deg)`;
  }
});
