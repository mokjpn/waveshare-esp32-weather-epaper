# Waveshare ESP32 E-Paper PoC Architecture

## Scope

This PoC targets these hardware and runtime assumptions.

- Controller and display driver: `Waveshare e-Paper ESP32 Driver Board`
- Display target: older `Waveshare 7.5inch e-Paper (B)` raw panel, rear label `X02R/171213-180502`, `640x384` driver path
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
7. Send the packed black and red planes.
8. Refresh the display.
9. Publish success or failure to MQTT.
10. Turn off the GPIO2 status LED and Wi-Fi/Bluetooth.
11. Deep sleep until the next scheduled update time.

## Schedule

The schedule is fixed in JST.

- 00:00
- 06:00
- 12:00
- 18:00

Failures currently retry after 5 minutes.

## Relay Server Contract

The firmware expects the relay server to provide already processed image data.  
This keeps TLS, scraping, image resizing, center-cropping, and 1bpp packing off the device.

### Manifest endpoint

- `GET /api/weather-map/latest.json`

Expected JSON:

```json
{
  "image_path": "/api/weather-map/latest.bin",
  "source_url": "https://www.jma.go.jp/bosai/weather_map/",
  "source_published_at": "2026-04-17T12:00:00+00:00",
  "display_label": "JMA weather map 2026-04-17 21:00 JST",
  "etag": "optional-version-string",
  "width": 640,
  "height": 384,
  "bytes": 61440,
  "planes": ["black", "red"],
  "renderer_version": "red-timestamp-utc-v1"
}
```

### Binary endpoint

- `GET /api/weather-map/latest.bin`

Expected payload:

- exactly `61440` bytes by default
- two `640x384` monochrome packed `1bpp` planes
- black plane first, red plane second
- row-major
- MSB-first within each byte

## Image Processing Rules

The relay server should transform the JMA image like this.

1. Use the latest published `日本周辺域 実況天気図`.
2. Scale to width `640` while preserving aspect ratio.
3. Keep the image center fixed.
4. Crop vertically to `384` pixels.
5. Do not add margins.
6. Clear a timestamp label background near the lower-left corner in the black plane.
7. Draw the source timestamp label in the red plane.
8. Convert each plane to 1bpp monochrome.

## SPI E-Paper Write Strategy

The active firmware target is PlatformIO environment `waveshare_esp32_epd_7in5bc`.

The Waveshare ESP32 driver board has fixed e-paper pins:

- `DIN/MOSI`: GPIO14
- `SCLK`: GPIO13
- `CS`: GPIO15
- `DC`: GPIO27
- `RST`: GPIO26
- `BUSY`: GPIO25

The older `7in5bc` driver path converts the relay's `1bpp` planes into the panel's two-pixels-per-byte transfer format:

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
