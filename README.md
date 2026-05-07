# Waveshare ESP32 Weather E-Paper

Firmware, relay server, and 3D-printable case files for a battery-powered
weather-map e-paper display.

This repository targets:

- Waveshare e-Paper ESP32 Driver Board
- Newer Waveshare 7.5inch e-Paper black/white raw panel, `800x480`,
  4-level grayscale
- Japan Meteorological Agency weather map source data
- A small Python relay server that prepares panel-ready packed bitmap data

## What It Does

1. Wakes on schedule.
2. Connects to Wi-Fi.
3. Synchronizes time with NTP.
4. Fetches a processed weather-map manifest and binary image from the relay.
5. Sends a packed 4-level grayscale weather map to the e-paper panel over SPI.
6. Publishes status events to MQTT.
7. Turns off Wi-Fi/Bluetooth and enters deep sleep.

The default firmware environment is `waveshare_esp32_epd`, which uses the newer
`800x480` panel's 4-gray update path. The relay defaults to `gray4` output:
one packed `800x480`, 2bpp payload of `96000` bytes. The darkest source strokes
are forced to black, with intermediate grays used for lighter map lines and
antialiasing.

## Repository Contents

- `src/main.cpp` - Arduino/PlatformIO firmware entry point
- `lib/WaveshareEsp32Epaper/` - SPI e-paper driver-board support
- `include/app_config.h` - non-secret firmware configuration
- `include/app_secrets.example.h` - template for local secrets
- `relay_server/` - Python JMA weather-map relay
- `cad/waveshare_7in5b_esp32_case.scad` - parametric OpenSCAD case model
- `cad/*.stl` - exported printable parts
- `docs/architecture.md` - firmware and relay API contract
- `docs/case_design.md` - case dimensions, export commands, and print notes

## Secrets

Real Wi-Fi, relay, and MQTT credentials must only be placed in
`include/app_secrets.h`. That file is ignored by Git and is intentionally not
published.

To configure locally:

```sh
cp include/app_secrets.example.h include/app_secrets.h
```

Then edit `include/app_secrets.h` with your local values.

## Build Firmware

Install PlatformIO, then run:

```sh
PLATFORMIO_CORE_DIR=/tmp/pio-core pio run
```

Diagnostic stripe-pattern firmware:

```sh
PLATFORMIO_CORE_DIR=/tmp/pio-core pio run -e waveshare_esp32_epd_diag
```

Upload example:

```sh
PLATFORMIO_CORE_DIR=/tmp/pio-core pio run -e waveshare_esp32_epd -t upload --upload-port /dev/tty.usbmodemXXXX
```

Fallback build targets:

- `waveshare_esp32_epd_1bpp` - newer `800x480` panel with the older 1bpp black/white payload contract
- `waveshare_esp32_epd_7in5bc` - legacy `640x384` Waveshare 7.5inch e-Paper (B), rear label `X02R/171213-180502`

## Run Relay Server

```sh
cd relay_server
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python3 weather_relay.py --bind 0.0.0.0 --port 8080
```

The relay defaults to `800x480` `gray4` output for the newer panel. To serve the
newer panel with the older 1bpp black/white contract instead:

```sh
python3 weather_relay.py --bind 0.0.0.0 --port 8080 --output-format 1bpp
```

For the legacy `640x384` panel path:

```sh
python3 weather_relay.py --bind 0.0.0.0 --port 8080 --image-width 640 --image-height 384
```

## Case Files

The OpenSCAD model exports these printable parts:

```sh
openscad -D 'part="front"' -o cad/waveshare_7in5b_front.stl cad/waveshare_7in5b_esp32_case.scad
openscad -D 'part="back"' -o cad/waveshare_7in5b_back.stl cad/waveshare_7in5b_esp32_case.scad
openscad -D 'part="back_battery"' -o cad/waveshare_7in5b_back_battery.stl cad/waveshare_7in5b_esp32_case.scad
```

The battery-back variant is designed around a cheero Canvas 3200mAh IoT USB-C
Ver. class battery installed landscape and powered through a short internal
USB-A-to-USB-C cable.

## Hardware Notes

- Waveshare ESP32 driver-board pins are fixed:
  `DIN=GPIO14`, `SCLK=GPIO13`, `CS=GPIO15`, `DC=GPIO27`, `RST=GPIO26`,
  `BUSY=GPIO25`.
- Firmware controls driver-board `LED1` through `GPIO2` while awake.
- The default newer-panel path uses Waveshare's `epd7in5_V2` 4-gray update
  sequence.
- The older `7in5bc` path performs a clear refresh before drawing to reduce
  ghosting.
- The raw panel is fragile. Confirm FPC orientation, driver-board DIP switches,
  and case clearances before final assembly.

---

# Waveshare ESP32 Weather E-Paper 日本語

Waveshare ESP32 e-Paper Driver Board を使った、バッテリー駆動の天気図
e-paper 表示機用ファームウェア、リレーサーバー、3Dプリント用ケースファイルです。

対象ハードウェア:

