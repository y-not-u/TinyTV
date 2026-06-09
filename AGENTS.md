# Repository Guidelines

## Project Structure & Module Organization

This repository contains ESP8266 TinyTV/weather-clock firmware source. The main Arduino sketch lives in `firmware/SmallDesktopDisplay/`. Bundled Arduino libraries are under `firmware/libraries/`, with TFT configuration in `firmware/libraries/TFT_eSPI/User_Setup.h`. Generated font and bitmap headers are in `firmware/SmallDesktopDisplay/font/` and `firmware/SmallDesktopDisplay/img/`. `screen-test/ScreenTest/` holds a small display test sketch, and `tools/` contains serial/debug utilities.

## Build, Test, and Development Commands

- `arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 firmware/SmallDesktopDisplay` builds the current sketch.
- `arduino-cli upload -p /dev/cu.usbserial-110 --fqbn esp8266:esp8266:nodemcuv2 firmware/SmallDesktopDisplay` flashes via the CH340 serial adapter.
- `python3 tools/read-esp8266-bootlog.py --port /dev/cu.usbserial-110 --seconds 8` captures ESP8266 boot output at 74880 baud.
- `python3 -m unittest discover -s tests` runs repository integrity tests without hardware.

Use the Arduino settings already present in `.vscode/arduino.json`: board `nodemcuv2`, flash layout `eesz=4M2M`, and upload baud `921600`.

## Coding Style & Naming Conventions

Keep Arduino/C++ files consistent with the existing style: 2-space indentation in new blocks where practical, braces on the same line for functions/control flow when editing nearby code, and descriptive globals matching current names such as `LCD_BL_PWM`, `cityCode`, and `updateweater_time`. Keep firmware edits inside `firmware/SmallDesktopDisplay/`. Do not hand-edit generated image/font headers unless regenerating the source asset.

## Testing Guidelines

Automated tests live in `tests/` and use Python `unittest` to verify repository structure, Arduino settings, required bundled libraries, TFT setup, and serial-tool defaults. Run `python3 -m unittest discover -s tests` before handoff. For firmware behavior, also compile the target sketch, flash a device when hardware is available, and check serial boot logs. For display or pin changes, run or adapt `screen-test/ScreenTest/ScreenTest.ino` before touching the main firmware.

## Commit & Pull Request Guidelines

No Git history is present in this checkout, so use concise imperative commits such as `fix: correct TFT backlight pin` or `docs: update flashing notes`. Pull requests should describe the target firmware version, hardware tested, compile/upload commands run, and any screenshots or serial logs for UI, Wi-Fi, or boot behavior changes.

## Security & Configuration Tips

Do not commit Wi-Fi credentials, API keys, device-specific flash backups, or generated firmware binaries unless they are intentionally part of a release. Keep local serial ports and private network settings out of shared configuration.
