#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <memory>
#include <vector>
#include "app_config.h"

class UnitNfcTransport {
public:
    struct DetectionResult {
        bool found{};
        String uid{};
        String type{};
    };

    bool begin(app::TransportMode mode);
    bool switchMode(app::TransportMode mode);
    void update();

    DetectionResult detect();
    DetectionResult detectAfterFieldWarmup(app::TransportMode mode, uint32_t warmup_ms);
    bool scanOtherModesForDiagnostics();
    bool scanAllModesForDiagnostics();
    bool transceive(const uint8_t* tx, uint16_t tx_len, uint8_t* rx, uint16_t& rx_len, uint32_t timeout_ms);
    void deactivate();

private:
    bool beginUnit();
    bool configureWire();
    bool detectA(DetectionResult& result);
    bool detectV(DetectionResult& result);
    bool switchRawMode(m5::nfc::NFC nfcMode, const char* label);

    m5::unit::UnitUnified units_{};
    m5::unit::UnitNFC unit_{app::kNfcI2cAddress};
    std::unique_ptr<m5::nfc::NFCLayerA> layerA_{};
    std::unique_ptr<m5::nfc::NFCLayerB> layerB_{};
    std::unique_ptr<m5::nfc::NFCLayerF> layerF_{};
    std::unique_ptr<m5::nfc::NFCLayerV> layerV_{};
    app::TransportMode mode_{app::TransportMode::NFCA};
    bool started_{false};
};
