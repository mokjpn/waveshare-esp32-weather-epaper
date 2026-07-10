#pragma once

#include <Arduino.h>

namespace app {

constexpr char kDeviceName[]                  = "waveshare-esp32-weather-epaper";
constexpr char kFirmwareVersion[]             = "0.1.0";
constexpr char kTimezone[]                    = "JST-9";
constexpr uint32_t kWifiConnectTimeoutMs      = 20000;
constexpr uint8_t kWifiConnectRetryCount      = 3;
constexpr uint32_t kWifiRetryIntervalMs       = 2000;
constexpr uint32_t kWifiRadioResetDelayMs     = 300;
constexpr uint32_t kHttpTimeoutMs             = 30000;
constexpr uint32_t kHttpReceiveIdleTimeoutMs  = 30000;
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
constexpr bool kEpdGray4ClearBeforeUpdate =
#if defined(APP_EPD_7IN5BC) || defined(APP_EPD_1BPP)
    false;
#else
    true;
#endif
constexpr bool kEpdFourGray =
#if defined(APP_EPD_7IN5BC) || defined(APP_EPD_1BPP)
    false;
#else
    true;
#endif
constexpr char kPanelName[] =
#if defined(APP_EPD_7IN5BC)
    "Waveshare 7.5inch e-Paper (B) legacy 640x384";
#elif defined(APP_EPD_1BPP)
    "Waveshare 7.5inch e-Paper V2 black/white 800x480 1bpp";
#else
    "Waveshare 7.5inch e-Paper V2 black/white 800x480 4-gray";
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
constexpr uint32_t kGray4ImageBytes           = (kImageWidth * kImageHeight) / 4;
constexpr uint32_t kImagePayloadBytes =
#if defined(APP_EPD_7IN5BC)
    kPackedImageBytes * 2;
#elif defined(APP_EPD_1BPP)
    kPackedImageBytes;
#else
    kGray4ImageBytes;
#endif
constexpr bool kMqttEnabled                   = true;

constexpr int kScheduleHours[] = {0, 3, 6, 9, 12, 15, 18, 21};
constexpr int kScheduleMinute = 0;

constexpr char kManifestPath[] = "/api/weather-map/latest.json";

}  // namespace app
