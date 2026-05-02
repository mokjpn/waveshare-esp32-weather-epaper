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

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
WaveshareEsp32Epaper epaper;

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

bool connectWifi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(APP_WIFI_SSID, APP_WIFI_PASSWORD);
    const auto start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < app::kWifiConnectTimeoutMs) {
        delay(250);
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[wifi] connect failed");
        return false;
    }
    Serial.printf("[wifi] connected ip=%s\n", WiFi.localIP().toString().c_str());
    return true;
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
    const bool acceptedLength =
        knownLength &&
        (static_cast<uint32_t>(length) == app::kPackedImageBytes ||
         static_cast<uint32_t>(length) == app::kImagePayloadBytes);
    if (knownLength && !acceptedLength) {
        Serial.printf("[http] image size mismatch %d\n", length);
        http.end();
        return false;
    }

    const size_t expectedSize = knownLength ? static_cast<size_t>(length) : app::kImagePayloadBytes;
    image.assign(expectedSize, 0xFF);
    auto* stream = http.getStreamPtr();
    size_t offset{};
    while (http.connected() && offset < image.size()) {
        const auto available = stream->available();
        if (!available) {
            delay(1);
            continue;
        }
        const auto chunk = std::min<size_t>(available, image.size() - offset);
        const auto read  = stream->readBytes(reinterpret_cast<char*>(image.data() + offset), chunk);
        offset += read;
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
        candidate.tm_min    = 0;
        candidate.tm_sec    = 0;
        auto epoch          = mktime(&candidate);
        if (epoch > nowEpoch + 30) {
            return epoch;
        }
    }

    struct tm nextDay = nowTm;
    nextDay.tm_mday += 1;
    nextDay.tm_hour = app::kScheduleHours[0];
    nextDay.tm_min  = 0;
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

bool beginDevice()
{
    beginStatusLed();
    return true;
}

#if defined(APP_EPD_DIAGNOSTIC)
std::vector<uint8_t> makeDiagnosticImage()
{
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

bool performUpdate()
{
    Manifest manifest;
    if (!fetchManifest(manifest)) {
        return false;
    }

    StaticJsonDocument<512> meta;
    meta["source_url"]          = manifest.sourceUrl;
    meta["source_published_at"] = manifest.sourcePublishedAt;
    meta["etag"]                = manifest.etag;
    publishEvent("manifest", "ok", "manifest fetched", &meta);

    std::vector<uint8_t> image;
    if (!fetchPackedImage(manifest, image)) {
        return false;
    }

    auto result = epaper.writeImage(image);
    StaticJsonDocument<512> updateInfo;
    updateInfo["source_url"] = manifest.sourceUrl;
    updateInfo["published"]  = manifest.sourcePublishedAt;
    updateInfo["transport"]  = "SPI";
    updateInfo["panel"]      = "7.5inch e-Paper (B) V2";

    if (!result.ok) {
        publishEvent("epaper_update", "error", result.message, &updateInfo);
        Serial.printf("[epd] update failed: %s\n", result.message.c_str());
        return false;
    }

    publishEvent("epaper_update", "ok", result.message, &updateInfo);
    Serial.println("[epd] update complete");
    beep(3000, 120);
    return true;
}

}  // namespace

void setup()
{
    Serial.begin(115200);
    delay(300);
    beginDevice();
    setStatusLed(true);

    Serial.printf("\n[%s] boot firmware=%s\n", app::kDeviceName, app::kFirmwareVersion);
    beep(2200, 40);

#if defined(APP_EPD_DIAGNOSTIC)
    runEpdDiagnostic();
#endif

    if (!connectWifi()) {
        retryLater("wifi connect failed");
    }
    connectMqtt();
    if (!syncTime()) {
        retryLater("time sync failed");
    }

    if (!performUpdate()) {
        retryLater("update flow failed");
    }

    publishEvent("sleep", "ok", "sleep until next schedule");
    sleepUntilNextSchedule();
}

void loop()
{
    mqttClient.loop();
    delay(10);
}
