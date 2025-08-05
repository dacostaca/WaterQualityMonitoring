#ifndef TDS_SENSOR_H
#define TDS_SENSOR_H

#include <Arduino.h>
#include <esp_adc_cal.h>

// ——— Configuración del sensor TDS ———
#define TDS_SENSOR_PIN          34    
#define TDS_OPERATION_TIMEOUT   5000  // Timeout para operación del sensor

// ——— VALORES CALIBRADOS FIJOS ———
#define TDS_CALIBRATED_KVALUE   1.60f     
#define TDS_CALIBRATED_VOFFSET  0.10000f 

// ——— Estructura de datos del sensor TDS ———
typedef struct __attribute__((packed)) {
    uint32_t timestamp;         // Tiempo en milisegundos
    float tds_value;           // Valor TDS en ppm
    float ec_value;            // Conductividad eléctrica en µS/cm
    float temperature;         // Temperatura usada para compensación
    uint16_t reading_number;   // Número de lectura
    uint8_t sensor_status;     // Estado del sensor (flags)
    bool valid;                // Indica si la lectura es válida
} TDSReading;

// ——— Códigos de estado del sensor ———
#define TDS_STATUS_OK               0x00  // Sin errores
#define TDS_STATUS_TIMEOUT          0x01  // Flag de timeout
#define TDS_STATUS_INVALID_READING  0x02  // Flag de lectura inválida
#define TDS_STATUS_VOLTAGE_LOW      0x04  // Voltaje muy bajo
#define TDS_STATUS_VOLTAGE_HIGH     0x08  // Voltaje muy alto

// ——— Namespace para el sensor TDS ———
namespace TDSSensor {
    
    // Constantes de validación
    constexpr float MIN_VALID_TDS = 0.0;
    constexpr float MAX_VALID_TDS = 2000.0;
    constexpr float MIN_VALID_EC = 0.0;
    constexpr float MAX_VALID_EC = 4000.0;
    constexpr float MIN_VALID_VOLTAGE = 0.001;
    constexpr float MAX_VALID_VOLTAGE = 2.2;
    
    // ——— Valores de calibración fijos ———
    extern float kValue;              
    extern float voltageOffset;      
    
    // ——— Funciones principales ———
    bool initialize(uint8_t pin = TDS_SENSOR_PIN);
    TDSReading takeReading(float temperature);
    TDSReading takeReadingWithTimeout(float temperature);
    
    // ——— Funciones de calibración ———
    void setCalibration(float kVal, float vOffset);
    void getCalibration(float& kVal, float& vOffset);
    void resetToDefaultCalibration();
    
    // ——— Funciones de estado ———
    bool isInitialized();
    bool isLastReadingValid();
    float getLastTDS();
    float getLastEC();
    uint32_t getLastReadingTime();
    uint16_t getTotalReadings();
    
    // ——— Funciones de utilidad ———
    void printLastReading();
    bool isTDSInRange(float tds);
    bool isECInRange(float ec);
    bool isVoltageInRange(float voltage);
    String getWaterQuality(float tds);
    
    // ——— Funciones para integración con sistema principal ———
    void setReadingCounter(uint16_t* total_readings_ptr);
    void setErrorLogger(void (*log_error_func)(int, int, uint32_t));
    
    // ——— Variables internas del módulo ———
    extern bool initialized;
    extern uint8_t sensor_pin;
    extern uint32_t last_reading_time;
    extern TDSReading last_reading;
    extern uint16_t* total_readings_counter;
    extern void (*error_logger)(int code, int severity, uint32_t context);
    extern esp_adc_cal_characteristics_t adc_chars;
    
    // ——— Configuración de muestreo ———
    constexpr int SAMPLES = 30;  // Número de lecturas a promediar
    
    // ——— Funciones adicionales para debugging ———
    void showCalibrationInfo();
    void testReading();
    void debugVoltageReading();
}

#endif // TDS_SENSOR_H