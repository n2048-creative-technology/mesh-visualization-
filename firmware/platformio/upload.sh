#!/usr/bin/env bash
set -euo pipefail

shopt -s nullglob
ports=(/dev/ttyACM* /dev/ttyUSB*)
PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"
export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-/tmp/pio-core}"
export PLATFORMIO_BUILD_DIR="${PLATFORMIO_BUILD_DIR:-/tmp/pio-build-mesh}"

if [ ${#ports[@]} -eq 0 ]; then
  echo "No serial ports found under /dev/ttyACM* or /dev/ttyUSB*" >&2
  exit 1
fi

"$PIO" run

for port in "${ports[@]}"; do
  echo "=== Uploading $port ==="
  "$PIO" run -t nobuild -t upload --upload-port "$port"
done
