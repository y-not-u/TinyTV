---
name: tinytv-compile-upload
description: Use for this TinyTV ESP8266 firmware project when the user asks to compile, build, upload, flash, burn, or deploy firmware to the connected device. Handles Arduino CLI discovery, bundled libraries, NodeMCU board options, and the CH340 serial port workflow.
metadata:
  short-description: Compile and flash TinyTV firmware
---

# TinyTV Compile Upload

Use this skill for compile-only checks and for flashing `firmware/SmallDesktopDisplay` to the ESP8266 TinyTV device.

## Defaults

- Sketch: `firmware/SmallDesktopDisplay`
- Bundled libraries: `firmware/libraries`
- Board: `esp8266:esp8266:nodemcuv2`
- Board options: `eesz=4M2M,baud=921600`
- Serial port: `/dev/cu.usbserial-110`
- Build path: `/private/tmp/tinytv-build`

If `arduino-cli` is not on `PATH`, use the Arduino IDE bundled binary:

```bash
/Applications/Arduino\ IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli
```

## Workflow

1. Confirm the serial port exists before flashing:

```bash
ls /dev/cu.usbserial-110
```

2. Compile with bundled libraries:

```bash
.agents/skills/tinytv-compile-upload/scripts/compile_upload.sh --compile-only
```

3. Flash the device:

```bash
.agents/skills/tinytv-compile-upload/scripts/compile_upload.sh
```

## Notes

- Running Arduino compile or upload may need sandbox escalation because Arduino writes to `~/Library/Caches/arduino` and upload needs serial access.
- If upload fails because the port is busy, stop Serial Monitor or any process using `/dev/cu.usbserial-110`, then retry.
- If compile cannot find `ArduinoJson.h` or other dependencies, make sure `--libraries firmware/libraries` is present.
- Treat `Hash of data verified` from `esptool.py` as the upload success signal.
