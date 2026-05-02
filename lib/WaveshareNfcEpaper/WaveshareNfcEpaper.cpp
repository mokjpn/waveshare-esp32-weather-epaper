#include "WaveshareNfcEpaper.hpp"

namespace {

constexpr uint8_t kOk0        = 0x00;
constexpr uint8_t kOk1        = 0x00;
constexpr uint8_t kDone0      = 0xFF;
constexpr uint8_t kDone1      = 0x00;
constexpr uint8_t kStep0[]    = {0xCD, 0x0D};
constexpr uint8_t kStep2[]    = {0xCD, 0x01};
constexpr uint8_t kStep3[]    = {0xCD, 0x02};
constexpr uint8_t kStep4[]    = {0xCD, 0x03};
constexpr uint8_t kStep5[]    = {0xCD, 0x05};
constexpr uint8_t kStep6[]    = {0xCD, 0x06};
constexpr uint8_t kStep7[]    = {0xCD, 0x07};
constexpr uint8_t kStep9[]    = {0xCD, 0x18};
constexpr uint8_t kStep10[]   = {0xCD, 0x09};
constexpr uint8_t kStep11[]   = {0xCD, 0x0A};
constexpr uint8_t kStep12[]   = {0xCD, 0x04};

}  // namespace

bool WaveshareNfcEpaper::exchange(const uint8_t* tx, uint16_t tx_len, uint8_t expected0, uint8_t expected1, String& error)
{
    uint8_t rx[32]  = {};
    uint16_t rx_len = sizeof(rx);
    if (!transport_.transceive(tx, tx_len, rx, rx_len, app::kNfcCommandTimeoutMs)) {
        error = "transceive failed";
        return false;
    }
    if (rx_len < 2) {
        error = "short response";
        return false;
    }
    if (rx[0] != expected0 || rx[1] != expected1) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "unexpected response %02X %02X", rx[0], rx[1]);
        error = buffer;
        return false;
    }
    return true;
}

bool WaveshareNfcEpaper::waitUntilRefreshDone(String& error)
{
    const uint32_t start = millis();
    while (millis() - start < app::kNfcCompletionTimeoutMs) {
        uint8_t rx[32]  = {};
        uint16_t rx_len = sizeof(rx);
        if (!transport_.transceive(kStep11, sizeof(kStep11), rx, rx_len, app::kNfcCommandTimeoutMs)) {
            delay(app::kNfcCompletionPollMs);
            continue;
        }
        if (rx_len >= 2 && rx[0] == kDone0 && rx[1] == kDone1) {
            return true;
        }
        delay(app::kNfcCompletionPollMs);
    }
    error = "refresh timeout";
    return false;
}

