#include "CalibrationManager.h"
#include <ArduinoJson.h>
#include <stdarg.h>
#include "pH.h"
#include "TDS.h"
#include "Turbidez.h"

// Variable en RTC Memory
RTC_DATA_ATTR CalibrationManager::CalibrationData rtc_calibration_data;
CalibrationManager::CalibrationData* CalibrationManager::_calibData = &rtc_calibration_data;

CalibrationManager::CalibrationManager(bool enableSerial)
    : _enableSerialOutput(enableSerial), _initialized(false), _logCallback(nullptr) {
}

bool CalibrationManager::begin() {
    if (_enableSerialOutput && !Serial) {
        Serial.begin(115200);
        delay(100);
    }
    
    log("=== Calibration Manager Inicializado ===");
    
    if (!validateIntegrity()) {
        log("⚠ Datos inválidos - Restaurando valores por defecto");
        restoreDefaults();
    } else {
        log("✓ Datos de calibración válidos");
        logf("  Última actualización: %u", _calibData->last_update);
        logf("  Actualizaciones: %u", _calibData->update_count);
    }
    
    _initialized = true;
    applyToSensors();
    
    return true;
}

bool CalibrationManager::validateIntegrity() {
    size_t dataSize = sizeof(CalibrationData) - sizeof(uint32_t);
    uint32_t calculatedCRC = calculateCRC32(_calibData, dataSize);
    
    if (_calibData->crc != calculatedCRC) {
        return false;
    }
    
    return validatePHValues(_calibData->ph_offset, _calibData->ph_slope) &&
        validateTDSValues(_calibData->tds_kvalue, _calibData->tds_voffset) &&
        validateTurbidityValues(_calibData->turb_coeff_a, _calibData->turb_coeff_b,
                                _calibData->turb_coeff_c, _calibData->turb_coeff_d);
}

void CalibrationManager::restoreDefaults() {
    _calibData->ph_offset = DEFAULT_PH_OFFSET;
    _calibData->ph_slope = DEFAULT_PH_SLOPE;
    _calibData->tds_kvalue = DEFAULT_TDS_KVALUE;
    _calibData->tds_voffset = DEFAULT_TDS_VOFFSET;
    _calibData->turb_coeff_a = DEFAULT_TURB_A;
    _calibData->turb_coeff_b = DEFAULT_TURB_B;
    _calibData->turb_coeff_c = DEFAULT_TURB_C;
    _calibData->turb_coeff_d = DEFAULT_TURB_D;
    _calibData->last_update = millis();
    _calibData->update_count = 0;
    updateCRC();
}

// Implementar getters
float CalibrationManager::getPHOffset() { return _calibData->ph_offset; }
float CalibrationManager::getPHSlope() { return _calibData->ph_slope; }
float CalibrationManager::getTDSKValue() { return _calibData->tds_kvalue; }
float CalibrationManager::getTDSVOffset() { return _calibData->tds_voffset; }

void CalibrationManager::getTurbidityCoefficients(float& a, float& b, float& c, float& d) {
    a = _calibData->turb_coeff_a;
    b = _calibData->turb_coeff_b;
    c = _calibData->turb_coeff_c;
    d = _calibData->turb_coeff_d;
}

// Setters con validación
CalibrationManager::CalibrationResult CalibrationManager::setPHCalibration(float offset, float slope) {
    if (!_initialized) return CALIB_ERROR_NOT_INITIALIZED;
    if (!validatePHValues(offset, slope)) return CALIB_ERROR_OUT_OF_RANGE;
    
    _calibData->ph_offset = offset;
    _calibData->ph_slope = slope;
    _calibData->last_update = millis();
    _calibData->update_count++;
    updateCRC();
    
    logf("✓ pH calibrado: offset=%.2f, slope=%.2f", offset, slope);
    return CALIB_SUCCESS;
}

CalibrationManager::CalibrationResult CalibrationManager::setTDSCalibration(float kvalue, float voffset) {
    if (!_initialized) return CALIB_ERROR_NOT_INITIALIZED;
    if (!validateTDSValues(kvalue, voffset)) return CALIB_ERROR_OUT_OF_RANGE;
    
    _calibData->tds_kvalue = kvalue;
    _calibData->tds_voffset = voffset;
    _calibData->last_update = millis();
    _calibData->update_count++;
    updateCRC();
    
    logf("✓ TDS calibrado: k=%.6f, v=%.6f", kvalue, voffset);
    return CALIB_SUCCESS;
}

