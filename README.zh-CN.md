# TinyTV 固件源码

这是 ESP8266 TinyTV / 天气时钟固件源码，整理自立创开源硬件平台项目 `https://oshwhub.com/q21182889/sd2`。

许可证：GPL 3.0。

## 项目结构

- `firmware/SmallDesktopDisplay/SmallDesktopDisplay.ino`：主 Arduino 固件草图。
- `firmware/SmallDesktopDisplay/font/`：生成的字体位图头文件。
- `firmware/SmallDesktopDisplay/img/`：生成的图片和天气图标头文件。
- `firmware/libraries/`：固件依赖的随仓库 Arduino 库。
- `firmware/libraries/TFT_eSPI/User_Setup.h`：TFT_eSPI 屏幕驱动和引脚配置。
- `screen-test/ScreenTest/`：用于验证屏幕和引脚的最小测试草图。
- `tools/read-esp8266-bootlog.py`：读取 ESP8266 启动串口日志的辅助脚本。
- `tests/`：仓库结构和关键配置的 Python 单元测试。

## Arduino 配置

草图对应的 VS Code Arduino 配置位于 `firmware/SmallDesktopDisplay/.vscode/arduino.json`。

- 开发板：`esp8266:esp8266:nodemcuv2`
- Flash 布局：`eesz=4M2M`
- 上传波特率：`921600`
- 草图文件：`SmallDesktopDisplay.ino`

当前 TFT 配置位于 `firmware/libraries/TFT_eSPI/User_Setup.h`。

- 驱动：`ST7789_2_DRIVER`
- 分辨率：`240x240`
- 引脚：`TFT_CS PIN_D8`、`TFT_DC PIN_D3`、`TFT_RST PIN_D4`、`TFT_BL PIN_D1`

## 常用命令

编译主固件：

```bash
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 firmware/SmallDesktopDisplay
```

通过 CH340 / CH341 串口适配器上传固件：

```bash
arduino-cli upload -p /dev/cu.usbserial-110 --fqbn esp8266:esp8266:nodemcuv2 firmware/SmallDesktopDisplay
```

读取 ESP8266 启动日志：

```bash
python3 tools/read-esp8266-bootlog.py --port /dev/cu.usbserial-110 --seconds 8
```

运行仓库完整性测试：

```bash
python3 -m unittest discover -s tests
```

## 开发注意事项

- 固件逻辑修改尽量集中在 `firmware/SmallDesktopDisplay/`。
- 不要手工修改 `font/` 和 `img/` 下的生成头文件，除非同时重新生成来源资源。
- 调整屏幕驱动、分辨率或引脚前，优先使用 `screen-test/ScreenTest/ScreenTest.ino` 做最小验证。
- 不要提交 Wi-Fi 凭据、API Key、设备专属备份或本地串口配置。

## 验证建议

提交前建议至少运行：

```bash
python3 -m unittest discover -s tests
```

如果本机安装了 `arduino-cli` 和 ESP8266 平台，也建议编译主固件：

```bash
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 firmware/SmallDesktopDisplay
```