WaveshareNfcEpaper::Result WaveshareNfcEpaper::tryPanelCode(uint8_t panelCode, const std::vector<uint8_t>& image)
{
    Result result{};
    result.panelCodeTried = panelCode;

    String error;
    uint8_t step1[] = {0xCD, 0x00, panelCode};
    uint8_t chunk[3 + app::kChunkBytes] = {0xCD, 0x08, app::kChunkBytes};

    if (!exchange(kStep0, sizeof(kStep0), kOk0, kOk1, error)) {
        result.message = "step0: " + error;
        return result;
    }
    if (!exchange(step1, sizeof(step1), kOk0, kOk1, error)) {
        result.message = "step1: " + error;
        return result;
    }
    if (!exchange(kStep2, sizeof(kStep2), kOk0, kOk1, error)) {
        result.message = "step2: " + error;
        return result;
    }
    if (!exchange(kStep3, sizeof(kStep3), kOk0, kOk1, error)) {
        result.message = "step3: " + error;
        return result;
    }
    if (!exchange(kStep4, sizeof(kStep4), kOk0, kOk1, error)) {
        result.message = "step4: " + error;
        return result;
    }
    if (!exchange(kStep5, sizeof(kStep5), kOk0, kOk1, error)) {
        result.message = "step5: " + error;
        return result;
    }
    if (!exchange(kStep6, sizeof(kStep6), kOk0, kOk1, error)) {
        result.message = "step6: " + error;
        return result;
    }
    if (!exchange(kStep7, sizeof(kStep7), kOk0, kOk1, error)) {
        result.message = "step7: " + error;
        return result;
    }
    Serial.printf("[epd] pre-transfer pause %u ms\n", static_cast<unsigned>(app::kNfcPreTransferDelayMs));
    delay(app::kNfcPreTransferDelayMs);

    for (uint16_t i = 0; i < app::kChunkCount; ++i) {
        if (i > 0 && app::kNfcChunkPauseEvery > 0 && (i % app::kNfcChunkPauseEvery) == 0) {
            Serial.printf("[epd] transfer pause at chunk %u ms=%u\n", i, static_cast<unsigned>(app::kNfcChunkPauseMs));
            delay(app::kNfcChunkPauseMs);
        }
        memcpy(&chunk[3], &image[i * app::kChunkBytes], app::kChunkBytes);
        bool chunkOk = false;
        String chunkError;
        for (uint8_t attempt = 0; attempt <= app::kNfcChunkRetryCount; ++attempt) {
            uint8_t rx[32]  = {};
            uint16_t rx_len = sizeof(rx);
            if (!transport_.transceive(chunk, sizeof(chunk), rx, rx_len, app::kNfcCommandTimeoutMs)) {
                chunkError = "transceive failed";
            } else if (rx_len >= 2 && (rx[0] != kOk0 || rx[1] != kOk1)) {
                char buffer[64];
                snprintf(buffer, sizeof(buffer), "response %02X %02X", rx[0], rx[1]);
                chunkError = buffer;
            } else {
                chunkOk = true;
                if (attempt > 0) {
                    Serial.printf("[epd] chunk %u recovered after retry %u\n", i, attempt);
                }
                break;
            }
            Serial.printf("[epd] chunk %u attempt %u failed: %s\n", i, attempt, chunkError.c_str());
            delay(app::kNfcChunkDelayMs * (attempt + 2));
        }
        if (!chunkOk) {
            result.message = "chunk failed at " + String(i) + ": " + chunkError;
            return result;
        }
        if ((i % 20) == 0) {
            Serial.printf("[epd] progress %u/%u\n", i, app::kChunkCount);
        }
        delay(app::kNfcChunkDelayMs);
    }

    if (!exchange(kStep9, sizeof(kStep9), kOk0, kOk1, error)) {
        result.message = "step9: " + error;
        return result;
    }
    delay(app::kNfcPreRefreshDelayMs);
    if (!exchange(kStep10, sizeof(kStep10), kOk0, kOk1, error)) {
        result.message = "step10: " + error;
        return result;
    }
    if (!waitUntilRefreshDone(error)) {
        result.message = "step11: " + error;
        return result;
    }
    if (!exchange(kStep12, sizeof(kStep12), kOk0, kOk1, error)) {
        result.message = "step12: " + error;
        return result;
    }

    result.ok      = true;
    result.message = "update complete";
    return result;
}

WaveshareNfcEpaper::Result WaveshareNfcEpaper::writeImage(const std::vector<uint8_t>& image)
{
    if (image.size() != app::kPackedImageBytes) {
        Result result{};
        result.message = "invalid image size";
        return result;
    }

    auto result = tryPanelCode(app::kPanelCodePrimary, image);
    if (result.ok) {
        return result;
    }
    const bool transferStarted = result.message.startsWith("chunk ") || result.message.startsWith("step9") ||
                                 result.message.startsWith("step10") || result.message.startsWith("step11") ||
                                 result.message.startsWith("step12");
    if (transferStarted && !app::kRetryFallbackAfterTransferStart) {
        return result;
    }
    if (app::kPanelCodeFallback != app::kPanelCodePrimary) {
        Serial.printf("[epd] retry with fallback panel code %u\n", app::kPanelCodeFallback);
        transport_.deactivate();
        delay(200);
        auto detection = transport_.detect();
        if (detection.found) {
            auto fallback = tryPanelCode(app::kPanelCodeFallback, image);
            if (fallback.ok) {
                return fallback;
            }
            fallback.message = result.message + " / fallback: " + fallback.message;
            return fallback;
        }
    }
    return result;
}
