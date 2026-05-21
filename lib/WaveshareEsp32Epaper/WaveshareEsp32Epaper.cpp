#include "WaveshareEsp32Epaper.hpp"
#include "app_config.h"

namespace {

constexpr uint8_t kPinSck  = 13;
constexpr uint8_t kPinMosi = 14;
constexpr uint8_t kPinCs   = 15;
constexpr uint8_t kPinRst  = 26;
constexpr uint8_t kPinDc   = 27;
constexpr uint8_t kPinBusy = 25;

}  // namespace

void WaveshareEsp32Epaper::beginPins()
{
    pinMode(kPinBusy, INPUT);
    pinMode(kPinRst, OUTPUT);
    pinMode(kPinDc, OUTPUT);
    pinMode(kPinCs, OUTPUT);
    pinMode(kPinSck, OUTPUT);
    pinMode(kPinMosi, OUTPUT);
    digitalWrite(kPinCs, HIGH);
    digitalWrite(kPinSck, LOW);
}

void WaveshareEsp32Epaper::reset()
{
    digitalWrite(kPinRst, HIGH);
    delay(200);
    digitalWrite(kPinRst, LOW);
    delay(2);
    digitalWrite(kPinRst, HIGH);
    delay(200);
}

void WaveshareEsp32Epaper::writeByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; ++i) {
        digitalWrite(kPinMosi, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        digitalWrite(kPinSck, HIGH);
        digitalWrite(kPinSck, LOW);
    }
}

void WaveshareEsp32Epaper::sendCommand(uint8_t command)
{
    digitalWrite(kPinDc, LOW);
    digitalWrite(kPinCs, LOW);
    writeByte(command);
    digitalWrite(kPinCs, HIGH);
}

void WaveshareEsp32Epaper::sendData(uint8_t data)
{
    digitalWrite(kPinDc, HIGH);
    digitalWrite(kPinCs, LOW);
    writeByte(data);
    digitalWrite(kPinCs, HIGH);
}

void WaveshareEsp32Epaper::sendRepeated(uint8_t data, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        sendData(data);
    }
}

void WaveshareEsp32Epaper::waitUntilIdle()
{
    Serial.println("[epd] busy");
    const auto start = millis();
    while (true) {
        sendCommand(0x71);
        const bool busy = (digitalRead(kPinBusy) & 0x01) == 0;
        if (!busy) {
            break;
        }
        if (millis() - start > app::kEpdBusyTimeoutMs) {
            Serial.println("[epd] busy wait timeout");
            break;
        }
        delay(100);
    }
    delay(200);
    Serial.println("[epd] busy release");
}

void WaveshareEsp32Epaper::turnOnDisplay()
{
    sendCommand(0x12);
    delay(100);
    waitUntilIdle();
}

void WaveshareEsp32Epaper::turnOnDisplayBc()
{
    sendCommand(0x04);
    waitUntilIdle();
    sendCommand(0x12);
    delay(100);
    waitUntilIdle();
}

void WaveshareEsp32Epaper::initPanel()
{
    reset();

    sendCommand(0x06);  // BOOSTER SOFT START
    sendData(0x17);
    sendData(0x17);
    sendData(0x28);
    sendData(0x17);

    sendCommand(0x01);  // POWER SETTING
    sendData(0x07);
    sendData(0x07);
    sendData(0x28);
    sendData(0x17);

    sendCommand(0x04);  // POWER ON
    delay(100);
    waitUntilIdle();

    sendCommand(0x00);  // PANEL SETTING
    sendData(0x1F);

    sendCommand(0x61);  // RESOLUTION SETTING: 800 x 480
    sendData(0x03);
    sendData(0x20);
    sendData(0x01);
    sendData(0xE0);

    sendCommand(0x15);
    sendData(0x00);

    sendCommand(0x50);  // VCOM AND DATA INTERVAL SETTING
    sendData(0x10);
    sendData(0x07);

    sendCommand(0x60);  // TCON SETTING
    sendData(0x22);

    sendCommand(0x65);  // RESOLUTION SETTING
    sendData(0x00);
    sendData(0x00);
    sendData(0x00);
    sendData(0x00);
}

