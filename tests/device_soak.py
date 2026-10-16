#!/usr/bin/env python3
"""Optional real-device soak; uses existing page controls plus /metrics.

Run explicitly, not through unittest discovery. No EEPROM settings are changed.
"""
import argparse
import json
import statistics
import time
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://sd3.local")
    parser.add_argument("--minutes", type=float, default=30)
    parser.add_argument("--output", required=True, help="Local JSONL evidence path")
    args = parser.parse_args()
    if args.minutes <= 0:
        parser.error("--minutes must be positive")
    base = args.url.rstrip("/")

    def request(path):
        with urllib.request.urlopen(base + path, timeout=5) as response:
            return response.read()

    def metrics():
        return json.loads(request("/metrics"))

    started = time.monotonic()
    request("/SetPage?LongClick")
    time.sleep(1)
    previous = metrics()
    # Each five-minute cycle exercises all pages and manual refresh actions.
    durations = [120, 60, 40, 40, 40]
    page = 0
    next_page = time.monotonic() + durations[page]
    fps = []
    heap = []
    control_ms = []
    samples = 0
    with open(args.output, "w", encoding="utf-8") as output:
        while time.monotonic() - started < args.minutes * 60:
            remaining = args.minutes * 60 - (time.monotonic() - started)
            time.sleep(min(10, max(0, remaining)))
            current = metrics()
            if current["uptime_ms"] <= previous["uptime_ms"]:
                raise RuntimeError("Device restarted or stopped advancing uptime")
            if current["page"] != page:
                raise RuntimeError("Unexpected page change")
            if page == 0 and previous["page"] == 0:
                fps.append((current["frames"] - previous["frames"]) * 1000 /
                           (current["uptime_ms"] - previous["uptime_ms"]))
            heap.append(current["heap"])
            row = {"elapsed_s": round(time.monotonic() - started, 3), **current}
            output.write(json.dumps(row) + "\n")
            output.flush()
            samples += 1
            previous = current
            if time.monotonic() >= next_page:
                begin = time.monotonic()
                request("/SetPage?Click")
                control_ms.append((time.monotonic() - begin) * 1000)
                page = (page + 1) % 5
                next_page = time.monotonic() + durations[page]
                # Quota/weather refresh failures remain responsive and cached.
                if page in (2, 3):
                    request("/SetPage?DoubleClick")
                time.sleep(.2)
                previous = metrics()
    request("/SetPage?LongClick")
    summary = {
        "elapsed_s": round(time.monotonic() - started, 2), "samples": samples,
        "fps_median": statistics.median(fps) if fps else None,
        "fps_min": min(fps) if fps else None,
        "heap_min": min(heap), "heap_max": max(heap),
        "control_max_ms": max(control_ms) if control_ms else None,
        "last": current,
    }
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
