#!/usr/bin/env python3
"""Periodic monitor for the weather-map relay server.

The monitor only talks to the relay endpoints. It does not access JMA directly.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from typing import Any, Dict, Optional


DEFAULT_BASE_URL = "http://127.0.0.1:8080"
DEFAULT_INTERVAL_SECONDS = 30 * 60
DEFAULT_TIMEOUT_SECONDS = 20
MANIFEST_PATH = "/api/weather-map/latest.json"


@dataclass(frozen=True)
class MonitorConfig:
    base_url: str
    interval_seconds: int
    timeout_seconds: int
    log_file: Optional[str]
    check_binary: bool
    once: bool


def parse_args() -> MonitorConfig:
    parser = argparse.ArgumentParser(
        description="Log the source weather-map time currently served by a relay server",
    )
    parser.add_argument(
        "--base-url",
        default=os.environ.get("JMA_RELAY_MONITOR_BASE_URL", DEFAULT_BASE_URL),
        help=f"relay base URL, default: {DEFAULT_BASE_URL}",
    )
    parser.add_argument(
        "--interval-seconds",
        type=int,
        default=int(os.environ.get("JMA_RELAY_MONITOR_INTERVAL_SECONDS", str(DEFAULT_INTERVAL_SECONDS))),
        help=f"polling interval, default: {DEFAULT_INTERVAL_SECONDS}",
    )
    parser.add_argument(
        "--timeout-seconds",
        type=int,
        default=int(os.environ.get("JMA_RELAY_MONITOR_TIMEOUT_SECONDS", str(DEFAULT_TIMEOUT_SECONDS))),
        help=f"HTTP timeout, default: {DEFAULT_TIMEOUT_SECONDS}",
    )
    parser.add_argument(
        "--log-file",
        default=os.environ.get("JMA_RELAY_MONITOR_LOG_FILE"),
        help="append logs to this file instead of stdout",
    )
    parser.add_argument(
        "--no-check-binary",
        dest="check_binary",
        action="store_false",
        default=os.environ.get("JMA_RELAY_MONITOR_CHECK_BINARY", "1") not in {"0", "false", "False", "no", "NO"},
        help="skip HEAD check of the relay binary endpoint",
    )
    parser.add_argument("--once", action="store_true", help="run one check and exit")
    args = parser.parse_args()

    if args.interval_seconds <= 0:
        parser.error("--interval-seconds must be positive")
    if args.timeout_seconds <= 0:
        parser.error("--timeout-seconds must be positive")

    return MonitorConfig(
        base_url=args.base_url.rstrip("/"),
        interval_seconds=args.interval_seconds,
        timeout_seconds=args.timeout_seconds,
        log_file=args.log_file,
        check_binary=args.check_binary,
        once=args.once,
    )


def fetch_manifest(config: MonitorConfig) -> Dict[str, Any]:
    url = join_url(config.base_url, MANIFEST_PATH)
    request = urllib.request.Request(
        url,
        headers={
            "Accept": "application/json",
            "User-Agent": "weather-relay-monitor/0.1",
        },
    )
    with urllib.request.urlopen(request, timeout=config.timeout_seconds) as response:
        status = getattr(response, "status", 200)
        if status != 200:
            raise urllib.error.HTTPError(url, status, "unexpected status", response.headers, None)
        payload = response.read()
    data = json.loads(payload.decode("utf-8"))
    if not isinstance(data, dict):
        raise RuntimeError("manifest response is not a JSON object")
    return data


def head_binary(config: MonitorConfig, image_path: str) -> Optional[int]:
    url = join_url(config.base_url, image_path)
    request = urllib.request.Request(
        url,
        method="HEAD",
        headers={"User-Agent": "weather-relay-monitor/0.1"},
    )
    with urllib.request.urlopen(request, timeout=config.timeout_seconds) as response:
        status = getattr(response, "status", 200)
        if status != 200:
            raise urllib.error.HTTPError(url, status, "unexpected status", response.headers, None)
        length = response.headers.get("Content-Length")
    return int(length) if length is not None else None


def join_url(base_url: str, path_or_url: str) -> str:
    parsed = urllib.parse.urlparse(path_or_url)
    if parsed.scheme and parsed.netloc:
        return path_or_url
    return urllib.parse.urljoin(f"{base_url}/", path_or_url.lstrip("/"))


def format_success(config: MonitorConfig, manifest: Dict[str, Any]) -> str:
    published_at = str(manifest.get("source_published_at", ""))
    source_dt = parse_datetime(published_at)
    age_text = ""
    if source_dt is not None:
        now = dt.datetime.now(source_dt.tzinfo or dt.datetime.now().astimezone().tzinfo)
        age_minutes = int((now - source_dt).total_seconds() // 60)
        age_text = f" age_min={age_minutes}"

    binary_text = ""
    if config.check_binary:
        image_path = str(manifest.get("image_path", ""))
        if image_path:
            try:
                length = head_binary(config, image_path)
                binary_text = f" binary_bytes={length if length is not None else 'unknown'}"
            except Exception as exc:
                binary_text = f" binary_check=error:{compact_error(exc)}"
        else:
            binary_text = " binary_check=skipped:no_image_path"

    fields = [
        "OK",
        f"source_published_at={published_at or 'unknown'}",
        f"display_label={quote_field(str(manifest.get('display_label', '')))}",
        f"etag={manifest.get('etag', 'unknown')}",
        f"size={manifest.get('width', '?')}x{manifest.get('height', '?')}",
        f"bytes={manifest.get('bytes', 'unknown')}",
        f"source_area={manifest.get('source_area', 'unknown')}",
        f"source_kind={manifest.get('source_kind', 'unknown')}",
    ]
    return " ".join(fields) + age_text + binary_text


def parse_datetime(value: str) -> Optional[dt.datetime]:
    if not value:
        return None
    try:
        parsed = dt.datetime.fromisoformat(value)
    except ValueError:
        return None
    if parsed.tzinfo is None:
        return parsed.replace(tzinfo=dt.datetime.now().astimezone().tzinfo)
    return parsed


def quote_field(value: str) -> str:
    if not value:
        return "-"
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def compact_error(exc: Exception) -> str:
    text = str(exc).replace("\n", " ").strip()
    return text if text else exc.__class__.__name__


def log_line(config: MonitorConfig, message: str) -> None:
    timestamp = dt.datetime.now().astimezone().isoformat(timespec="seconds")
    line = f"{timestamp} {message}"
    if config.log_file:
        try:
            with open(config.log_file, "a", encoding="utf-8") as handle:
                handle.write(line + "\n")
            return
        except OSError as exc:
            print(f"{timestamp} LOG_ERROR {compact_error(exc)}", file=sys.stderr, flush=True)
            print(line, file=sys.stderr, flush=True)
    else:
        print(line, flush=True)


def run_check(config: MonitorConfig) -> bool:
    try:
        manifest = fetch_manifest(config)
        log_line(config, format_success(config, manifest))
        return True
    except Exception as exc:
        log_line(config, f"ERROR {compact_error(exc)}")
        return False


def main() -> int:
    config = parse_args()
    exit_code = 0
    while True:
        if not run_check(config):
            exit_code = 1
        if config.once:
            return exit_code
        time.sleep(config.interval_seconds)


if __name__ == "__main__":
    raise SystemExit(main())
