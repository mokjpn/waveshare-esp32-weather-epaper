# Waveshare ESP32 Weather E-Paper

Firmware, relay server, and 3D-printable case files for a battery-powered
weather-map e-paper display.

This repository targets:

- Waveshare e-Paper ESP32 Driver Board
- Older Waveshare 7.5inch e-Paper (B) raw panel, rear label
  `X02R/171213-180502`
- Japan Meteorological Agency weather map source data
- A small Python relay server that prepares panel-ready packed bitmap data

## What It Does

1. Wakes on schedule.
2. Connects to Wi-Fi.
3. Synchronizes time with NTP.
4. Fetches a processed weather-map manifest and binary image from the relay.
5. Sends black and red planes to the e-paper panel over SPI.
6. Publishes status events to MQTT.
7. Turns off Wi-Fi/Bluetooth and enters deep sleep.

The active firmware environment is `waveshare_esp32_epd_7in5bc`, which uses the
older `7in5bc` `640x384` panel path. The relay defaults to two packed `1bpp`
planes: black first, red second. The red plane is used for the source timestamp
label.

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
PLATFORMIO_CORE_DIR=/tmp/pio-core pio run -e waveshare_esp32_epd_7in5bc
```

Diagnostic stripe-pattern firmware:

```sh
PLATFORMIO_CORE_DIR=/tmp/pio-core pio run -e waveshare_esp32_epd_7in5bc_diag
```

Upload example:

```sh
PLATFORMIO_CORE_DIR=/tmp/pio-core pio run -e waveshare_esp32_epd_7in5bc -t upload --upload-port /dev/tty.usbmodemXXXX
```

## Run Relay Server

```sh
cd relay_server
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python3 weather_relay.py --bind 0.0.0.0 --port 8080
```

The relay defaults to `640x384` for the older panel. To serve a newer
`800x480` panel instead:

```sh
python3 weather_relay.py --bind 0.0.0.0 --port 8080 --image-width 800 --image-height 480
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
- 古い Waveshare 7.5inch e-Paper (B) 生パネル、背面ラベル
  `X02R/171213-180502`
- 気象庁の天気図データ
- パネルにそのまま送れるビットマップへ変換する Python リレーサーバー

## 機能

1. スケジュールに従って起床します。
2. Wi-Fi に接続します。
3. NTP で時刻同期します。
4. リレーサーバーから処理済み天気図の manifest とバイナリ画像を取得します。
5. SPI 経由で e-paper パネルへ黒・赤の2プレーンを書き込みます。
6. MQTT に状態イベントを送信します。
7. Wi-Fi/Bluetooth を停止して deep sleep に入ります。

通常使用する PlatformIO 環境は `waveshare_esp32_epd_7in5bc` です。古い
`7in5bc` 系パネル向けに `640x384` として扱います。リレーサーバーは標準で
黒プレーン、赤プレーンの順に `1bpp` パック済みデータを返し、赤プレーンには
元画像の時刻ラベルを描画します。

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
PLATFORMIO_CORE_DIR=/tmp/pio-core pio run -e waveshare_esp32_epd_7in5bc
```

縦縞パターンの診断用ファームウェア:

```sh
PLATFORMIO_CORE_DIR=/tmp/pio-core pio run -e waveshare_esp32_epd_7in5bc_diag
```

アップロード例:

```sh
PLATFORMIO_CORE_DIR=/tmp/pio-core pio run -e waveshare_esp32_epd_7in5bc -t upload --upload-port /dev/tty.usbmodemXXXX
```

## リレーサーバーの起動

```sh
cd relay_server
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python3 weather_relay.py --bind 0.0.0.0 --port 8080
```

標準では古いパネル向けに `640x384` を返します。新しい `800x480` パネル向けに
する場合は次のように指定します。

```sh
python3 weather_relay.py --bind 0.0.0.0 --port 8080 --image-width 800 --image-height 480
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
- 古い `7in5bc` パネルでは、ゴースト低減のため描画前に白クリアを行います。
- 生パネルは薄く破損しやすいため、FPC の向き、DIPスイッチ、ケースクリアランスを
  最終組み立て前に確認してください。
