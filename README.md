# TinyTV Firmware Source

ESP8266 TinyTV/weather-clock firmware source collected from the public LCSC OSHWHub project `https://oshwhub.com/q21182889/sd2`.

License: GPL 3.0.

## Source Layout

- `firmware/SmallDesktopDisplay/SmallDesktopDisplay.ino` - main Arduino firmware sketch.
- `firmware/SmallDesktopDisplay/font/` - generated font bitmap headers.
- `firmware/SmallDesktopDisplay/img/` - generated image/weather icon headers.
- `firmware/libraries/` - bundled Arduino libraries required by the sketch.
- `screen-test/ScreenTest/` - minimal display/pin validation sketch.
- `tools/read-esp8266-bootlog.py` - helper for capturing ESP8266 serial boot logs.

## Arduino Configuration

The sketch-specific VS Code Arduino settings are in `firmware/SmallDesktopDisplay/.vscode/arduino.json`.

- Board: `esp8266:esp8266:nodemcuv2`
- Flash layout: `eesz=4M2M`
- Upload baud: `921600`
- Sketch: `SmallDesktopDisplay.ino`

The TFT setup is in `firmware/libraries/TFT_eSPI/User_Setup.h`.

- Driver: `ST7789_2_DRIVER`
- Size: `240x240`
- Pins: `TFT_CS PIN_D8`, `TFT_DC PIN_D3`, `TFT_RST PIN_D4`, `TFT_BL PIN_D1`

## Common Commands

Compile the V1.4 firmware:

```bash
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 firmware/SmallDesktopDisplay
```

Upload to a CH340/CH341 serial adapter:

```bash
arduino-cli upload -p /dev/cu.usbserial-110 --fqbn esp8266:esp8266:nodemcuv2 firmware/SmallDesktopDisplay
```

Capture boot output:

```bash
python3 tools/read-esp8266-bootlog.py --port /dev/cu.usbserial-110 --seconds 8
```

Run repository integrity tests:

```bash
python3 -m unittest discover -s tests
```
