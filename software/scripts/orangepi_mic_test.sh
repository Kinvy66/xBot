#!/usr/bin/env bash
# USB mic smoke test on Orange Pi (no ROS2).
# Records a few seconds from the default Pulse source, plays back via RK809 3.5mm.
#
# Usage (on board, after scripts sync):
#   bash ~/xbot_ws/scripts/orangepi_mic_test.sh
#   bash ~/xbot_ws/scripts/orangepi_mic_test.sh --seconds 5 --mux SPK
#   bash ~/xbot_ws/scripts/orangepi_mic_test.sh --skip-setup
set -euo pipefail

SECONDS_REC=3
MUX=HP
SKIP_SETUP=0
OUT=/tmp/xbot_mic_test.wav

while [[ $# -gt 0 ]]; do
  case "$1" in
    --seconds|-d)
      SECONDS_REC="${2:?}"
      shift 2
      ;;
    --mux)
      MUX="${2:?}"
      shift 2
      ;;
    --out)
      OUT="${2:?}"
      shift 2
      ;;
    --skip-setup)
      SKIP_SETUP=1
      shift
      ;;
    -h|--help)
      sed -n '2,12p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "missing command: $1" >&2
    exit 1
  }
}

need_cmd pactl
need_cmd parecord
need_cmd paplay

echo "=== USB / ALSA presence ==="
lsusb | grep -iE '08bb|C-Media|PCM2902|Audio' || echo "(no C-Media/PCM2902 line — still check Pulse sources)"
arecord -l 2>/dev/null || true
echo

if [[ "$SKIP_SETUP" -eq 0 ]]; then
  if [[ -x "$SCRIPT_DIR/orangepi_audio_setup.sh" ]]; then
    bash "$SCRIPT_DIR/orangepi_audio_setup.sh" "$MUX"
  else
    echo "warn: $SCRIPT_DIR/orangepi_audio_setup.sh not found; using current Pulse defaults" >&2
  fi
fi

echo
echo "=== Pulse defaults ==="
pactl info | grep -E 'Default Sink|Default Source' || true
echo "sources:"
pactl list short sources || true
echo

SRC="$(pactl info | awk -F': ' '/Default Source/ {print $2; exit}')"
SINK="$(pactl info | awk -F': ' '/Default Sink/ {print $2; exit}')"

if [[ -z "$SRC" || "$SRC" == *monitor* ]]; then
  echo "FAIL: Default Source missing or is a .monitor — plug USB mic, re-run orangepi_audio_setup.sh" >&2
  exit 2
fi

if [[ "$SINK" != *rk809* && "$SINK" != *alsa_output* ]]; then
  echo "warn: Default Sink is '$SINK' (expected rk809_analog for 3.5mm)" >&2
fi

rm -f "$OUT"
echo "=== record ${SECONDS_REC}s from '$SRC' -> $OUT ==="
echo "(speak into the USB mic now)"
# timeout exits 124 when it kills parecord; recording file is still OK
set +e
timeout "${SECONDS_REC}" parecord --device="$SRC" --file-format=wav "$OUT"
rec_rc=$?
set -e
if [[ ! -s "$OUT" ]]; then
  echo "FAIL: empty recording $OUT (parecord rc=$rec_rc)" >&2
  exit 3
fi

BYTES=$(wc -c <"$OUT" | tr -d ' ')
echo "recorded bytes=$BYTES"

echo
echo "=== playback via '$SINK' ==="
paplay --device="$SINK" "$OUT"
echo "OK: if you heard your voice, USB mic + RK809 path works."
echo "file kept: $OUT"
