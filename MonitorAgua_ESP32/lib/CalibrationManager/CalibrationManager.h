#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

#include <Arduino.h>
#include "esp_crc.h"

class CalibrationManager {
public:
    typedef struct __attribute__((packed)) {
        // pH Sensor
        float ph_offset;
        float ph_slope;
        
        // TDS Sensor
        float tds_kvalue;
        float tds_voffset;
        
        // Turbidity Sensor
        float turb_coeff_a;
        float turb_coeff_b;
        float turb_coeff_c;
        float turb_coeff_d;
        
        // Metadata
        uint32_t last_update;
        uint16_t update_count;
        uint32_t crc;
    } CalibrationData;

    // Valores por defecto
    static constexpr float DEFAULT_PH_OFFSET = 1.33f;
    static constexpr float DEFAULT_PH_SLOPE = 3.5f;
    static constexpr float DEFAULT_TDS_KVALUE = 1.60f;
    static constexpr float DEFAULT_TDS_VOFFSET = 0.10000f;
    static constexpr float DEFAULT_TURB_A = -1120.4f;
    static constexpr float DEFAULT_TURB_B = 5742.3f;
    static constexpr float DEFAULT_TURB_C = -4352.9f;
    static constexpr float DEFAULT_TURB_D = -2500.0f;

    enum CalibrationResult {
        CALIB_SUCCESS = 0,
        CALIB_ERROR_INVALID_VALUE,
        CALIB_ERROR_OUT_OF_RANGE,
        CALIB_ERROR_CRC_MISMATCH,
        CALIB_ERROR_WRITE_FAILED,
        CALIB_ERROR_NOT_INITIALIZED
    };

    typedef void (*LogCallback)(const char* message);

private:
    bool _enableSerialOutput;
    bool _initialized;
    LogCallback _logCallback;
    static CalibrationData* _calibData;

public:
    CalibrationManager(bool enableSerial = true);
    bool begin();
    bool validateIntegrity();
    void restoreDefaults();
    
    // Getters/Setters pH
    float getPHOffset();
    float getPHSlope();
    CalibrationResult setPHCalibration(float offset, float slope);
    
    // Getters/Setters TDS
    float getTDSKValue();
    float getTDSVOffset();
    CalibrationResult setTDSCalibration(float kvalue, float voffset);
    
    // Getters/Setters Turbidez
    void getTurbidityCoefficients(float& a, float& b, float& c, float& d);
    CalibrationResult setTurbidityCoefficients(float a, float b, float c, float d);
    
    // Validación
    bool validatePHValues(float offset, float slope);
    bool validateTDSValues(float kvalue, float voffset);
    bool validateTurbidityValues(float a, float b, float c, float d);
    
    // Procesamiento de comandos
    CalibrationResult processCalibrationCommand(const String& jsonCommand);
    String getCalibrationJSON();
    
    // Información
    void printCalibrationInfo();
    uint32_t getLastUpdateTime();
    uint16_t getUpdateCount();
    
    // Aplicar a sensores
    void applyToSensors();
    
    void setLogCallback(LogCallback callback);
    void enableSerial(bool enable);

private:
    void updateCRC();
    uint32_t calculateCRC32(const void* data, size_t length);
    void log(const char* message);
    void logf(const char* format, ...);
};

#endif