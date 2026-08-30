#!/usr/bin/env bash
# Install user systemd unit so orangepi_audio_setup.sh runs after login/boot PulseAudio.
# Run ON the Orange Pi as user orangepi:
#   bash software/scripts/install_orangepi_audio_service.sh
#   bash software/scripts/install_orangepi_audio_service.sh SPK
set -euo pipefail

MODE="${1:-HP}"
case "$MODE" in
  HP|SPK) ;;
  *)
    echo "Usage: $0 [HP|SPK]" >&2
    exit 1
    ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SETUP="$SCRIPT_DIR/orangepi_audio_setup.sh"
if [[ ! -f "$SETUP" ]]; then
  echo "missing $SETUP" >&2
  exit 1
fi
chmod +x "$SETUP"

# Prefer a stable path under home (works after sync to ~/xbot_ws or hgfs)
INSTALL_DIR="${HOME}/.local/bin"
mkdir -p "$INSTALL_DIR"
cp -f "$SETUP" "$INSTALL_DIR/orangepi_audio_setup.sh"
chmod +x "$INSTALL_DIR/orangepi_audio_setup.sh"

UNIT_DIR="${HOME}/.config/systemd/user"
mkdir -p "$UNIT_DIR"
UNIT="$UNIT_DIR/xbot-audio.service"

cat >"$UNIT" <<EOF
[Unit]
Description=xBot RK809 audio route (not HDMI)
After=pulseaudio.service sound.target
Wants=pulseaudio.service

[Service]
Type=oneshot
RemainAfterExit=yes
Environment=XDG_RUNTIME_DIR=/run/user/%U
ExecStart=/bin/bash ${INSTALL_DIR}/orangepi_audio_setup.sh ${MODE}
# Retry if Pulse was not ready yet
ExecStartPost=/bin/sleep 2
ExecStartPost=-/bin/bash ${INSTALL_DIR}/orangepi_audio_setup.sh ${MODE}

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user enable xbot-audio.service
systemctl --user start xbot-audio.service || true

# Linger so user services can run without graphical login (SSH-only boards)
if command -v loginctl >/dev/null 2>&1; then
  if ! loginctl show-user "$USER" 2>/dev/null | grep -q 'Linger=yes'; then
    echo "Enabling lingering for $USER (needs sudo once)..."
    sudo loginctl enable-linger "$USER" || true
  fi
fi

echo "installed: $UNIT (MODE=$MODE)"
systemctl --user status xbot-audio.service --no-pager || true
pactl info | grep -E 'Default Sink|Default Source' || true
echo "Tip: mic smoke → bash $(dirname "$0")/orangepi_mic_test.sh --skip-setup"
