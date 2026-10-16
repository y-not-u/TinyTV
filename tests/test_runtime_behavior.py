"""Execute the same portable scheduling, UTF-8 and HTTP code as the ESP8266."""
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class RuntimeBehaviorTests(unittest.TestCase):
    def test_firmware_runtime_algorithms(self):
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler, "A C++ compiler is required for runtime tests")
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "runtime-tests"
            subprocess.run([
                compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-g",
                "-I", str(ROOT / "firmware/SmallDesktopDisplay"),
                str(ROOT / "tests/runtime_behavior.cpp"), "-o", str(output),
            ], check=True, text=True, timeout=60)
            subprocess.run([str(output)], check=True, text=True, timeout=30)

    def test_existing_touch_gestures(self):
        sketch = ROOT / "firmware/SmallDesktopDisplay"
        source = (sketch / "PagesTouch.ino").read_text()
        poll = source[source.index("void Touch_poll()"):source.index("void Touch_onSingleClick()")]
        # ESP8266 unsigned long is 32-bit; the macOS host uses 64-bit.
        poll = poll.replace("unsigned long", "uint32_t")
        defines = (sketch / "SmallDesktopDisplay.ino").read_text()
        names = ["TOUCH_AUTO_PIN", "TOUCH_PIN", "TOUCH_DEBOUNCE_MS",
                 "TOUCH_DOUBLE_CLICK_MS", "TOUCH_LONG_PRESS_MS"]
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            include = pathlib.Path(directory) / "TouchPoll.inc"
            include.write_text("\n".join(
                re.search(r"^#define " + name + r"\s+\d+", defines, re.MULTILINE)[0]
                for name in names) + "\n" + poll)
            output = pathlib.Path(directory) / "touch-tests"
            subprocess.run([compiler, "-std=c++11", "-fsanitize=address,undefined",
                            "-I", directory, str(ROOT / "tests/touch_behavior.cpp"),
                            "-o", str(output)], check=True, timeout=60)
            for scenario in ["single", "double", "long", "bounce", "wrap"]:
                with self.subTest(scenario=scenario):
                    subprocess.run([str(output), scenario], check=True, timeout=30)

    def test_native_text_drawing_and_region_bounds(self):
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "text-tests"
            subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra",
                            "-fsanitize=address,undefined", "-I", str(ROOT / "tests/stubs"),
                            "-I", str(ROOT / "firmware/SmallDesktopDisplay"),
                            str(ROOT / "tests/text_rendering.cpp"), "-o", str(output)],
                           check=True, capture_output=True, text=True, timeout=60)
            subprocess.run([str(output)], check=True, timeout=30)

    def test_clock_uses_one_time_snapshot_across_midnight(self):
        compiler = shutil.which("c++")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "clock-tests"
            subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=address,undefined", "-I",
                            str(ROOT / "firmware/SmallDesktopDisplay"),
                            str(ROOT / "tests/clock_rendering.cpp"), "-o", str(output)],
                           check=True, timeout=60)
            subprocess.run([str(output)], check=True, timeout=30)


if __name__ == "__main__":
    unittest.main()