CalibrationManager::CalibrationResult CalibrationManager::setTurbidityCoefficients(
    float a, float b, float c, float d) {
    
    if (!_initialized) return CALIB_ERROR_NOT_INITIALIZED;
    if (!validateTurbidityValues(a, b, c, d)) return CALIB_ERROR_OUT_OF_RANGE;
    
    _calibData->turb_coeff_a = a;
    _calibData->turb_coeff_b = b;
    _calibData->turb_coeff_c = c;
    _calibData->turb_coeff_d = d;
    _calibData->last_update = millis();
    _calibData->update_count++;
    updateCRC();
    
    logf("✓ Turbidez calibrada: a=%.1f, b=%.1f, c=%.1f, d=%.1f", a, b, c, d);
    return CALIB_SUCCESS;
}

// Validaciones
bool CalibrationManager::validatePHValues(float offset, float slope) {
    if (isnan(offset) || isnan(slope)) return false;
    if (offset < -5.0f || offset > 5.0f) return false;
    if (slope < -10.0f || slope > 10.0f) return false;
    if (fabs(slope) < 0.1f) return false;
    return true;
}

bool CalibrationManager::validateTDSValues(float kvalue, float voffset) {
    if (isnan(kvalue) || isnan(voffset)) return false;
    if (kvalue < 0.1f || kvalue > 5.0f) return false;
    if (voffset < -1.0f || voffset > 1.0f) return false;
    return true;
}

bool CalibrationManager::validateTurbidityValues(float a, float b, float c, float d) {
    if (isnan(a) || isnan(b) || isnan(c) || isnan(d)) return false;
    if (isinf(a) || isinf(b) || isinf(c) || isinf(d)) return false;
    if (fabs(a) > 100000.0f || fabs(b) > 100000.0f || 
        fabs(c) > 100000.0f || fabs(d) > 100000.0f) return false;
    return true;
}

// Procesamiento de comandos JSON
CalibrationManager::CalibrationResult CalibrationManager::processCalibrationCommand(
    const String& jsonCommand) {
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonCommand);
    
    if (error) {
        logf("⚠ Error JSON: %s", error.c_str());
        return CALIB_ERROR_INVALID_VALUE;
    }
    
    if (!doc.containsKey("action") || doc["action"] != "calibrate") {
        return CALIB_ERROR_INVALID_VALUE;
    }
    
    log("📝 Procesando calibración...");
    
    CalibrationResult result = CALIB_SUCCESS;
    bool anyUpdated = false;
    
    // pH
    if (doc.containsKey("ph_offset") || doc.containsKey("ph_slope")) {
        float offset = doc.containsKey("ph_offset") ? 
                    doc["ph_offset"].as<float>() : _calibData->ph_offset;
        float slope = doc.containsKey("ph_slope") ? 
                    doc["ph_slope"].as<float>() : _calibData->ph_slope;
        
        result = setPHCalibration(offset, slope);
        if (result == CALIB_SUCCESS) {
            anyUpdated = true;
            pHSensor::setCalibration(offset, slope);
        }
    }
    
    // TDS
    if (doc.containsKey("tds_kvalue") || doc.containsKey("tds_voffset")) {
        float kvalue = doc.containsKey("tds_kvalue") ? 
                    doc["tds_kvalue"].as<float>() : _calibData->tds_kvalue;
        float voffset = doc.containsKey("tds_voffset") ? 
                    doc["tds_voffset"].as<float>() : _calibData->tds_voffset;
        
        result = setTDSCalibration(kvalue, voffset);
        if (result == CALIB_SUCCESS) {
            anyUpdated = true;
            TDSSensor::setCalibration(kvalue, voffset);
        }
    }
    
    // Turbidez
    if (doc.containsKey("turb_coeff_a") || doc.containsKey("turb_coeff_b") ||
        doc.containsKey("turb_coeff_c") || doc.containsKey("turb_coeff_d")) {
        
        float a = doc.containsKey("turb_coeff_a") ? 
                doc["turb_coeff_a"].as<float>() : _calibData->turb_coeff_a;
        float b = doc.containsKey("turb_coeff_b") ? 
                doc["turb_coeff_b"].as<float>() : _calibData->turb_coeff_b;
        float c = doc.containsKey("turb_coeff_c") ? 
                doc["turb_coeff_c"].as<float>() : _calibData->turb_coeff_c;
        float d = doc.containsKey("turb_coeff_d") ? 
                doc["turb_coeff_d"].as<float>() : _calibData->turb_coeff_d;
        
        result = setTurbidityCoefficients(a, b, c, d);
        if (result == CALIB_SUCCESS) {
            anyUpdated = true;
            TurbiditySensor::setCalibrationCoefficients(a, b, c, d);
        }
    }
    
    // Restaurar defaults
    if (doc.containsKey("restore_defaults") && doc["restore_defaults"].as<bool>()) {
        restoreDefaults();
        applyToSensors();
        anyUpdated = true;
    }
    
    if (anyUpdated) {
        log("✓ Calibración actualizada");
        printCalibrationInfo();
    }
    
    return result;
}