- Waveshare e-Paper ESP32 Driver Board
- 新しい Waveshare 7.5inch e-Paper 黒白生パネル、`800x480`、4階調対応
- 気象庁の天気図データ
- パネルにそのまま送れるビットマップへ変換する Python リレーサーバー

## 機能

1. スケジュールに従って起床します。
2. Wi-Fi に接続します。
3. NTP で時刻同期します。
4. リレーサーバーから処理済み天気図の manifest とバイナリ画像を取得します。
5. SPI 経由で e-paper パネルへ4階調の天気図を書き込みます。
6. MQTT に状態イベントを送信します。
7. Wi-Fi/Bluetooth を停止して deep sleep に入ります。

通常使用する PlatformIO 環境は `waveshare_esp32_epd` です。新しい
`800x480` パネルの4階調更新経路を使います。リレーサーバーは標準で `gray4`
出力になっており、`800x480`、2bpp、`96000` bytes のペイロードを返します。
濃い線は黒に固定し、薄い地図線やアンチエイリアスは中間グレーとして残します。

## 含まれるもの

- `src/main.cpp` - Arduino/PlatformIO ファームウェア本体
- `lib/WaveshareEsp32Epaper/` - SPI e-paper ドライバーボード対応
- `include/app_config.h` - 秘密情報を含まない設定
- `include/app_secrets.example.h` - ローカル秘密情報用テンプレート
- `relay_server/` - 気象庁天気図リレーサーバー
- `cad/waveshare_7in5b_esp32_case.scad` - OpenSCAD ケースモデル
- `cad/*.stl` - 3Dプリント用STL
- `docs/architecture.md` - ファームウェアとリレーAPIの仕様
- `docs/case_design.md` - ケース寸法、エクスポート方法、印刷メモ

## 秘密情報

実際の Wi-Fi、リレー、MQTT の認証情報は `include/app_secrets.h` のみに置きます。
このファイルは Git の管理対象外で、公開リポジトリには含めません。

ローカル設定は次のように作成します。

```sh
cp include/app_secrets.example.h include/app_secrets.h
```

その後、`include/app_secrets.h` にローカル環境の値を入力してください。

## ファームウェアのビルド

PlatformIO をインストールした上で実行します。

```sh
PLATFORMIO_CORE_DIR=/tmp/pio-core pio run
```

縦縞パターンの診断用ファームウェア:

```sh
PLATFORMIO_CORE_DIR=/tmp/pio-core pio run -e waveshare_esp32_epd_diag
```

アップロード例:

```sh
PLATFORMIO_CORE_DIR=/tmp/pio-core pio run -e waveshare_esp32_epd -t upload --upload-port /dev/tty.usbmodemXXXX
```

退避用のビルドターゲット:

- `waveshare_esp32_epd_1bpp` - 新しい `800x480` パネルを従来の1bpp黒白ペイロードで使う
- `waveshare_esp32_epd_7in5bc` - 旧 `640x384` Waveshare 7.5inch e-Paper (B)、背面ラベル `X02R/171213-180502`

## リレーサーバーの起動

```sh
cd relay_server
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python3 weather_relay.py --bind 0.0.0.0 --port 8080
```

標準では新しいパネル向けに `800x480` `gray4` を返します。新しいパネルを
従来の1bpp黒白ペイロードで使う場合は次のように指定します。

```sh
python3 weather_relay.py --bind 0.0.0.0 --port 8080 --output-format 1bpp
```

旧 `640x384` パネル向けは次のように指定します。

```sh
python3 weather_relay.py --bind 0.0.0.0 --port 8080 --image-width 640 --image-height 384
```

## ケースファイル

OpenSCAD モデルから次の部品を出力できます。

```sh
openscad -D 'part="front"' -o cad/waveshare_7in5b_front.stl cad/waveshare_7in5b_esp32_case.scad
openscad -D 'part="back"' -o cad/waveshare_7in5b_back.stl cad/waveshare_7in5b_esp32_case.scad
openscad -D 'part="back_battery"' -o cad/waveshare_7in5b_back_battery.stl cad/waveshare_7in5b_esp32_case.scad
```

バッテリーバック版は、cheero Canvas 3200mAh IoT USB-C Ver. 相当のバッテリーを
横向きに収め、短い USB-A to USB-C ケーブルでドライバーボードへ給電する前提です。

## ハードウェアメモ

- Waveshare ESP32 ドライバーボードの e-paper ピンは固定です:
  `DIN=GPIO14`, `SCLK=GPIO13`, `CS=GPIO15`, `DC=GPIO27`, `RST=GPIO26`,
  `BUSY=GPIO25`
- ファームウェアは起床中だけ `GPIO2` 経由でドライバーボードの `LED1` を点灯します。
- 標準の新パネル経路では、Waveshare `epd7in5_V2` の4階調更新シーケンスを使います。
- 古い `7in5bc` パネルでは、ゴースト低減のため描画前に白クリアを行います。
- 生パネルは薄く破損しやすいため、FPC の向き、DIPスイッチ、ケースクリアランスを
  最終組み立て前に確認してください。
