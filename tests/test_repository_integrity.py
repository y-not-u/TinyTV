import json
import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware"
SKETCH_DIR = FIRMWARE / "SmallDesktopDisplay"


class RepositoryIntegrityTests(unittest.TestCase):
    def test_arduino_sketch_layout_is_valid(self):
        sketch = SKETCH_DIR / "SmallDesktopDisplay.ino"

        self.assertTrue(sketch.is_file())
        self.assertEqual(sketch.stem, SKETCH_DIR.name)
        self.assertTrue((SKETCH_DIR / "number.cpp").is_file())
        self.assertTrue((SKETCH_DIR / "weathernum.cpp").is_file())

    def test_arduino_vscode_configuration_matches_target_board(self):
        config_path = SKETCH_DIR / ".vscode" / "arduino.json"
        config = json.loads(config_path.read_text(encoding="utf-8"))

        self.assertEqual(config["sketch"], "SmallDesktopDisplay.ino")
        self.assertEqual(config["board"], "esp8266:esp8266:nodemcuv2")
        self.assertIn("eesz=4M2M", config["configuration"])
        self.assertIn("baud=921600", config["configuration"])

    def test_required_bundled_libraries_are_present(self):
        expected = [
            "Adafruit_Unified_Sensor",
            "ArduinoJson",
            "DHT_sensor_library",
            "FastLED",
            "TFT_eSPI",
            "TJpg_Decoder",
            "Time-Library",
            "WiFiManager",
        ]

        for library in expected:
            with self.subTest(library=library):
                self.assertTrue((FIRMWARE / "libraries" / library).is_dir())

    def test_tft_user_setup_matches_documented_display(self):
        setup = (FIRMWARE / "libraries" / "TFT_eSPI" / "User_Setup.h").read_text(
            encoding="utf-8", errors="ignore"
        )

        expected_defines = {
            "ST7789_2_DRIVER": None,
            "TFT_WIDTH": "240",
            "TFT_HEIGHT": "240",
            "TFT_CS": "PIN_D8",
            "TFT_DC": "PIN_D3",
            "TFT_RST": "PIN_D4",
            "TFT_BL": "PIN_D1",
        }

        for name, value in expected_defines.items():
            with self.subTest(define=name):
                if value is None:
                    pattern = rf"^\s*#define\s+{re.escape(name)}\b"
                else:
                    pattern = rf"^\s*#define\s+{re.escape(name)}\s+{re.escape(value)}\b"
                self.assertRegex(setup, re.compile(pattern, re.MULTILINE))

    def test_firmware_includes_expected_local_assets(self):
        sketch = (SKETCH_DIR / "SmallDesktopDisplay.ino").read_text(
            encoding="utf-8", errors="ignore"
        )

        for include in [
            '"qr.h"',
            '"number.h"',
            '"weathernum.h"',
            '"font/ZdyLwFont_20.h"',
            '"img/misaka.h"',
            '"img/temperature.h"',
            '"img/humidity.h"',
        ]:
            with self.subTest(include=include):
                self.assertIn(f"#include {include}", sketch)

    def test_bootlog_tool_defaults_are_stable(self):
        tool = (ROOT / "tools" / "read-esp8266-bootlog.py").read_text(
            encoding="utf-8"
        )

        self.assertRegex(tool, re.compile(r'--port", default="/dev/cu\.usbserial-110"'))
        self.assertRegex(tool, re.compile(r'--baud", type=int, default=74880'))
        self.assertRegex(tool, re.compile(r'--seconds", type=float, default=8\.0'))


if __name__ == "__main__":
    unittest.main()
