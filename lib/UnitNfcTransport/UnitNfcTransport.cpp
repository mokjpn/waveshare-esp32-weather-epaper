#include "UnitNfcTransport.hpp"

using namespace m5::nfc;

bool UnitNfcTransport::configureWire()
{
    auto pin_sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto pin_scl = M5.getPin(m5::pin_name_t::port_a_scl);
    Serial.printf("[nfc] port_a pins sda=%d scl=%d\n", pin_sda, pin_scl);
    if (pin_sda < 0 || pin_scl < 0) {
        Serial.println("[nfc] failed to resolve port_a pins");
        return false;
    }
    Wire.end();
    Wire.begin(pin_sda, pin_scl, 400000U);
    return true;
}

bool UnitNfcTransport::beginUnit()
{
    if (!configureWire()) {
        return false;
    }

    auto cfg            = unit_.config();
    cfg.mode            = (mode_ == app::TransportMode::NFCA) ? NFC::A : NFC::V;
    cfg.vdd_voltage_5V  = app::kNfcUse5V;
    cfg.tx_am_modulation = app::kNfcTxAmModulation;
    cfg.using_irq       = false;
    cfg.emulation       = false;
    unit_.config(cfg);

    started_ = units_.add(unit_, Wire) && units_.begin();
    if (!started_) {
        Serial.println("[nfc] Units.begin failed");
        return false;
    }

    layerA_.reset(new NFCLayerA(unit_));
    layerB_.reset(new NFCLayerB(unit_));
    layerF_.reset(new NFCLayerF(unit_));
    layerV_.reset(new NFCLayerV(unit_));
    Serial.printf("[nfc] unit initialized mode=%s\n", (mode_ == app::TransportMode::NFCA) ? "NFCA" : "NFCV");
    return true;
}

bool UnitNfcTransport::begin(app::TransportMode mode)
{
    mode_ = mode;
    return beginUnit();
}

bool UnitNfcTransport::switchMode(app::TransportMode mode)
{
    const auto nfcMode = (mode == app::TransportMode::NFCA) ? NFC::A : NFC::V;
    if (!switchRawMode(nfcMode, (mode == app::TransportMode::NFCA) ? "NFCA" : "NFCV")) {
        return false;
    }
    mode_ = mode;
    return true;
}

bool UnitNfcTransport::switchRawMode(m5::nfc::NFC nfcMode, const char* label)
{
    if (!started_) {
        return beginUnit();
    }
    unit_.disableField();
    delay(10);
    if (!unit_.configureNFCMode(nfcMode)) {
        Serial.printf("[nfc] configureNFCMode first attempt failed for mode=%s, retrying after field off\n", label);
        unit_.disableField();
        delay(50);
        if (!unit_.configureNFCMode(nfcMode)) {
            Serial.printf("[nfc] configureNFCMode failed for mode=%s\n", label);
            return false;
        }
    }
    Serial.printf("[nfc] switched mode=%s\n", label);
    return true;
}

void UnitNfcTransport::update()
{
    if (started_) {
        units_.update();
    }
}

bool UnitNfcTransport::detectA(DetectionResult& result)
{
    std::vector<a::PICC> piccs;
    if (!layerA_->detect(piccs, app::kNfcDetectTimeoutMs) || piccs.empty()) {
        return false;
    }
    auto picc = piccs.front();
    const bool identified = layerA_->identify(picc);
    if (identified) {
        if (!layerA_->reactivate(picc)) {
            Serial.println("[nfc] NFCA reactivate failed after identify; continuing with detected PICC");
        }
    } else {
        Serial.println("[nfc] NFCA identify failed; continuing with detected PICC");
    }
    result.found = true;
    result.uid   = picc.uidAsString().c_str();
    result.type  = picc.typeAsString().c_str();
    return true;
}

bool UnitNfcTransport::detectV(DetectionResult& result)
{
    v::PICC picc;
    if (!layerV_->detect(picc, app::kNfcDetectTimeoutMs)) {
        return false;
    }
    if (!layerV_->reactivate(picc)) {
        Serial.println("[nfc] NFCV reactivate failed");
        return false;
    }
    result.found = true;
    result.uid   = picc.uidAsString().c_str();
    result.type  = picc.typeAsString().c_str();
    return true;
}

