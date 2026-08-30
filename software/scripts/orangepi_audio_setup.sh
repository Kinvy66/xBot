#!/usr/bin/env bash
# Orange Pi 3B audio: route playback to RK809 3.5mm (not HDMI), prefer USB mic.
# Usage:
#   ./orangepi_audio_setup.sh           # headphones (HP)
#   ./orangepi_audio_setup.sh HP
#   ./orangepi_audio_setup.sh SPK       # control-board amp / speaker
set -euo pipefail

MODE="${1:-HP}"
case "$MODE" in
  HP|SPK) ;;
  *)
    echo "Usage: $0 [HP|SPK]" >&2
    exit 1
    ;;
esac

# Wait for PulseAudio (user session or system)
for _ in $(seq 1 30); do
  if pactl info >/dev/null 2>&1; then
    break
  fi
  sleep 1
done
if ! pactl info >/dev/null 2>&1; then
  echo "PulseAudio not available" >&2
  exit 1
fi

# Card 2 = Analog RK809 (3.5mm). Card 0 = HDMI on this image.
CARD=2
if ! amixer -c "$CARD" scontrols >/dev/null 2>&1; then
  echo "ALSA card $CARD (RK809) not found" >&2
  aplay -l >&2 || true
  exit 1
fi

amixer -c "$CARD" set Master 100% unmute >/dev/null
amixer -c "$CARD" set 'Playback Mux' "$MODE" >/dev/null

SINK_NAME="rk809_analog"
if ! pactl list short sinks | awk '{print $2}' | grep -qx "$SINK_NAME"; then
  pactl load-module module-alsa-sink \
    device=hw:${CARD},0 \
    sink_name="$SINK_NAME" \
    sink_properties=device.description="RK809_3.5mm" >/dev/null
fi

pactl set-default-sink "$SINK_NAME"
pactl set-sink-mute "$SINK_NAME" 0
pactl set-sink-volume "$SINK_NAME" 100%

# Prefer USB mic as default Pulse source (skip .monitor sinks).
# Order: C-Media / PCM2902 / 08bb, then any alsa_input.usb-*, else any *usb* source.
pick_usb_source() {
  local list
  list="$(pactl list short sources | awk '!/\.monitor$/ {print $2}')"
  [[ -z "$list" ]] && return 1
  local prefer
  prefer="$(printf '%s\n' "$list" | grep -iE 'C-Media|PCM2902|08bb|usb-C-Media' | head -1 || true)"
  if [[ -n "$prefer" ]]; then
    printf '%s\n' "$prefer"
    return 0
  fi
  prefer="$(printf '%s\n' "$list" | grep -iE '^alsa_input\.usb-' | head -1 || true)"
  if [[ -n "$prefer" ]]; then
    printf '%s\n' "$prefer"
    return 0
  fi
  prefer="$(printf '%s\n' "$list" | grep -iE 'usb' | head -1 || true)"
  if [[ -n "$prefer" ]]; then
    printf '%s\n' "$prefer"
    return 0
  fi
  return 1
}

SRC=""
if SRC="$(pick_usb_source)"; then
  pactl set-default-source "$SRC" || true
  pactl set-source-mute "$SRC" 0 || true
  pactl set-source-volume "$SRC" 80% || true
  echo "mic ok: source=$SRC"
else
  echo "mic warn: no USB Pulse source found (check lsusb / pactl list short sources)" >&2
fi

echo "audio ok: sink=$SINK_NAME mux=$MODE card=$CARD"
pactl info | grep -E 'Default Sink|Default Source' || true
