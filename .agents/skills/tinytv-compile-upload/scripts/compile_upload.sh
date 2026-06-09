#!/usr/bin/env bash
set -euo pipefail

PORT="/dev/cu.usbserial-110"
FQBN="esp8266:esp8266:nodemcuv2:eesz=4M2M,baud=921600"
SKETCH="firmware/SmallDesktopDisplay"
LIBRARIES="firmware/libraries"
BUILD_PATH="/private/tmp/tinytv-build"
COMPILE_ONLY=0

usage() {
  printf 'Usage: %s [--compile-only] [--port PORT] [--build-path PATH]\n' "$0"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --compile-only)
      COMPILE_ONLY=1
      shift
      ;;
    --port)
      PORT="${2:?missing value for --port}"
      shift 2
      ;;
    --build-path)
      BUILD_PATH="${2:?missing value for --build-path}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

if command -v arduino-cli >/dev/null 2>&1; then
  ARDUINO_CLI="$(command -v arduino-cli)"
elif [ -x "/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" ]; then
  ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
else
  printf 'arduino-cli not found on PATH or inside Arduino IDE.app\n' >&2
  exit 127
fi

"$ARDUINO_CLI" compile \
  --fqbn "$FQBN" \
  --libraries "$LIBRARIES" \
  --build-path "$BUILD_PATH" \
  "$SKETCH"

if [ "$COMPILE_ONLY" -eq 1 ]; then
  exit 0
fi

if [ ! -e "$PORT" ]; then
  printf 'serial port not found: %s\n' "$PORT" >&2
  exit 1
fi

"$ARDUINO_CLI" upload \
  -p "$PORT" \
  --fqbn "$FQBN" \
  --input-dir "$BUILD_PATH" \
  "$SKETCH"
