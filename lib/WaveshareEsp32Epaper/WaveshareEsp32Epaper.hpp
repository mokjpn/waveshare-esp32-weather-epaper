#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <vector>

class WaveshareEsp32Epaper {
public:
    struct Result {
        bool ok{};
        String message{};
    };

    Result writeImage(const std::vector<uint8_t>& image);
    void sleep();

private:
    void beginPins();
    void reset();
    void writeByte(uint8_t data);
    void sendCommand(uint8_t command);
    void sendData(uint8_t data);
    void sendRepeated(uint8_t data, uint32_t count);
    void waitUntilIdle();
    void turnOnDisplay();
    void initPanel();
    void initPanel4Gray();
    void writeGray4Planes(const std::vector<uint8_t>& image);
    void clearPanelV2White();
    void turnOnDisplayBc();
    void initPanelBc();
    void clearPanelBc();
    void fillPanelBc(uint8_t packedPixels, const char* label);
    bool markedPixelAt(const std::vector<uint8_t>& image, size_t planeOffset, uint16_t x, uint16_t y) const;
};