UnitNfcTransport::DetectionResult UnitNfcTransport::detect()
{
    DetectionResult result{};
    if (!started_ && !begin(mode_)) {
        return result;
    }
    update();
    if (mode_ == app::TransportMode::NFCA) {
        detectA(result);
    } else {
        detectV(result);
    }
    if (result.found) {
        Serial.printf("[nfc] detected mode=%s uid=%s type=%s\n", (mode_ == app::TransportMode::NFCA) ? "NFCA" : "NFCV",
                      result.uid.c_str(), result.type.c_str());
    } else {
        Serial.printf("[nfc] no target detected mode=%s\n", (mode_ == app::TransportMode::NFCA) ? "NFCA" : "NFCV");
    }
    return result;
}

UnitNfcTransport::DetectionResult UnitNfcTransport::detectAfterFieldWarmup(app::TransportMode mode, uint32_t warmup_ms)
{
    DetectionResult result{};
    if (!switchMode(mode)) {
        return result;
    }
    Serial.printf("[nfc] field warmup mode=%s ms=%u\n", (mode == app::TransportMode::NFCA) ? "NFCA" : "NFCV",
                  static_cast<unsigned>(warmup_ms));
    if (!unit_.enableField()) {
        Serial.println("[nfc] enableField failed before warmup");
        return result;
    }
    delay(warmup_ms);
    return detect();
}

bool UnitNfcTransport::scanOtherModesForDiagnostics()
{
    bool found = false;

    if (switchRawMode(NFC::B, "NFCB")) {
        update();
        std::vector<b::PICC> piccs;
        if (layerB_->detect(piccs) && !piccs.empty()) {
            found = true;
            for (auto&& picc : piccs) {
                Serial.printf("[nfc] diagnostic detected mode=NFCB pupi=%s type=%s\n", picc.pupiAsString().c_str(),
                              picc.typeAsString().c_str());
            }
        } else {
            Serial.println("[nfc] diagnostic no target detected mode=NFCB");
        }
    }

    if (switchRawMode(NFC::F, "NFCF")) {
        update();
        std::vector<f::PICC> piccs;
        if (layerF_->detect(piccs) && !piccs.empty()) {
            found = true;
            for (auto&& picc : piccs) {
                Serial.printf("[nfc] diagnostic detected mode=NFCF idm=%s type=%s\n", picc.idmAsString().c_str(),
                              picc.typeAsString().c_str());
            }
        } else {
            Serial.println("[nfc] diagnostic no target detected mode=NFCF");
        }
    }

    return found;
}

bool UnitNfcTransport::scanAllModesForDiagnostics()
{
    bool found = false;

    if (!started_ && !begin(mode_)) {
        return false;
    }

    if (switchMode(app::TransportMode::NFCA)) {
        delay(50);
        auto result = detect();
        found       = result.found || found;
    }

    if (switchRawMode(NFC::B, "NFCB")) {
        update();
        std::vector<b::PICC> piccs;
        if (layerB_->detect(piccs) && !piccs.empty()) {
            found = true;
            for (auto&& picc : piccs) {
                Serial.printf("[nfc] diagnostic detected mode=NFCB pupi=%s type=%s\n", picc.pupiAsString().c_str(),
                              picc.typeAsString().c_str());
            }
        } else {
            Serial.println("[nfc] diagnostic no target detected mode=NFCB");
        }
    }

    if (switchRawMode(NFC::F, "NFCF")) {
        update();
        std::vector<f::PICC> piccs;
        if (layerF_->detect(piccs) && !piccs.empty()) {
            found = true;
            for (auto&& picc : piccs) {
                Serial.printf("[nfc] diagnostic detected mode=NFCF idm=%s type=%s\n", picc.idmAsString().c_str(),
                              picc.typeAsString().c_str());
            }
        } else {
            Serial.println("[nfc] diagnostic no target detected mode=NFCF");
        }
    }

    if (switchMode(app::TransportMode::NFCV)) {
        delay(50);
        auto result = detect();
        found       = result.found || found;
    }

    return found;
}

bool UnitNfcTransport::transceive(const uint8_t* tx, uint16_t tx_len, uint8_t* rx, uint16_t& rx_len, uint32_t timeout_ms)
{
    if (!started_) {
        return false;
    }
    update();
    if (mode_ == app::TransportMode::NFCA) {
        return unit_.nfcaTransceive(rx, rx_len, tx, tx_len, timeout_ms) != 0;
    }
    return unit_.nfcvTransceive(rx, rx_len, tx, tx_len, timeout_ms, m5::nfc::v::ModulationMode::OneOf4);
}

void UnitNfcTransport::deactivate()
{
    if (!started_) {
        return;
    }
    if (mode_ == app::TransportMode::NFCA) {
        layerA_->deactivate();
    } else {
        layerV_->deactivate();
    }
}
