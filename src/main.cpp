#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <vector>
#include <time.h>
#include "app_config.h"
#if __has_include("app_secrets.h")
#include "app_secrets.h"
#else
#include "app_secrets.example.h"
#endif
#include "WaveshareEsp32Epaper.hpp"

namespace {

struct Manifest {
    String imagePath{};
    String sourceUrl{};
    String sourcePublishedAt{};
    String etag{};
    uint16_t width{};
    uint16_t height{};
};

enum class UpdateResult : uint8_t {
    Ok,
    RelayFetchFailed,
    EpaperUpdateFailed,
};

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
WaveshareEsp32Epaper epaper;

struct WifiFailureBacklog {
    uint32_t magic{};
    uint32_t failureCount{};
    int32_t lastStatus{-1};
    time_t lastFailureEpoch{};
    uint32_t lastFailureMillis{};
};

constexpr uint32_t kWifiFailureBacklogMagic = 0x57494642UL;
RTC_DATA_ATTR WifiFailureBacklog gWifiFailureBacklog;

void ensureWifiFailureBacklogInitialized()
{
    if (gWifiFailureBacklog.magic == kWifiFailureBacklogMagic) {
        return;
    }
    gWifiFailureBacklog.magic = kWifiFailureBacklogMagic;
    gWifiFailureBacklog.failureCount = 0;
    gWifiFailureBacklog.lastStatus = -1;
    gWifiFailureBacklog.lastFailureEpoch = 0;
    gWifiFailureBacklog.lastFailureMillis = 0;
}

String formatEventTimeString(time_t epoch, uint32_t millisFallback)
{
    if (epoch > 100000) {
        struct tm utcTm {};
        gmtime_r(&epoch, &utcTm);
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utcTm);
        return String(buffer);
    }

    const auto fallback = millisFallback > 0 ? millisFallback : millis();
    return String("unsynced-millis-") + String(fallback);
}

String nowEventTimeString()
{
    return formatEventTimeString(time(nullptr), millis());
}

void recordWifiFailure(int32_t wifiStatus)
{
    ensureWifiFailureBacklogInitialized();
    gWifiFailureBacklog.failureCount += 1;
    gWifiFailureBacklog.lastStatus = wifiStatus;
    gWifiFailureBacklog.lastFailureEpoch = time(nullptr);
    gWifiFailureBacklog.lastFailureMillis = millis();
}

void beep(uint16_t freq, uint32_t duration)
{
    (void)freq;
    (void)duration;
}

void setStatusLed(bool on)
{
    if (!app::kStatusLedEnabled || app::kStatusLedPin < 0) {
        return;
    }
    const auto level = on == app::kStatusLedActiveHigh ? HIGH : LOW;
    digitalWrite(app::kStatusLedPin, level);
}

void beginStatusLed()
{
    if (!app::kStatusLedEnabled || app::kStatusLedPin < 0) {
        return;
    }
    pinMode(app::kStatusLedPin, OUTPUT);
    setStatusLed(false);
}

String joinUrl(const char* base, const String& path)
{
    if (path.startsWith("http://") || path.startsWith("https://")) {
        return path;
    }
    String url(base);
    if (url.endsWith("/") && path.startsWith("/")) {
        url.remove(url.length() - 1);
    } else if (!url.endsWith("/") && !path.startsWith("/")) {
        url += '/';
    }
    url += path;
    return url;
}

void publishEvent(const char* event, const char* status, const String& message, JsonDocument* extra = nullptr)
{
    if (!app::kMqttEnabled) {
        return;
    }
    if (!mqttClient.connected()) {
        return;
    }

    StaticJsonDocument<1024> doc;
    doc["device"]   = app::kDeviceName;
    doc["firmware"] = app::kFirmwareVersion;
    doc["event"]    = event;
    doc["status"]   = status;
    doc["message"]  = message;
    doc["event_time"] = nowEventTimeString();
    doc["heap"]     = ESP.getFreeHeap();
    doc["millis"]   = millis();
    if (extra) {
        JsonObject nested = doc["data"].to<JsonObject>();
        for (JsonPairConst kv : extra->as<JsonObjectConst>()) {
            nested[kv.key()] = kv.value();
        }
    }

    char payload[1024];
    const auto len = serializeJson(doc, payload, sizeof(payload));
    mqttClient.publish(APP_MQTT_TOPIC_BASE "/events", reinterpret_cast<const uint8_t*>(payload), len);
}

