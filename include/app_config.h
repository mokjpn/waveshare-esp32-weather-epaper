#pragma once

#include <Arduino.h>

namespace app {

constexpr char kDeviceName[] =
#if defined(APP_WAVESHARE_ESP32_DRIVER)
    "waveshare-esp32-weather-epaper";
#else
    "m5capsule-weather-epaper";
#endif
constexpr char kFirmwareVersion[]             = "0.1.0";
constexpr char kTimezone[]                    = "JST-9";
constexpr uint32_t kWifiConnectTimeoutMs      = 20000;
constexpr uint32_t kHttpTimeoutMs             = 30000;
constexpr uint8_t kHttpRetryCount             = 3;
constexpr uint32_t kHttpRetryDelayMs          = 1000;
constexpr uint32_t kMqttSocketTimeoutSeconds  = 10;
constexpr bool kStatusLedEnabled =
#if defined(APP_WAVESHARE_ESP32_DRIVER)
    true;
#else
    false;
#endif
constexpr int8_t kStatusLedPin                = 2;  // Waveshare driver board LED1
constexpr bool kStatusLedActiveHigh           = true;
constexpr uint32_t kRetryDelaySeconds         = 5 * 60;
constexpr uint32_t kNfcCommandTimeoutMs       = 4300;
constexpr uint32_t kNfcCompletionTimeoutMs    = 18000;
constexpr uint32_t kNfcCompletionPollMs       = 100;
constexpr uint32_t kNfcPreRefreshDelayMs      = 1000;
constexpr uint32_t kNfcPreTransferDelayMs     = 2500;
constexpr uint32_t kNfcDetectTimeoutMs =
#if defined(APP_NFC_DIAGNOSTIC)
    180;
#else
    2500;
#endif
constexpr uint32_t kNfcFieldWarmupMs          = 8000;
constexpr uint32_t kNfcFailureCooldownMs      = 15000;
constexpr uint32_t kNfcChunkDelayMs           = 25;
constexpr uint16_t kNfcChunkPauseEvery        = 20;
constexpr uint32_t kNfcChunkPauseMs           = 1200;
constexpr uint32_t kEpdBusyTimeoutMs          = 120000;
constexpr bool kEpdClearBeforeUpdate =
#if defined(APP_EPD_7IN5BC)
    true;
#else
    false;
#endif
constexpr bool kEpdAggressiveClear =
#if defined(APP_EPD_7IN5BC)
    true;
#else
    false;
#endif
constexpr uint16_t kImageWidth =
#if defined(APP_EPD_7IN5BC)
    640;
#else
    800;
#endif
constexpr uint16_t kImageHeight =
#if defined(APP_EPD_7IN5BC)
    384;
#else
    480;
#endif
constexpr uint32_t kPackedImageBytes          = (kImageWidth * kImageHeight) / 8;
constexpr uint32_t kImagePayloadBytes =
#if defined(APP_EPD_7IN5BC)
    kPackedImageBytes * 2;
#else
    kPackedImageBytes;
#endif
constexpr uint8_t kPanelCodePrimary           = 12;  // EPD_7IN5 (v1)
constexpr uint8_t kPanelCodeFallback          = 14;  // EPD_7IN5V2
constexpr bool kRetryFallbackAfterTransferStart = false;
constexpr uint8_t kChunkBytes                 = 60;
constexpr uint16_t kChunkCount                = 800;
constexpr uint8_t kNfcChunkRetryCount         = 5;
constexpr uint8_t kNfcI2cAddress              = 0x50;
constexpr bool kNfcUse5V                      = true;
constexpr uint8_t kNfcTxAmModulation          = 15;
constexpr bool kMqttEnabled                   = true;

constexpr int kScheduleHours[] = {0, 6, 12, 18};

enum class TransportMode : uint8_t {
    NFCA,
    NFCV,
};

constexpr TransportMode kPreferredTransportMode = TransportMode::NFCA;
constexpr bool kTryAlternateTransportMode       = true;

constexpr char kManifestPath[] = "/api/weather-map/latest.json";

}  // namespace app
