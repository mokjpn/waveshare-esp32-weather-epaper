# Waveshare ESP32 E-Paper PoC Architecture

## Scope

This PoC targets these hardware and runtime assumptions.

- Controller and display driver: `Waveshare e-Paper ESP32 Driver Board`
- Default display target: newer `Waveshare 7.5inch e-Paper` black/white raw panel, `800x480`, 4-level grayscale
- Legacy display target: older `Waveshare 7.5inch e-Paper (B)` raw panel, rear label `X02R/171213-180502`, `640x384` driver path
- Framework: `Arduino` on `PlatformIO`
- Power: battery operation with deep sleep
- Network: Wi-Fi to a relay server and MQTT broker

## Firmware Flow

1. Boot on timer wake or button wake.
2. Connect Wi-Fi.
3. Sync time with NTP.
4. Fetch a relay manifest from `APP_RELAY_BASE_URL + /api/weather-map/latest.json`.
5. Download the processed packed image binary.
6. Initialize the SPI e-paper interface.
7. Send the packed 4-level grayscale image.
8. Refresh the display.
9. Publish success or failure to MQTT.
10. Turn off the GPIO2 status LED and Wi-Fi/Bluetooth.
11. Deep sleep until the next scheduled update time.

## Schedule

The schedule is fixed in JST and follows the JMA weather-map observation cadence.
JMA maps are published roughly 2 hours and 10 minutes after the observation time,
so each scheduled run normally fetches the map from the previous 3-hour observation.

- 00:00
- 03:00
- 06:00
- 09:00
- 12:00
- 15:00
- 18:00
- 21:00

Wi-Fi and relay image-fetch failures retry after 10 minutes. Other update failures retry after 5 minutes. MQTT failures are non-fatal.
An image transfer that makes no receive progress for 30 seconds is aborted and treated as a relay image-fetch failure.
Wi-Fi modem sleep is disabled while the device is awake to reduce the chance of stalled image transfers.
The relay's default upstream/render cache TTL is 300 seconds.

## Relay Server Contract

The firmware expects the relay server to provide already processed image data.  
This keeps TLS, scraping, image resizing, center-cropping, and display-format packing off the device.

### Manifest endpoint

- `GET /api/weather-map/latest.json`

Expected JSON:

```json
{
  "image_path": "/api/weather-map/latest.bin",
  "source_url": "https://www.jma.go.jp/bosai/weather_map/",
  "source_image_url": "https://www.jma.go.jp/bosai/weather_map/data/png/...",
  "source_area": "near_monochrome",
  "source_kind": "now",
  "source_published_at": "2026-04-17T12:00:00+00:00",
  "display_label": "JMA weather map 2026-04-17 21:00 JST",
  "etag": "optional-version-string",
  "width": 800,
  "height": 480,
  "bytes": 96000,
  "planes": ["gray4"],
  "format": "gray4",
  "gray_levels": 4,
  "renderer_version": "weather-map-v4-near-monochrome-gray4"
}
```

### Binary endpoint

- `GET /api/weather-map/latest.bin`

Expected payload:

- exactly `96000` bytes by default
- one `800x480`, 2bpp, 4-level grayscale image
- row-major
- four pixels per byte
- each pixel uses the next two high-to-low bits in the byte

## Image Processing Rules

The relay server should transform the JMA image like this.

1. Use the latest published `near_monochrome.now` `日本周辺域 実況天気図`.
2. Scale to width `800` while preserving aspect ratio.
3. Keep the image center fixed.
4. Crop vertically to `480` pixels.
5. Do not add margins.
6. Clear a timestamp label background near the lower-left corner.
7. Draw the source timestamp label in the black plane.
8. Quantize the image to 4 grayscale levels while forcing darkest strokes to black.

## SPI E-Paper Write Strategy

The default firmware target is PlatformIO environment `waveshare_esp32_epd`.
It targets the newer `800x480` black/white 7.5inch panel and uses the 4-level
grayscale refresh path by default.

The fallback 1bpp target is PlatformIO environment `waveshare_esp32_epd_1bpp`.
Use it with relay option `--output-format 1bpp` to keep the newer panel on the
older black/white payload contract.

The legacy firmware target is PlatformIO environment `waveshare_esp32_epd_7in5bc`.
Use it only for the older `640x384` 7.5inch e-Paper (B) path.

The Waveshare ESP32 driver board has fixed e-paper pins:

- `DIN/MOSI`: GPIO14
- `SCLK`: GPIO13
- `CS`: GPIO15
- `DC`: GPIO27
- `RST`: GPIO26
- `BUSY`: GPIO25

Default `waveshare_esp32_epd` write path:

- grayscale source: the `96000` relay bytes
- firmware expands the packed 2bpp image into the `0x10` and `0x13` RAM planes expected by Waveshare's `epd7in5_V2` 4-gray update sequence

Legacy `7in5bc` write path converts the relay's `1bpp` planes into the panel's two-pixels-per-byte transfer format:

- black source: the first `30720` relay bytes
- red source: the second `30720` relay bytes
- red pixels take priority if both planes mark the same pixel

If the relay still serves the older single-plane `30720` byte payload, the firmware treats the missing red plane as white for compatibility.

## Power Notes

The Waveshare ESP32 driver board schematic shows `LED1` connected to `GPIO2`; firmware turns this LED on only while awake and off before deep sleep. A separate USB-UART/power indicator may remain lit independently of firmware. Waveshare documents DIP switch 2 as the USB-UART module power switch for saving power when uploads/serial monitoring are not needed.

## MQTT

The firmware publishes JSON events to:

- `APP_MQTT_TOPIC_BASE/events`

Events include:

- boot
- manifest
- epaper_update
- retry
- sleep
