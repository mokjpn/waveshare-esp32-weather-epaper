#pragma once

#include <Arduino.h>
#include <vector>
#include "UnitNfcTransport.hpp"
#include "app_config.h"

class WaveshareNfcEpaper {
public:
    struct Result {
        bool ok{};
        String message{};
        uint8_t panelCodeTried{};
    };

    explicit WaveshareNfcEpaper(UnitNfcTransport& transport) : transport_(transport) {}

    Result writeImage(const std::vector<uint8_t>& image);

private:
    bool exchange(const uint8_t* tx, uint16_t tx_len, uint8_t expected0, uint8_t expected1, String& error);
    bool waitUntilRefreshDone(String& error);
    Result tryPanelCode(uint8_t panelCode, const std::vector<uint8_t>& image);

    UnitNfcTransport& transport_;
};