void WaveshareEsp32Epaper::initPanel4Gray()
{
    reset();

    sendCommand(0x00);  // PANEL SETTING
    sendData(0x1F);

    sendCommand(0x50);  // VCOM AND DATA INTERVAL SETTING
    sendData(0x10);
    sendData(0x07);

    sendCommand(0x04);  // POWER ON
    delay(100);
    waitUntilIdle();

    sendCommand(0x06);  // BOOSTER SOFT START
    sendData(0x27);
    sendData(0x27);
    sendData(0x18);
    sendData(0x17);

    sendCommand(0xE0);
    sendData(0x02);
    sendCommand(0xE5);
    sendData(0x5F);
}

void WaveshareEsp32Epaper::initPanelBc()
{
    reset();

    sendCommand(0x01);  // POWER SETTING
    sendData(0x37);
    sendData(0x00);

    sendCommand(0x00);  // PANEL SETTING
    sendData(0xCF);
    sendData(0x08);

    sendCommand(0x30);  // PLL CONTROL
    sendData(0x3A);

    sendCommand(0x82);  // VCM DC SETTING
    sendData(0x28);

    sendCommand(0x06);  // BOOSTER SOFT START
    sendData(0xC7);
    sendData(0xCC);
    sendData(0x15);

    sendCommand(0x50);  // VCOM AND DATA INTERVAL SETTING
    sendData(0x77);

    sendCommand(0x60);  // TCON SETTING
    sendData(0x22);

    sendCommand(0x65);  // FLASH CONTROL
    sendData(0x00);

    sendCommand(0x61);  // 640 x 384
    sendData(0x02);
    sendData(0x80);
    sendData(0x01);
    sendData(0x80);

    sendCommand(0xE5);  // FLASH MODE
    sendData(0x03);
}

void WaveshareEsp32Epaper::writeGray4Planes(const std::vector<uint8_t>& image)
{
    Serial.println("[epd] sending 4-gray old plane");
    sendCommand(0x10);
    for (uint32_t i = 0; i < app::kPackedImageBytes; ++i) {
        uint8_t out = 0;
        for (uint8_t j = 0; j < 2; ++j) {
            uint8_t src = image[i * 2 + j];
            for (uint8_t k = 0; k < 2; ++k) {
                const uint8_t p0 = src & 0xC0;
                out |= (p0 == 0x00 || p0 == 0x80) ? 0x01 : 0x00;
                out <<= 1;
                src <<= 2;

                const uint8_t p1 = src & 0xC0;
                out |= (p1 == 0x00 || p1 == 0x80) ? 0x01 : 0x00;
                if (j != 1 || k != 1) {
                    out <<= 1;
                }
                src <<= 2;
            }
        }
        sendData(out);
    }

    Serial.println("[epd] sending 4-gray new plane");
    sendCommand(0x13);
    for (uint32_t i = 0; i < app::kPackedImageBytes; ++i) {
        uint8_t out = 0;
        for (uint8_t j = 0; j < 2; ++j) {
            uint8_t src = image[i * 2 + j];
            for (uint8_t k = 0; k < 2; ++k) {
                const uint8_t p0 = src & 0xC0;
                out |= (p0 == 0x00 || p0 == 0x40) ? 0x01 : 0x00;
                out <<= 1;
                src <<= 2;

                const uint8_t p1 = src & 0xC0;
                out |= (p1 == 0x00 || p1 == 0x40) ? 0x01 : 0x00;
                if (j != 1 || k != 1) {
                    out <<= 1;
                }
                src <<= 2;
            }
        }
        sendData(out);
    }
}

void WaveshareEsp32Epaper::clearPanelV2White()
{
    Serial.println("[epd] clearing V2 panel to white");
    sendCommand(0x10);
    sendRepeated(0xFF, app::kPackedImageBytes);
    sendCommand(0x13);
    sendRepeated(0x00, app::kPackedImageBytes);
    turnOnDisplay();
}