void publishWifiFailureBacklogIfAny()
{
    ensureWifiFailureBacklogInitialized();
    if (!app::kMqttEnabled || !mqttClient.connected()) {
        return;
    }
    if (gWifiFailureBacklog.failureCount == 0) {
        return;
    }

    StaticJsonDocument<384> extra;
    extra["count"] = gWifiFailureBacklog.failureCount;
    extra["last_status"] = gWifiFailureBacklog.lastStatus;
    extra["last_failed_at"] =
        formatEventTimeString(gWifiFailureBacklog.lastFailureEpoch, gWifiFailureBacklog.lastFailureMillis);
    extra["persisted"] = "rtc";
    publishEvent("wifi_failure_backlog", "warn", "deferred wifi failures observed", &extra);

    gWifiFailureBacklog.failureCount = 0;
    gWifiFailureBacklog.lastStatus = -1;
    gWifiFailureBacklog.lastFailureEpoch = 0;
    gWifiFailureBacklog.lastFailureMillis = 0;
}

bool connectWifi()
{
    WiFi.persistent(false);
    for (uint8_t attempt = 1; attempt <= app::kWifiConnectRetryCount; ++attempt) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(app::kWifiRadioResetDelayMs);

        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        delay(app::kWifiRadioResetDelayMs);
        WiFi.begin(APP_WIFI_SSID, APP_WIFI_PASSWORD);
        Serial.printf("[wifi] connect attempt=%u/%u\n", attempt, app::kWifiConnectRetryCount);

        const auto start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < app::kWifiConnectTimeoutMs) {
            delay(250);
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[wifi] connected ip=%s rssi=%d sleep=off\n", WiFi.localIP().toString().c_str(),
                          WiFi.RSSI());
            return true;
        }

        Serial.printf("[wifi] attempt=%u failed status=%d\n", attempt, static_cast<int>(WiFi.status()));
        if (attempt < app::kWifiConnectRetryCount) {
            delay(app::kWifiRetryIntervalMs);
        }
    }

    recordWifiFailure(static_cast<int32_t>(WiFi.status()));
    Serial.println("[wifi] connect failed after retries");
    return false;
}

bool connectMqtt()
{
    if (!app::kMqttEnabled) {
        return true;
    }

    mqttClient.setServer(APP_MQTT_HOST, APP_MQTT_PORT);
    mqttClient.setSocketTimeout(app::kMqttSocketTimeoutSeconds);

    String clientId = String(app::kDeviceName) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    bool ok{};
    if (strlen(APP_MQTT_USERNAME) > 0) {
        ok = mqttClient.connect(clientId.c_str(), APP_MQTT_USERNAME, APP_MQTT_PASSWORD);
    } else {
        ok = mqttClient.connect(clientId.c_str());
    }
    if (!ok) {
        Serial.printf("[mqtt] connect failed rc=%d\n", mqttClient.state());
        return false;
    }
    publishEvent("boot", "ok", "mqtt connected");
    return true;
}

