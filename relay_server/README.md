# Weather Map Relay Server

Small Python relay for the firmware contract in `docs/architecture.md`.

## Endpoints

- `GET /api/weather-map/latest.json`
- `GET /api/weather-map/latest.bin`
- `GET /healthz`

The binary endpoint defaults to exactly `61440` bytes: two `640x384`, packed
`1bpp`, row-major, MSB-first planes. The first `30720` bytes are black pixels,
and the second `30720` bytes are red pixels for the source timestamp label.
This matches the older Waveshare 7.5inch e-Paper (B) `X02R/171213-180502`
panel path.

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
| `--image-width` | `JMA_RELAY_IMAGE_WIDTH` | `640` |
| `--image-height` | `JMA_RELAY_IMAGE_HEIGHT` | `384` |
| `--annotate-timestamp` / `--no-annotate-timestamp` | `JMA_RELAY_ANNOTATE_TIMESTAMP` | `1` |
| `--red-timestamp` / `--no-red-timestamp` | `JMA_RELAY_RED_TIMESTAMP` | auto: `1` for `640x384`, otherwise `0` |

For a newer `800x480` panel, run with:

```sh
python3 weather_relay.py --bind 0.0.0.0 --port 8080 --image-width 800 --image-height 480
```

Lower `--threshold` values keep only darker source pixels. Higher values pick
up lighter terrain/coastline strokes, which is useful on the older monochrome
panel. The relay also draws the source time into the red plane by default,
converting the UTC timestamp in the JMA image filename to JST, for example
`JMA weather map 2026-05-01 21:00 JST`.

The server fetches the JMA monochrome near-Japan current weather map:

- `https://www.jma.go.jp/bosai/weather_map/data/list.json`
- `near_monochrome.now` from that list
- `https://www.jma.go.jp/bosai/weather_map/data/png/<latest near_monochrome.now png>`

If a refresh fails but a valid cached image exists, the server returns the
cached image.