void WaveshareEsp32Epaper::fillPanelBc(uint8_t packedPixels, const char* label)
{
    Serial.printf("[epd] 7in5bc fill %s\n", label);
    sendCommand(0x10);
    for (uint16_t y = 0; y < app::kImageHeight; ++y) {
        for (uint16_t x = 0; x < app::kImageWidth; x += 2) {
            sendData(packedPixels);
        }
    }
    turnOnDisplayBc();
}

void WaveshareEsp32Epaper::clearPanelBc()
{
    if (app::kEpdAggressiveClear) {
        fillPanelBc(0x00, "black");
        fillPanelBc(0x33, "white after black");
        fillPanelBc(0x44, "red");
        fillPanelBc(0x33, "white after red");
        return;
    }
    fillPanelBc(0x33, "white");
}

bool WaveshareEsp32Epaper::markedPixelAt(
    const std::vector<uint8_t>& image,
    size_t planeOffset,
    uint16_t x,
    uint16_t y
) const
{
    if (x >= app::kImageWidth || y >= app::kImageHeight) {
        return false;
    }
    constexpr uint16_t bytesPerRow = app::kImageWidth / 8;
    const size_t index = planeOffset + y * bytesPerRow + (x / 8);
    if (index >= image.size()) {
        return false;
    }
    const uint8_t value = image[index];
    return (value & (0x80 >> (x % 8))) == 0;
}

WaveshareEsp32Epaper::Result WaveshareEsp32Epaper::writeImage(const std::vector<uint8_t>& image)
{
    Result result;
    if (app::kEpdFourGray && image.size() != app::kImagePayloadBytes) {
        result.message = "invalid 4-gray image size";
        return result;
    }
    if (image.size() != app::kPackedImageBytes && image.size() != app::kImagePayloadBytes) {
        result.message = "invalid image size";
        return result;
    }

    beginPins();
#if defined(APP_EPD_7IN5BC)
    initPanelBc();
    if (app::kEpdClearBeforeUpdate) {
        Serial.println("[epd] clearing 7in5bc panel before update");
        clearPanelBc();
    }

    Serial.println("[epd] sending 7in5bc packed plane");
    sendCommand(0x10);
    for (uint16_t y = 0; y < app::kImageHeight; ++y) {
        for (uint16_t x = 0; x < app::kImageWidth; x += 2) {
            const bool black0 = markedPixelAt(image, 0, x, y);
            const bool black1 = markedPixelAt(image, 0, x + 1, y);
            const bool red0 = markedPixelAt(image, app::kPackedImageBytes, x, y);
            const bool red1 = markedPixelAt(image, app::kPackedImageBytes, x + 1, y);
            const uint8_t nibble0 = red0 ? 0x04 : (black0 ? 0x00 : 0x03);
            const uint8_t nibble1 = red1 ? 0x04 : (black1 ? 0x00 : 0x03);
            sendData((nibble0 << 4) | nibble1);
        }
    }

    Serial.println("[epd] refreshing 7in5bc");
    turnOnDisplayBc();
    sleep();

    result.ok = true;
    result.message = "spi 7in5bc update complete";
    return result;
#else
    if (app::kEpdFourGray) {
        if (app::kEpdGray4ClearBeforeUpdate) {
            initPanel();
            clearPanelV2White();
            delay(500);
        }
        initPanel4Gray();
        Serial.println("[epd] sending 4-gray image");
        writeGray4Planes(image);
        Serial.println("[epd] refreshing 4-gray");
        turnOnDisplay();
        sleep();

        result.ok = true;
        result.message = "spi 4-gray update complete";
        return result;
    }

    initPanel();

    Serial.println("[epd] sending 1bpp old plane");
    sendCommand(0x10);
    for (auto value : image) {
        sendData(value);
    }

    Serial.println("[epd] sending 1bpp new plane");
    sendCommand(0x13);
    for (auto value : image) {
        sendData(~value);
    }

    Serial.println("[epd] refreshing 1bpp");
    turnOnDisplay();
    sleep();

    result.ok = true;
    result.message = "spi 1bpp update complete";
    return result;
#endif
}

void WaveshareEsp32Epaper::sleep()
{
    sendCommand(0x02);  // POWER OFF
    waitUntilIdle();
    sendCommand(0x07);  // DEEP SLEEP
    sendData(0xA5);
}
