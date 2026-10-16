# SmallDesktopDisplay Firmware Structure

This Arduino sketch is split into feature tabs so each subsystem can change with minimal impact on the others.

- `SmallDesktopDisplay.ino`: global configuration, shared state, setup, and main loop.
- `Storage.ino`: EEPROM read/write helpers for Wi-Fi, city, brightness, rotation, refresh interval, DHT, and stock settings.
- `Display.ino`: low-level TFT/JPEG callback and reusable drawing widgets.
- `PagesTouch.ino`: page navigation/rendering and optional touch input handling.
- `ClockNtp.ino`: clock rendering and native-size date labels.
- `AsyncNtp.ino`: nonblocking DNS/UDP time synchronization.
- `AsyncNetwork.ino`: shared lwIP HTTP state machine; no remote I/O in renderers.
- `RuntimeCore.h`: portable frame scheduling, UTF-8 traversal, request tokens, and HTTP framing.
- `RenderBuffers.ino`: persistent partial drawing buffers.
- `RuntimeMetrics.ino`: serial performance counters and read-only `/metrics` JSON.
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
Codex queues a refresh (minimum three seconds between attempts), and long press returns to Clock. While visible,
Codex refreshes every 60 seconds from `http://aihub.lan/api/usage/quotas` and
selects the `provider == "codex"` entry. The device must be able to resolve and
reach `aihub.lan` on its Wi-Fi network.

The 240×240 page shows the plan, the first two quota windows (currently 5 hours
and 7 days), used percentages and progress bars, reset times, and API `readAt`.
Times use the clock's UTC+8 setting. The API exposes quota usage percentages,
not request counts. Errors retain the last successful snapshot and show a
cached-data status; retries are rate limited to once per minute unless manually
requested. No account identity or other providers' usage is displayed.

## Display and network scheduling

The 450ms double-tap window, five pages, EEPROM format, pin assignments, SPI
frequency, and 80MHz / 4M2M target are preserved. A page change first renders
cached values or placeholders. Manual refresh and periodic requests are handled
on later loop iterations. Leaving a page cancels its TCP connection and invalidates
its outstanding DNS token; configuration changes invalidate the affected request.
Stock quotes run one at a time. Failures preserve successful values.

HTTP supports status checking, Content-Length, chunked transfer (including
trailers), and connection-close framing. Requests have a five-second total
deadline; response bodies are capped at 16KiB, header lines at 255 bytes, and
headers/trailers at 4KiB. Unsupported encodings and malformed/truncated responses
fail closed. NTP has separate asynchronous DNS and UDP states and retains the
existing three servers and UTC+8 setting. Reading the clock cannot initiate I/O.

Animation submits only when its 37ms frame period expires, skipping missed
frames. A 70×70 RGB565 image is decoded before one region transfer. Animation
and HTTP pages share a persistent 16KiB workspace; a reusable 130×28 8-bit sprite
handles stock value strips. Allocation failure falls back to direct region drawing
(the network request fails without replacing cache if its workspace is unavailable).
There is no full-screen framebuffer. Chinese glyphs always use the generated
native 16×16 data; measured ASCII advances and UTF-8-safe ellipsis keep mixed text
inside its field. Shorter values overwrite the entire previous field.

## Verification

```sh
python3 -m unittest discover -s tests
.agents/skills/tinytv-compile-upload/scripts/compile_upload.sh --compile-only
python3 tests/device_soak.py --url http://sd3.local --minutes 30 --output /tmp/tinytv-soak.jsonl
```

The host suite compiles and executes the firmware's portable code with address
and undefined-behavior sanitizers. It also runs the original touch polling function
against simulated pin levels and tests the actual text renderer against a display
stub. A midnight rollover test advances time during JPEG decoding to verify that
each clock redraw uses one consistent timestamp. A C++ compiler is required. The optional device soak switches through all
five pages and records uptime, heap, request outcomes, animation rate, and page
latency without changing EEPROM. `/metrics` exposes diagnostic counters only.
Serial emits `PERF` every ten seconds and a brief `NET` line per completed request;
network counters describe HTTP transport, while `*_valid` indicates parsed cache.
`decode_avg_us` includes JPEG decode and transfer, and `first_max_us` starts after
gesture recognition; the deliberate 450ms single-tap wait is separate.

### Measured device result — 2026-10-16

Built for NodeMCU ESP8266 at 80MHz with 4M2M flash layout, uploaded to the
connected ESP8266EX / 4MB device, and verified by esptool's flash hash check.
The unchanged screen-test sketch was run before modifying rendering; its serial
red/green/blue/white sequence completed. The ten repository/host tests passed.

| Measurement | Instrumented original | Optimized firmware |
| --- | ---: | ---: |
| Animation submissions / second | ~45, including repeats | 27.03, scheduled frames |
| JPEG decode + transfer average | ~21.4ms | 21.6ms |
| Longest measured loop body | 2376ms | 137ms |
| Page intent to drawing start | Not measured | 2.77ms maximum |
| Full page draw, including original synchronous fetch | 2368ms maximum | 128ms maximum |

The final firmware completed 1800.26 seconds of five-page rotation with 178
samples and no uptime reset. Clock samples had a median of 27.027fps and a minimum
of 26.907fps. Sampled free heap ranged from 7832 to 10112 bytes, ending at 9752;
the final largest free block was 9264 bytes. After cache warmup, repeated clock
visits showed no continuing heap decline. These are ten-second idle samples,
not a measurement of transient allocation peaks.

During the run, 288 stock, 6 weather, and 12 Codex responses completed successfully.
One weather response failed transport/framing validation; cached weather remained
valid and subsequent refreshes succeeded. The maximum page-control HTTP round trip
was 178ms; this includes request/response transfer and differs from the 2.77ms
on-device intent-to-draw measurement. Neither includes the touch gesture's
intentional 450ms recognition window.

A temporary test firmware, kept outside the repository, routed requests to a local
fixture. All three parsers retained cache after 503 responses, invalid JSON,
oversized bodies, slow responses, and failed DNS. Fixed-length, chunked/trailer,
and connection-close responses succeeded. Long mixed Chinese names, negative
temperatures, and shorter replacement values were exercised. Switching away from
a slow request took 88ms for the control HTTP round trip; group changes invalidated
in-flight stock requests. A scheduled navigation action still ran 801ms after the
start of a deliberate disconnect (target: 800ms), then Wi-Fi reconnected and weather
updated. Test endpoints and fixture routing are absent from the final firmware.

Physical panel appearance and direct finger input were not observed automatically.
Gesture timing is covered by the real polling-code simulation; visual clearance
and glyph boundaries are covered by renderer tests. A person should still inspect
the panel for visible artifacts and confirm the physical touch response.