bool syncTime()
{
    configTzTime(app::kTimezone, "ntp.nict.jp", "pool.ntp.org", "time.google.com");
    struct tm timeinfo {};
    for (int i = 0; i < 40; ++i) {
        if (getLocalTime(&timeinfo, 250)) {
            Serial.printf("[time] synced %04d-%02d-%02d %02d:%02d:%02d\n", timeinfo.tm_year + 1900,
                          timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            return true;
        }
    }
    Serial.println("[time] sync failed");
    return false;
}

bool fetchManifest(Manifest& manifest)
{
    const auto url = joinUrl(APP_RELAY_BASE_URL, app::kManifestPath);
    Serial.printf("[http] manifest url=%s\n", url.c_str());

    HTTPClient http;
    int code = 0;
    for (uint8_t attempt = 1; attempt <= app::kHttpRetryCount; ++attempt) {
        http.setTimeout(app::kHttpTimeoutMs);
        http.setReuse(false);
        http.useHTTP10(true);
        if (!http.begin(url)) {
            Serial.println("[http] manifest begin failed");
            return false;
        }
        code = http.GET();
        if (code == HTTP_CODE_OK) {
            break;
        }
        Serial.printf("[http] manifest attempt=%u code=%d error=%s\n", attempt, code,
                      HTTPClient::errorToString(code).c_str());
        http.end();
        delay(app::kHttpRetryDelayMs);
    }
    if (code != HTTP_CODE_OK) {
        return false;
    }

    StaticJsonDocument<1024> doc;
    const auto error = deserializeJson(doc, http.getStream());
    http.end();
    if (error) {
        Serial.printf("[http] manifest parse failed: %s\n", error.c_str());
        return false;
    }

    manifest.imagePath         = doc["image_path"] | "";
    manifest.sourceUrl         = doc["source_url"] | "";
    manifest.sourcePublishedAt = doc["source_published_at"] | "";
    manifest.etag              = doc["etag"] | "";
    manifest.width             = doc["width"] | 0;
    manifest.height            = doc["height"] | 0;

    if (manifest.imagePath.isEmpty()) {
        Serial.println("[http] manifest missing image_path");
        return false;
    }
    if (manifest.width != app::kImageWidth || manifest.height != app::kImageHeight) {
        Serial.printf("[http] unexpected dimensions %u x %u\n", manifest.width, manifest.height);
        return false;
    }
    return true;
}

bool fetchPackedImage(const Manifest& manifest, std::vector<uint8_t>& image)
{
    const auto url = joinUrl(APP_RELAY_BASE_URL, manifest.imagePath);
    Serial.printf("[http] image url=%s\n", url.c_str());

    HTTPClient http;
    int code = 0;
    for (uint8_t attempt = 1; attempt <= app::kHttpRetryCount; ++attempt) {
        http.setTimeout(app::kHttpTimeoutMs);
        http.setReuse(false);
        http.useHTTP10(true);
        if (!http.begin(url)) {
            Serial.println("[http] image begin failed");
            return false;
        }
        code = http.GET();
        if (code == HTTP_CODE_OK) {
            break;
        }
        Serial.printf("[http] image attempt=%u code=%d error=%s\n", attempt, code,
                      HTTPClient::errorToString(code).c_str());
        http.end();
        delay(app::kHttpRetryDelayMs);
    }
    if (code != HTTP_CODE_OK) {
        return false;
    }

    const int length = http.getSize();
    const bool knownLength = length > 0;
    bool acceptedLength = false;
    if (knownLength) {
        const auto imageBytes = static_cast<uint32_t>(length);
        if (app::kEpdFourGray) {
            acceptedLength = imageBytes == app::kImagePayloadBytes;
        } else {
            acceptedLength = imageBytes == app::kPackedImageBytes || imageBytes == app::kImagePayloadBytes;
        }
    }
    if (knownLength && !acceptedLength) {
        Serial.printf("[http] image size mismatch %d\n", length);
        http.end();
        return false;
    }

    const size_t expectedSize = knownLength ? static_cast<size_t>(length) : app::kImagePayloadBytes;
    image.assign(expectedSize, 0xFF);
    auto* stream = http.getStreamPtr();
    size_t offset{};
    uint32_t lastProgressAt = millis();
    while (offset < image.size()) {
        const auto available = stream->available();
        if (available) {
            const auto chunk = std::min<size_t>(available, image.size() - offset);
            const auto read  = stream->readBytes(reinterpret_cast<char*>(image.data() + offset), chunk);
            if (read > 0) {
                offset += read;
                lastProgressAt = millis();
                continue;
            }
        }

        if (!http.connected()) {
            break;
        }
        if (millis() - lastProgressAt >= app::kHttpReceiveIdleTimeoutMs) {
            Serial.printf("[http] image receive idle timeout at %u / %u bytes\n",
                          static_cast<unsigned>(offset), static_cast<unsigned>(image.size()));
            break;
        }
        delay(10);
    }
    http.end();
    if (offset != image.size()) {
        Serial.printf("[http] short image read %u / %u\n", static_cast<unsigned>(offset),
                      static_cast<unsigned>(image.size()));
        return false;
    }
    return true;
}

time_t computeNextSchedule(time_t nowEpoch)
{
    struct tm nowTm {};
    localtime_r(&nowEpoch, &nowTm);

    for (auto hour : app::kScheduleHours) {
        struct tm candidate = nowTm;
        candidate.tm_hour   = hour;
        candidate.tm_min    = app::kScheduleMinute;
        candidate.tm_sec    = 0;
        auto epoch          = mktime(&candidate);
        if (epoch > nowEpoch + 30) {
            return epoch;
        }
    }

    struct tm nextDay = nowTm;
    nextDay.tm_mday += 1;
    nextDay.tm_hour = app::kScheduleHours[0];
    nextDay.tm_min  = app::kScheduleMinute;
    nextDay.tm_sec  = 0;
    return mktime(&nextDay);
}

[[noreturn]] void goSleepSeconds(uint32_t seconds)
{
    Serial.printf("[sleep] %u seconds\n", static_cast<unsigned>(seconds));
    setStatusLed(false);
    delay(100);
    mqttClient.disconnect();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    btStop();
    Serial.flush();
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
    esp_deep_sleep_start();
    while (true) {
        delay(1000);
    }
}

[[noreturn]] void sleepUntilNextSchedule()
{
    const auto now = time(nullptr);
    if (now <= 100000) {
        goSleepSeconds(app::kRetryDelaySeconds);
    }
    const auto next = computeNextSchedule(now);
    const auto diff = (next > now) ? static_cast<uint32_t>(next - now) : app::kRetryDelaySeconds;
    goSleepSeconds(diff);
}

[[noreturn]] void retryLater(const String& reason)
{
    Serial.printf("[retry] %s\n", reason.c_str());
    publishEvent("retry", "error", reason);
    goSleepSeconds(app::kRetryDelaySeconds);
}

[[noreturn]] void retryLater(const String& reason, uint32_t seconds)
{
    Serial.printf("[retry] %s\n", reason.c_str());
    publishEvent("retry", "error", reason);
    goSleepSeconds(seconds);
}

bool beginDevice()
{
    beginStatusLed();
    return true;
}

#if defined(APP_EPD_DIAGNOSTIC)
std::vector<uint8_t> makeDiagnosticImage()
{
    if (app::kEpdFourGray) {
        std::vector<uint8_t> image(app::kGray4ImageBytes, 0xFF);
        constexpr uint16_t bytesPerRow = app::kImageWidth / 4;
        for (uint16_t y = 0; y < app::kImageHeight; ++y) {
            for (uint16_t xByte = 0; xByte < bytesPerRow; ++xByte) {
                const uint16_t x = xByte * 4;
                const uint8_t shade = (x / 80) % 4;
                uint8_t pixel = 0xC0;
                if (shade == 0) {
                    pixel = 0x00;
                } else if (shade == 1) {
                    pixel = 0x40;
                } else if (shade == 2) {
                    pixel = 0x80;
                }
                image[y * bytesPerRow + xByte] =
                    pixel | (pixel >> 2) | (pixel >> 4) | (pixel >> 6);
            }
        }
        return image;
    }

    std::vector<uint8_t> image(app::kPackedImageBytes, 0xFF);
    constexpr uint16_t bytesPerRow = app::kImageWidth / 8;
    for (uint16_t y = 0; y < app::kImageHeight; ++y) {
        for (uint16_t xByte = 0; xByte < bytesPerRow; ++xByte) {
            const uint16_t x = xByte * 8;
            const bool black = ((x / 80) % 2) == 0;
            image[y * bytesPerRow + xByte] = black ? 0x00 : 0xFF;
        }
    }
    return image;
}

[[noreturn]] void runEpdDiagnostic()
{
    Serial.println("[diag] EPD diagnostic pattern: vertical black/white stripes");
    auto image = makeDiagnosticImage();
    auto result = epaper.writeImage(image);
    Serial.printf("[diag] result ok=%d message=%s\n", result.ok ? 1 : 0, result.message.c_str());
    while (true) {
        delay(1000);
    }
}
#endif

UpdateResult performUpdate()
{
    Manifest manifest;
    if (!fetchManifest(manifest)) {
        return UpdateResult::RelayFetchFailed;
    }

    StaticJsonDocument<512> meta;
    meta["source_url"]          = manifest.sourceUrl;
    meta["source_published_at"] = manifest.sourcePublishedAt;
    meta["etag"]                = manifest.etag;
    publishEvent("manifest", "ok", "manifest fetched", &meta);

    std::vector<uint8_t> image;
    if (!fetchPackedImage(manifest, image)) {
        return UpdateResult::RelayFetchFailed;
    }

    auto result = epaper.writeImage(image);
    StaticJsonDocument<512> updateInfo;
    updateInfo["source_url"] = manifest.sourceUrl;
    updateInfo["published"]  = manifest.sourcePublishedAt;
    updateInfo["transport"]  = "SPI";
    updateInfo["panel"]      = app::kPanelName;

    if (!result.ok) {
        publishEvent("epaper_update", "error", result.message, &updateInfo);
        Serial.printf("[epd] update failed: %s\n", result.message.c_str());
        return UpdateResult::EpaperUpdateFailed;
    }

    publishEvent("epaper_update", "ok", result.message, &updateInfo);
    Serial.println("[epd] update complete");
    beep(3000, 120);
    return UpdateResult::Ok;
}

}  // namespace

void setup()
{
    Serial.begin(115200);
    delay(300);
    ensureWifiFailureBacklogInitialized();
    beginDevice();
    setStatusLed(true);

    Serial.printf("\n[%s] boot firmware=%s\n", app::kDeviceName, app::kFirmwareVersion);
    beep(2200, 40);

#if defined(APP_EPD_DIAGNOSTIC)
    runEpdDiagnostic();
#endif

    if (!connectWifi()) {
        retryLater("wifi connect failed", app::kNetworkRetryDelaySeconds);
    }
    connectMqtt();
    publishWifiFailureBacklogIfAny();
    if (!syncTime()) {
        retryLater("time sync failed");
    }

    const auto updateResult = performUpdate();
    if (updateResult == UpdateResult::RelayFetchFailed) {
        retryLater("relay image fetch failed", app::kNetworkRetryDelaySeconds);
    }
    if (updateResult == UpdateResult::EpaperUpdateFailed) {
        retryLater("epaper update failed");
    }

    publishEvent("sleep", "ok", "sleep until next schedule");
    sleepUntilNextSchedule();
}

void loop()
{
    mqttClient.loop();
    delay(10);
}
