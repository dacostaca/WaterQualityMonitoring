#ifndef PH_SENSOR_H
#define PH_SENSOR_H

#include <Arduino.h>
#include <esp_adc_cal.h>

// ——— Configuración del sensor de pH  ———
#define PH_SENSOR_PIN           33    // Pin ADC
#define PH_OPERATION_TIMEOUT    5000  // Timeout para operación del sensor

// ——— Valores de calibración por defecto ———
#define PH_CALIBRATED_OFFSET    1.33f     // Offset calibrado en pH 7.0
#define PH_CALIBRATED_SLOPE     3.5f      // Pendiente de la curva de calibración
#define PH_SAMPLING_INTERVAL    20        // ms entre muestras
#define PH_ARRAY_LENGTH        40         // Número de muestras para promediar

// ——— Estructura de datos del sensor pH  ———
typedef struct __attribute__((packed)) {
    uint32_t timestamp;         // Tiempo en milisegundos
    float ph_value;            // Valor de pH (0-14)
    float voltage;             // Voltaje medido del sensor
    float temperature; 
    uint16_t reading_number;   // Número de lectura
    uint8_t sensor_status;     // Estado del sensor (flags)
    bool valid;                // Indica si la lectura es válida
} pHReading;

// ——— Códigos de estado del sensor ———
#define PH_STATUS_OK               0x00  // Sin errores
#define PH_STATUS_TIMEOUT          0x01  // Flag de timeout
#define PH_STATUS_INVALID_READING  0x02  // Flag de lectura inválida
#define PH_STATUS_VOLTAGE_LOW      0x04  // Voltaje muy bajo
#define PH_STATUS_VOLTAGE_HIGH     0x08  // Voltaje muy alto
#define PH_STATUS_OUT_OF_RANGE     0x10  // pH fuera de rango (0-14)

// ——— Namespace para el sensor de pH ———
namespace pHSensor {
    
    // Constantes de validación
    constexpr float MIN_VALID_PH = 0.0f;
    constexpr float MAX_VALID_PH = 14.0f;
    constexpr float MIN_VALID_VOLTAGE = 0.1f;
    constexpr float MAX_VALID_VOLTAGE = 3.2f;
    
    // ——— Valores de calibración  ———
    extern float phOffset;              // Offset de calibración
    extern float phSlope;               // Pendiente de calibración
    
    // ——— Funciones principales ———
    bool initialize(uint8_t pin = PH_SENSOR_PIN);
    void cleanup();
    pHReading takeReading(float temperature );
    pHReading takeReadingWithTimeout(float temperature);
    
    // ——— Funciones de calibración ———
    void setCalibration(float offset, float slope);
    void getCalibration(float& offset, float& slope);
    void resetToDefaultCalibration();
    bool calibrateWithBuffer(float bufferPH, float measuredVoltage);
    
    // ——— Funciones de estado ———
    bool isInitialized();
    bool isLastReadingValid();
    float getLastPH();
    float getLastVoltage();
    uint32_t getLastReadingTime();
    uint16_t getTotalReadings();
    
    // ——— Funciones de utilidad ———
    void printLastReading();
    bool isPHInRange(float ph);
    bool isVoltageInRange(float voltage);
    String getWaterType(float ph);
    
    // ——— Funciones para integración con sistema principal ———
    void setReadingCounter(uint16_t* total_readings_ptr);
    void setErrorLogger(void (*log_error_func)(int, int, uint32_t));
    
    // ——— Variables internas del módulo ———
    extern bool initialized;
    extern uint8_t sensor_pin;
    extern uint32_t last_reading_time;
    extern pHReading last_reading;
    extern uint16_t* total_readings_counter;
    extern void (*error_logger)(int code, int severity, uint32_t context);
    extern esp_adc_cal_characteristics_t adc_chars;
    
    // ——— Buffer de muestras para promediado ———
    extern int phArray[PH_ARRAY_LENGTH];
    extern int phArrayIndex;
    
    // ——— Funciones adicionales para debugging ———
    void showCalibrationInfo();
    void testReading();
    void performCalibrationRoutine();
}

#endif // PH_SENSOR_H