# SmallDesktopDisplay Firmware Structure

This Arduino sketch is split into feature tabs so each subsystem can change with minimal impact on the others.

- `SmallDesktopDisplay.ino`: global configuration, shared state, setup, and main loop.
- `Storage.ino`: EEPROM read/write helpers for Wi-Fi, city, brightness, rotation, refresh interval, DHT, and stock settings.
- `Display.ino`: low-level TFT/JPEG callback and reusable drawing widgets.
- `PagesTouch.ino`: page navigation/rendering and optional touch input handling.
- `ClockNtp.ino`: clock rendering, date labels, and NTP synchronization.
- `Weather.ino`: weather HTTP fetch, parsing, refresh scheduling, and weather screen rendering.
- `Sensors.ino`: optional DHT11 indoor temperature/humidity rendering.
- `WebServer.ino`: runtime configuration HTTP server and page-control endpoints.
- `WiFiManagerPortal.ino`: WiFiManager captive portal and first-boot configuration fields.
- `WiFiSetup.ino`: SmartConfig fallback when WiFiManager is disabled.
- `SerialCommands.ino`: serial configuration commands.
- `Astronaut.ino`: optional astronaut animation asset playback.

Keep new code in the module that owns the behavior. Cross-module calls should go through the existing public functions declared in `SmallDesktopDisplay.ino` instead of directly mixing unrelated responsibilities.