String CalibrationManager::getCalibrationJSON() {
    StaticJsonDocument<512> doc;
    
    doc["ph_offset"] = _calibData->ph_offset;
    doc["ph_slope"] = _calibData->ph_slope;
    doc["tds_kvalue"] = _calibData->tds_kvalue;
    doc["tds_voffset"] = _calibData->tds_voffset;
    doc["turb_coeff_a"] = _calibData->turb_coeff_a;
    doc["turb_coeff_b"] = _calibData->turb_coeff_b;
    doc["turb_coeff_c"] = _calibData->turb_coeff_c;
    doc["turb_coeff_d"] = _calibData->turb_coeff_d;
    doc["last_update"] = _calibData->last_update;
    doc["update_count"] = _calibData->update_count;
    doc["crc"] = _calibData->crc;
    
    String output;
    serializeJson(doc, output);
    return output;
}

void CalibrationManager::printCalibrationInfo() {
    log("\n=== VALORES DE CALIBRACIÓN ===");
    logf("pH: offset=%.2f, slope=%.2f", _calibData->ph_offset, _calibData->ph_slope);
    logf("TDS: k=%.6f, v=%.6f", _calibData->tds_kvalue, _calibData->tds_voffset);
    logf("Turb: a=%.1f, b=%.1f, c=%.1f, d=%.1f", 
        _calibData->turb_coeff_a, _calibData->turb_coeff_b,
        _calibData->turb_coeff_c, _calibData->turb_coeff_d);
    logf("Updates: %u, CRC: 0x%08X", _calibData->update_count, _calibData->crc);
    log("==============================\n");
}

uint32_t CalibrationManager::getLastUpdateTime() { return _calibData->last_update; }
uint16_t CalibrationManager::getUpdateCount() { return _calibData->update_count; }

void CalibrationManager::applyToSensors() {
    log("🔧 Aplicando calibración a sensores...");
    
    if (pHSensor::isInitialized()) {
        pHSensor::setCalibration(_calibData->ph_offset, _calibData->ph_slope);
    }
    
    if (TDSSensor::isInitialized()) {
        TDSSensor::setCalibration(_calibData->tds_kvalue, _calibData->tds_voffset);
    }
    
    if (TurbiditySensor::isInitialized()) {
        TurbiditySensor::setCalibrationCoefficients(
            _calibData->turb_coeff_a, _calibData->turb_coeff_b,
            _calibData->turb_coeff_c, _calibData->turb_coeff_d
        );
    }
    
    log("✓ Calibración aplicada");
}

void CalibrationManager::setLogCallback(LogCallback callback) {
    _logCallback = callback;
}

void CalibrationManager::enableSerial(bool enable) {
    _enableSerialOutput = enable;
}

void CalibrationManager::updateCRC() {
    size_t dataSize = sizeof(CalibrationData) - sizeof(uint32_t);
    _calibData->crc = calculateCRC32(_calibData, dataSize);
}

uint32_t CalibrationManager::calculateCRC32(const void* data, size_t length) {
    return esp_crc32_le(0xFFFFFFFF, (const uint8_t*)data, length) ^ 0xFFFFFFFF;
}

void CalibrationManager::log(const char* message) {
    if (_logCallback) {
        _logCallback(message);
    } else if (_enableSerialOutput && Serial) {
        Serial.println(message);
    }
}

void CalibrationManager::logf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(buffer);
}