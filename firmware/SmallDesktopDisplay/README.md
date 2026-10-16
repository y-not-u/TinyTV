# SmallDesktopDisplay Firmware Structure

This Arduino sketch is split into feature tabs so each subsystem can change with minimal impact on the others.

- `SmallDesktopDisplay.ino`: global configuration, shared state, setup, and main loop.
- `Storage.ino`: EEPROM read/write helpers for Wi-Fi, city, brightness, rotation, refresh interval, DHT, and stock settings.
- `Display.ino`: low-level TFT/JPEG callback and reusable drawing widgets.
- `PagesTouch.ino`: page navigation/rendering and optional touch input handling.
- `ClockNtp.ino`: clock rendering, date labels, and NTP synchronization.
- `Weather.ino`: weather HTTP fetch, parsing, refresh scheduling, and weather screen rendering.
- `Codex.ino`: Codex quota fetch and screen rendering, with cached data on errors.
- `Sensors.ino`: optional DHT11 indoor temperature/humidity rendering.
- `WebServer.ino`: runtime configuration HTTP server and page-control endpoints.
- `WiFiManagerPortal.ino`: WiFiManager captive portal and first-boot configuration fields.
- `WiFiSetup.ino`: SmartConfig fallback when WiFiManager is disabled.
- `SerialCommands.ino`: serial configuration commands.
- `Astronaut.ino`: optional astronaut animation asset playback.

Keep new code in the module that owns the behavior. Cross-module calls should go through the existing public functions declared in `SmallDesktopDisplay.ino` instead of directly mixing unrelated responsibilities.

## Codex quota page

Single tap cycles Clock → Stock → Weather → Codex → Settings; double tap on
Codex refreshes immediately, and long press returns to Clock. While visible,
Codex refreshes every 60 seconds from `http://aihub.lan/api/usage/quotas` and
selects the `provider == "codex"` entry. The device must be able to resolve and
reach `aihub.lan` on its Wi-Fi network.

The 240×240 page shows the plan, the first two quota windows (currently 5 hours
and 7 days), used percentages and progress bars, reset times, and API `readAt`.
Times use the clock's UTC+8 setting. The API exposes quota usage percentages,
not request counts. Errors retain the last successful snapshot and show a
cached-data status; retries are rate limited to once per minute unless manually
requested. No account identity or other providers' usage is displayed.
