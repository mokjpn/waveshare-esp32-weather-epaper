# Weather Map Relay Server

Small Python relay for the firmware contract in `docs/architecture.md`.

## Endpoints

- `GET /api/weather-map/latest.json`
- `GET /api/weather-map/latest.bin`
- `GET /healthz`

The binary endpoint defaults to exactly `96000` bytes: one `800x480`, packed
2bpp, row-major, 4-level grayscale image for the newer Waveshare 7.5inch
black/white panel. The renderer keeps the darkest strokes black and uses the
intermediate levels for lighter map lines and antialiasing.

## Run

```sh
cd relay_server
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python3 weather_relay.py --bind 0.0.0.0 --port 8080
```

Then point firmware `APP_RELAY_BASE_URL` at the host, for example:

```c
#define APP_RELAY_BASE_URL "http://192.168.1.20:8080"
```

## Configuration

Command-line flags can also be supplied through environment variables.

| Flag | Env var | Default |
| --- | --- | --- |
| `--bind` | `JMA_RELAY_BIND` | `0.0.0.0` |
| `--port` | `JMA_RELAY_PORT` | `8080` |
| `--cache-dir` | `JMA_RELAY_CACHE_DIR` | `relay_server/cache` |
| `--cache-seconds` | `JMA_RELAY_CACHE_SECONDS` | `300` |
| `--threshold` | `JMA_RELAY_THRESHOLD` | `200` |
| `--timeout-seconds` | `JMA_RELAY_TIMEOUT_SECONDS` | `20` |
| `--image-width` | `JMA_RELAY_IMAGE_WIDTH` | `800` |
| `--image-height` | `JMA_RELAY_IMAGE_HEIGHT` | `480` |
| `--output-format` | `JMA_RELAY_OUTPUT_FORMAT` | auto: `gray4`, or `1bpp` for legacy `640x384` |
| `--annotate-timestamp` / `--no-annotate-timestamp` | `JMA_RELAY_ANNOTATE_TIMESTAMP` | `1` |
| `--red-timestamp` / `--no-red-timestamp` | `JMA_RELAY_RED_TIMESTAMP` | auto: `1` for `640x384`, otherwise `0` |
| `--access-log-file` | `JMA_RELAY_ACCESS_LOG_FILE` | stdout only |

For the newer `800x480` panel with the older 1bpp black/white contract, run
with:

```sh
python3 weather_relay.py --bind 0.0.0.0 --port 8080 --output-format 1bpp
```

For the legacy `640x384` Waveshare 7.5inch e-Paper (B) path, run with:

```sh
python3 weather_relay.py --bind 0.0.0.0 --port 8080 --image-width 640 --image-height 384
```

For `gray4`, the relay quantizes the cropped weather map into four levels:
darkest strokes become black, medium strokes become dark/light gray, and the
background stays white. For `1bpp`, lower `--threshold` values keep only darker
source pixels and higher values pick up lighter terrain/coastline strokes. The
relay draws the source time into the rendered image, converting the UTC
timestamp in the JMA image filename to JST, for example
`JMA weather map 2026-05-01 21:00 JST`. On the legacy `640x384` path, the
timestamp is drawn into a second red plane unless `--no-red-timestamp` is
supplied.

The server fetches the JMA monochrome near-Japan current weather map:

- `https://www.jma.go.jp/bosai/weather_map/data/list.json`
- `near_monochrome.now` from that list
- `https://www.jma.go.jp/bosai/weather_map/data/png/<latest near_monochrome.now png>`

If a refresh fails but a valid cached image exists, the server returns the
cached image.

The relay writes access logs with timestamp, method, path, status, response
bytes, duration, remote address, and user-agent. To append logs to a file:

```sh
python3 weather_relay.py \
  --bind 0.0.0.0 \
  --port 8080 \
  --access-log-file /var/log/weather-relay-access.log
```

Example access log line:

```text
2026-05-07T09:00:01+0900 INFO weather_relay.access access method=GET path=/api/weather-map/latest.json status=200 bytes=389 duration_ms=52.1 remote=192.168.1.68 xff=- ua="ESP32HTTPClient"
```

## Monitor

`relay_monitor.py` can run on a Linux server and periodically log which source
weather map the relay is currently serving. It only accesses the relay server;
it does not access JMA directly.

Run one check:

```sh
python3 relay_monitor.py --base-url http://127.0.0.1:8080 --once
```

Run every 30 minutes and append to a log file:

```sh
python3 relay_monitor.py \
  --base-url http://127.0.0.1:8080 \
  --interval-seconds 1800 \
  --log-file /var/log/weather-relay-monitor.log
```

Example log line:

```text
2026-05-05T12:00:00+09:00 OK source_published_at=2026-05-05T00:00:00+00:00 display_label="JMA weather map 2026-05-05 09:00 JST" etag=... size=800x480 bytes=96000 source_area=near_monochrome source_kind=now age_min=180 binary_bytes=96000
```

The monitor also performs a lightweight `HEAD` request to the relay binary
endpoint and logs `binary_bytes`. Use `--no-check-binary` to skip that check.

Environment variables:

| Flag | Env var | Default |
| --- | --- | --- |
| `--base-url` | `JMA_RELAY_MONITOR_BASE_URL` | `http://127.0.0.1:8080` |
| `--interval-seconds` | `JMA_RELAY_MONITOR_INTERVAL_SECONDS` | `1800` |
| `--timeout-seconds` | `JMA_RELAY_MONITOR_TIMEOUT_SECONDS` | `20` |
| `--log-file` | `JMA_RELAY_MONITOR_LOG_FILE` | stdout |
| `--no-check-binary` | `JMA_RELAY_MONITOR_CHECK_BINARY=0` | check enabled |

Example systemd unit:

```ini
[Unit]
Description=Weather relay monitor
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=/opt/waveshare-esp32-weather-epaper/relay_server
ExecStart=/usr/bin/python3 /opt/waveshare-esp32-weather-epaper/relay_server/relay_monitor.py --base-url http://127.0.0.1:8080 --log-file /var/log/weather-relay-monitor.log
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```
