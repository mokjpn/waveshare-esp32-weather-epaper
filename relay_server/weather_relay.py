#!/usr/bin/env python3
"""Relay server for M5Capsule weather-map e-paper firmware.

The firmware expects two endpoints:

  GET /api/weather-map/latest.json
  GET /api/weather-map/latest.bin

This server fetches the latest JMA "near / now" weather map, resizes it to the
configured panel size, converts it to packed 1bpp MSB-first bytes, and caches
the result.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import io
import json
import os
import re
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import tempfile
from typing import Any, Dict, Optional, Tuple
from urllib.parse import urlparse

from PIL import Image, ImageDraw, ImageFont, ImageOps


JMA_LIST_URL = "https://www.jma.go.jp/bosai/weather_map/data/list.json"
JMA_IMAGE_BASE_URL = "https://www.jma.go.jp/bosai/weather_map/data/png"
SOURCE_PAGE_URL = "https://www.jma.go.jp/bosai/weather_map/"

DEFAULT_IMAGE_WIDTH = 640
DEFAULT_IMAGE_HEIGHT = 384
MANIFEST_PATH = "/api/weather-map/latest.json"
BINARY_PATH = "/api/weather-map/latest.bin"
JST = dt.timezone(dt.timedelta(hours=9))
UTC = dt.timezone.utc
RENDERER_VERSION = "red-timestamp-utc-v1"


@dataclass(frozen=True)
class RelayConfig:
    bind: str
    port: int
    cache_dir: Path
    cache_seconds: int
    threshold: int
    timeout_seconds: int
    image_width: int
    image_height: int
    annotate_timestamp: bool
    red_timestamp: bool

    @property
    def packed_image_bytes(self) -> int:
        return (self.image_width * self.image_height) // 8

    @property
    def payload_image_bytes(self) -> int:
        if self.annotate_timestamp and self.red_timestamp:
            return self.packed_image_bytes * 2
        return self.packed_image_bytes


@dataclass(frozen=True)
class RenderedMap:
    manifest: Dict[str, Any]
    packed: bytes


class WeatherMapRelay:
    def __init__(self, config: RelayConfig) -> None:
        self.config = config
        self.config.cache_dir.mkdir(parents=True, exist_ok=True)
        self._cached: Optional[RenderedMap] = None
        self._cached_at = 0.0
        self._load_disk_cache()

    def latest(self, force_refresh: bool = False) -> RenderedMap:
        now = time.monotonic()
        if (
            not force_refresh
            and self._cached is not None
            and now - self._cached_at < self.config.cache_seconds
        ):
            return self._cached

        try:
            rendered = self._fetch_render_store()
            self._cached = rendered
            self._cached_at = now
            return rendered
        except Exception:
            if self._cached is not None:
                return self._cached
            raise

    def _fetch_render_store(self) -> RenderedMap:
        filename = self._latest_jma_filename()
        image_url = f"{JMA_IMAGE_BASE_URL}/{filename}"
        raw_png = self._http_get(image_url)
        published_at = published_at_from_filename(filename)
        label = display_label_from_published_at(published_at) if self.config.annotate_timestamp else None
        packed = image_to_packed_1bpp(
            raw_png,
            self.config.threshold,
            self.config.image_width,
            self.config.image_height,
            label,
            self.config.red_timestamp,
        )
        digest = hashlib.sha256(packed).hexdigest()
        planes = ["black", "red"] if label and self.config.red_timestamp else ["black"]
        manifest = {
            "image_path": BINARY_PATH,
            "source_url": SOURCE_PAGE_URL,
            "source_image_url": image_url,
            "source_published_at": published_at,
            "display_label": label or "",
            "etag": digest[:16],
            "width": self.config.image_width,
            "height": self.config.image_height,
            "bytes": len(packed),
            "planes": planes,
            "renderer_version": RENDERER_VERSION,
        }
        self._write_cache(manifest, packed)
        return RenderedMap(manifest=manifest, packed=packed)

    def _latest_jma_filename(self) -> str:
        payload = self._http_get(JMA_LIST_URL)
        data = json.loads(payload.decode("utf-8"))
        filenames = data.get("near", {}).get("now", [])
        if not isinstance(filenames, list) or not filenames:
            raise RuntimeError("JMA list did not contain near.now filenames")
        valid_filenames = [
            filename
            for filename in filenames
            if isinstance(filename, str) and "/" not in filename and filename.endswith(".png")
        ]
        if not valid_filenames:
            raise RuntimeError("JMA list did not contain valid near.now PNG filenames")
        filename = max(valid_filenames, key=filename_sort_key)
        if not isinstance(filename, str) or "/" in filename or not filename.endswith(".png"):
            raise RuntimeError(f"unexpected JMA weather map filename: {filename!r}")
        return filename

    def _http_get(self, url: str) -> bytes:
        request = urllib.request.Request(
            url,
            headers={
                "User-Agent": "m5capsule-weather-epaper-relay/0.1",
                "Accept": "*/*",
            },
        )
        with urllib.request.urlopen(request, timeout=self.config.timeout_seconds) as response:
            status = getattr(response, "status", HTTPStatus.OK)
            if status != HTTPStatus.OK:
                raise urllib.error.HTTPError(url, status, "unexpected status", response.headers, None)
            return response.read()

    def _load_disk_cache(self) -> None:
        manifest_path = self.config.cache_dir / "latest.json"
        bin_path = self.config.cache_dir / "latest.bin"
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            packed = bin_path.read_bytes()
        except FileNotFoundError:
            return
        except (OSError, json.JSONDecodeError):
            return
        if manifest.get("width") != self.config.image_width or manifest.get("height") != self.config.image_height:
            return
        if manifest.get("renderer_version") != RENDERER_VERSION:
            return
        if len(packed) != self.config.payload_image_bytes:
            return
        self._cached = RenderedMap(manifest=manifest, packed=packed)
        self._cached_at = time.monotonic()

    def _write_cache(self, manifest: Dict[str, Any], packed: bytes) -> None:
        write_atomic(self.config.cache_dir / "latest.json", json.dumps(manifest, ensure_ascii=False, indent=2).encode())
        write_atomic(self.config.cache_dir / "latest.bin", packed)


def image_to_packed_1bpp(
    raw_image: bytes,
    threshold: int,
    width: int,
    height: int,
    label: Optional[str] = None,
    red_label: bool = False,
) -> bytes:
    cropped = render_cropped_grayscale(raw_image, width, height)

    if label and red_label:
        clear_label_background(cropped, label)
        red_plane = Image.new("L", (width, height), 255)
        draw_label_text(red_plane, label)
        return pack_1bpp(cropped, threshold, width, height) + pack_1bpp(red_plane, threshold, width, height)

    if label:
        annotate_image(cropped, label)

    return pack_1bpp(cropped, threshold, width, height)


def render_cropped_grayscale(raw_image: bytes, width: int, height: int) -> Image.Image:
    with Image.open(io.BytesIO(raw_image)) as source:
        image = ImageOps.exif_transpose(source).convert("L")

    scaled_height = round(image.height * (width / image.width))
    if scaled_height < height:
        scaled_width = round(image.width * (height / image.height))
        resized = image.resize((scaled_width, height), Image.Resampling.LANCZOS)
        left = (scaled_width - width) // 2
        cropped = resized.crop((left, 0, left + width, height))
    else:
        resized = image.resize((width, scaled_height), Image.Resampling.LANCZOS)
        top = (scaled_height - height) // 2
        cropped = resized.crop((0, top, width, top + height))

    return cropped


def pack_1bpp(image: Image.Image, threshold: int, width: int, height: int) -> bytes:
    monochrome = image.point(lambda pixel: 255 if pixel >= threshold else 0, mode="1")
    packed = monochrome.tobytes()
    packed_image_bytes = (width * height) // 8
    if len(packed) != packed_image_bytes:
        raise RuntimeError(f"packed image size mismatch: {len(packed)}")
    return packed


def annotate_image(image: Image.Image, label: str) -> None:
    clear_label_background(image, label)
    draw_label_text(image, label)


def clear_label_background(image: Image.Image, label: str) -> None:
    draw = ImageDraw.Draw(image)
    x0, y0, x1, y1, _pad_x, _pad_y, _text_bbox = label_layout(draw, image, label)
    draw.rectangle((x0, y0, x1, y1), fill=255, outline=0)


def draw_label_text(image: Image.Image, label: str) -> None:
    draw = ImageDraw.Draw(image)
    x0, y0, _x1, _y1, pad_x, pad_y, text_bbox = label_layout(draw, image, label)
    draw.text((x0 + pad_x, y0 + pad_y - text_bbox[1]), label, fill=0, font=load_label_font(image.width))


def label_layout(
    draw: ImageDraw.ImageDraw,
    image: Image.Image,
    label: str,
) -> Tuple[int, int, int, int, int, int, Tuple[int, int, int, int]]:
    font = load_label_font(image.width)
    text_bbox = draw.textbbox((0, 0), label, font=font)
    text_w = text_bbox[2] - text_bbox[0]
    text_h = text_bbox[3] - text_bbox[1]
    pad_x = max(5, image.width // 128)
    pad_y = max(4, image.height // 128)
    margin = max(6, image.width // 100)
    x0 = margin
    y0 = image.height - text_h - pad_y * 2 - margin
    x1 = x0 + text_w + pad_x * 2
    y1 = y0 + text_h + pad_y * 2
    return x0, y0, x1, y1, pad_x, pad_y, text_bbox


def load_label_font(width: int) -> ImageFont.ImageFont:
    size = max(12, min(18, width // 38))
    candidates = [
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "DejaVuSans-Bold.ttf",
    ]
    for path in candidates:
        try:
            return ImageFont.truetype(path, size=size)
        except OSError:
            continue
    return ImageFont.load_default()


def display_label_from_published_at(published_at: str) -> str:
    try:
        value = dt.datetime.fromisoformat(published_at)
    except ValueError:
        return f"JMA weather map {published_at}"
    value = value.astimezone(JST)
    return value.strftime("JMA weather map %Y-%m-%d %H:%M JST")


def published_at_from_filename(filename: str) -> str:
    match = re.search(r"_C_010000_(\d{14})_MET_", filename)
    if match is None:
        match = re.match(r"(\d{14})_", filename)
    if match is None:
        return dt.datetime.now(JST).isoformat(timespec="seconds")
    value = dt.datetime.strptime(match.group(1), "%Y%m%d%H%M%S").replace(tzinfo=UTC)
    return value.isoformat(timespec="seconds")


def filename_sort_key(filename: str) -> str:
    match = re.search(r"_C_010000_(\d{14})_MET_", filename)
    if match is not None:
        return match.group(1)
    match = re.match(r"(\d{14})_", filename)
    if match is not None:
        return match.group(1)
    return filename


def write_atomic(path: Path, payload: bytes) -> None:
    fd, tmp_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(payload)
        os.replace(tmp_name, path)
    except Exception:
        try:
            os.unlink(tmp_name)
        except OSError:
            pass
        raise


def add_boolean_optional_argument(
    parser: argparse.ArgumentParser,
    name: str,
    dest: str,
    default: Optional[bool],
) -> None:
    parser.add_argument(name, dest=dest, action="store_true", default=default)
    parser.add_argument(f"--no-{name[2:]}", dest=dest, action="store_false")


class RelayRequestHandler(BaseHTTPRequestHandler):
    server_version = "WeatherMapRelay/0.1"
    relay: WeatherMapRelay

    def do_GET(self) -> None:
        self._handle(send_body=True)

    def do_HEAD(self) -> None:
        self._handle(send_body=False)

    def log_message(self, fmt: str, *args: Any) -> None:
        print(f"{self.address_string()} - {fmt % args}")

    def _handle(self, send_body: bool) -> None:
        parsed = urlparse(self.path)
        try:
            if parsed.path == MANIFEST_PATH:
                self._send_manifest(send_body)
            elif parsed.path == BINARY_PATH:
                self._send_binary(send_body)
            elif parsed.path == "/healthz":
                self._send_bytes(HTTPStatus.OK, b"ok\n", "text/plain; charset=utf-8", send_body)
            else:
                self._send_json(HTTPStatus.NOT_FOUND, {"error": "not_found"}, send_body)
        except Exception as exc:
            self._send_json(HTTPStatus.BAD_GATEWAY, {"error": "upstream_failed", "message": str(exc)}, send_body)

    def _send_manifest(self, send_body: bool) -> None:
        rendered = self.relay.latest()
        payload = json.dumps(rendered.manifest, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self._send_bytes(HTTPStatus.OK, payload, "application/json; charset=utf-8", send_body)

    def _send_binary(self, send_body: bool) -> None:
        rendered = self.relay.latest()
        self._send_bytes(HTTPStatus.OK, rendered.packed, "application/octet-stream", send_body)

    def _send_json(self, status: HTTPStatus, value: Dict[str, Any], send_body: bool) -> None:
        payload = json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self._send_bytes(status, payload, "application/json; charset=utf-8", send_body)

    def _send_bytes(self, status: HTTPStatus, payload: bytes, content_type: str, send_body: bool) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", f"public, max-age={self.relay.config.cache_seconds}")
        self.end_headers()
        if send_body:
            self.wfile.write(payload)


def parse_args() -> RelayConfig:
    parser = argparse.ArgumentParser(description="JMA weather-map relay for M5Capsule e-paper firmware")
    parser.add_argument("--bind", default=os.environ.get("JMA_RELAY_BIND", "0.0.0.0"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("JMA_RELAY_PORT", "8080")))
    parser.add_argument(
        "--cache-dir",
        type=Path,
        default=Path(os.environ.get("JMA_RELAY_CACHE_DIR", Path(__file__).with_name("cache"))),
    )
    parser.add_argument("--cache-seconds", type=int, default=int(os.environ.get("JMA_RELAY_CACHE_SECONDS", "3600")))
    parser.add_argument("--threshold", type=int, default=int(os.environ.get("JMA_RELAY_THRESHOLD", "200")))
    parser.add_argument("--timeout-seconds", type=int, default=int(os.environ.get("JMA_RELAY_TIMEOUT_SECONDS", "20")))
    parser.add_argument("--image-width", type=int, default=int(os.environ.get("JMA_RELAY_IMAGE_WIDTH", str(DEFAULT_IMAGE_WIDTH))))
    parser.add_argument(
        "--image-height",
        type=int,
        default=int(os.environ.get("JMA_RELAY_IMAGE_HEIGHT", str(DEFAULT_IMAGE_HEIGHT))),
    )
    add_boolean_optional_argument(
        parser,
        "--annotate-timestamp",
        "annotate_timestamp",
        os.environ.get("JMA_RELAY_ANNOTATE_TIMESTAMP", "1") not in {"0", "false", "False", "no", "NO"},
    )
    add_boolean_optional_argument(
        parser,
        "--red-timestamp",
        "red_timestamp",
        (
            None
            if os.environ.get("JMA_RELAY_RED_TIMESTAMP") is None
            else os.environ.get("JMA_RELAY_RED_TIMESTAMP") not in {"0", "false", "False", "no", "NO"}
        ),
    )
    args = parser.parse_args()
    if not 0 <= args.threshold <= 255:
        parser.error("--threshold must be between 0 and 255")
    if args.image_width <= 0 or args.image_width % 8 != 0:
        parser.error("--image-width must be a positive multiple of 8")
    if args.image_height <= 0:
        parser.error("--image-height must be positive")
    red_timestamp = args.red_timestamp
    if red_timestamp is None:
        red_timestamp = args.image_width == DEFAULT_IMAGE_WIDTH and args.image_height == DEFAULT_IMAGE_HEIGHT

    return RelayConfig(
        bind=args.bind,
        port=args.port,
        cache_dir=args.cache_dir,
        cache_seconds=args.cache_seconds,
        threshold=args.threshold,
        timeout_seconds=args.timeout_seconds,
        image_width=args.image_width,
        image_height=args.image_height,
        annotate_timestamp=args.annotate_timestamp,
        red_timestamp=red_timestamp,
    )


def main() -> None:
    config = parse_args()
    relay = WeatherMapRelay(config)

    class Handler(RelayRequestHandler):
        pass

    Handler.relay = relay
    server = ThreadingHTTPServer((config.bind, config.port), Handler)
    print(f"listening on http://{config.bind}:{config.port}")
    print(f"manifest: {MANIFEST_PATH}")
    print(f"binary:   {BINARY_PATH}")
    print(f"image:    {config.image_width}x{config.image_height} ({config.payload_image_bytes} bytes)")
    print(f"label:    {'enabled' if config.annotate_timestamp else 'disabled'}")
    print(f"red text: {'enabled' if config.red_timestamp else 'disabled'}")
    server.serve_forever()


if __name__ == "__main__":
    main()
