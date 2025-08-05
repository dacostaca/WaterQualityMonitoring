#ifndef TURBIDITY_SENSOR_H
#define TURBIDITY_SENSOR_H

#include <Arduino.h>
#include <esp_adc_cal.h>

// ——— Configuración del sensor de turbidez ———
#define TURBIDITY_SENSOR_PIN    32    
#define TURBIDITY_OPERATION_TIMEOUT  5000  // Timeout para operación del sensor

// ——— Valores de calibración obtenidos experimentalmente ———
#define TURBIDITY_MAX_VOLTAGE   2.179100f  
#define TURBIDITY_MIN_VOLTAGE   0.653200f  

// ——— Estructura de datos del sensor de turbidez ———
typedef struct __attribute__((packed)) {
    uint32_t timestamp;         // Tiempo en milisegundos
    float turbidity_ntu;        // Valor de turbidez en NTU
    float voltage;              // Voltaje medido del sensor
    uint16_t reading_number;    // Número de lectura
    uint8_t sensor_status;      // Estado del sensor (flags)
    bool valid;                 // Indica si la lectura es válida
} TurbidityReading;

// ——— Códigos de estado del sensor ———
#define TURBIDITY_STATUS_OK              0x00  // Sin errores
#define TURBIDITY_STATUS_TIMEOUT         0x01  // Flag de timeout
#define TURBIDITY_STATUS_INVALID_READING 0x02  // Flag de lectura inválida
#define TURBIDITY_STATUS_VOLTAGE_LOW     0x04  // Voltaje muy bajo
#define TURBIDITY_STATUS_VOLTAGE_HIGH    0x08  // Voltaje muy alto
#define TURBIDITY_STATUS_OVERFLOW        0x10  // Turbidez fuera de rango

// ——— Namespace para el sensor de turbidez ———
namespace TurbiditySensor {
    
    // Constantes de validación
    constexpr float MIN_VALID_NTU = 0.0;
    constexpr float MAX_VALID_NTU = 3000.0;
    constexpr float MIN_VALID_VOLTAGE = 0.1;   // Voltaje mínimo válido
    constexpr float MAX_VALID_VOLTAGE = 2.5;   // Voltaje máximo válido
    
    // Coeficientes de calibración
    // Ecuación: NTU = a*V³ + b*V² + c*V + d
    constexpr float CALIB_COEFF_A = -1120.4f;  // Coeficiente cúbico
    constexpr float CALIB_COEFF_B = 5742.3f;   // Coeficiente cuadrático  
    constexpr float CALIB_COEFF_C = -4352.9f;  // Coeficiente lineal
    constexpr float CALIB_COEFF_D = -2500.0f;  // Término independiente
    
    // ——— Funciones principales  ———
    bool initialize(uint8_t pin = TURBIDITY_SENSOR_PIN);
    void cleanup();
    TurbidityReading takeReading();
    TurbidityReading takeReadingWithTimeout();
    
    // ——— Funciones de calibración ———
    float voltageToNTU(float voltage);
    float calibrateReading(float rawVoltage);
    void setCalibrationCoefficients(float a, float b, float c, float d);
    void getCalibrationCoefficients(float& a, float& b, float& c, float& d);
    void resetToDefaultCalibration();
    
    // ——— Funciones de estado ———
    bool isInitialized();
    bool isLastReadingValid();
    float getLastTurbidity();
    float getLastVoltage();
    uint32_t getLastReadingTime();
    uint16_t getTotalReadings();
    
    // ——— Funciones de utilidad ———
    void printLastReading();
    bool isTurbidityInRange(float ntu);
    bool isVoltageInRange(float voltage);
    String getWaterQuality(float ntu);
    String getTurbidityCategory(float ntu);
    
    // ——— Funciones para integración con sistema principal ———
    void setReadingCounter(uint16_t* total_readings_ptr);
    void setErrorLogger(void (*log_error_func)(int, int, uint32_t));
    
    // ——— Variables internas del módulo ———
    extern bool initialized;
    extern uint8_t sensor_pin;
    extern uint32_t last_reading_time;
    extern TurbidityReading last_reading;
    extern uint16_t* total_readings_counter;
    extern void (*error_logger)(int code, int severity, uint32_t context);
    extern esp_adc_cal_characteristics_t adc_chars;
    
    // ——— Configuración de muestreo ———
    constexpr int SAMPLES = 50;  // Número de lecturas a promediar
    
    // ——— Funciones adicionales para debugging ———
    void showCalibrationInfo();
    void testReading();
    void debugVoltageReading();
    void printCalibrationCurve();
}

#endif // TURBIDITY_SENSOR_H