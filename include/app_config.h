#pragma once

#include <Arduino.h>

namespace app {

constexpr char kDeviceName[]                  = "waveshare-esp32-weather-epaper";
constexpr char kFirmwareVersion[]             = "0.1.0";
constexpr char kTimezone[]                    = "JST-9";
constexpr uint32_t kWifiConnectTimeoutMs      = 20000;
constexpr uint32_t kHttpTimeoutMs             = 30000;
constexpr uint8_t kHttpRetryCount             = 3;
constexpr uint32_t kHttpRetryDelayMs          = 1000;
constexpr uint32_t kMqttSocketTimeoutSeconds  = 10;
constexpr bool kStatusLedEnabled              = true;
constexpr int8_t kStatusLedPin                = 2;  // Waveshare driver board LED1
constexpr bool kStatusLedActiveHigh           = true;
constexpr uint32_t kRetryDelaySeconds         = 5 * 60;
constexpr uint32_t kNetworkRetryDelaySeconds  = 10 * 60;
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
constexpr bool kMqttEnabled                   = true;

constexpr int kScheduleHours[] = {0, 6, 12, 18};
constexpr int kScheduleMinute = 30;

constexpr char kManifestPath[] = "/api/weather-map/latest.json";

}  // namespace app
