#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ——— Configuración del sensor de temperatura  ———
#define TEMP_SENSOR_PIN         25    
#define TEMP_OPERATION_TIMEOUT  5000  

// ——— Estructura de datos del sensor ———
typedef struct __attribute__((packed)) {
    uint32_t timestamp;         // Tiempo en milisegundos
    float temperature;          // Temperatura en °C
    uint16_t reading_number;    // Número de lectura
    uint8_t sensor_status;      // Estado del sensor (flags)
    bool valid;                 // Indica si la lectura es válida
} TemperatureReading;

// ——— Códigos de estado del sensor  ———
#define TEMP_STATUS_OK              0x00  // Sin errores
#define TEMP_STATUS_TIMEOUT         0x01  // Flag de timeout
#define TEMP_STATUS_INVALID_READING 0x02  // Flag de lectura inválida

// ——— Namespace para el sensor de temperatura  ———
namespace TemperatureSensor {
    
    // Constantes internas 
    constexpr float MIN_VALID_TEMP = -50.0;
    constexpr float MAX_VALID_TEMP = 85.0;
    // ——— Funciones principales ———
    bool initialize(uint8_t pin = TEMP_SENSOR_PIN);
    void cleanup();
    TemperatureReading takeReading();
    TemperatureReading takeReadingWithTimeout();
    
    // ——— Funciones de estado ———
    bool isInitialized();
    bool isLastReadingValid();
    float getLastTemperature();
    uint32_t getLastReadingTime();
    uint16_t getTotalReadings();
    
    // ——— Funciones de utilidad ———
    void printLastReading();
    bool isTemperatureInRange(float temp);
    
    // ——— Funciones para integración con sistema principal ———
    void setReadingCounter(uint16_t* total_readings_ptr);
    void setErrorLogger(void (*log_error_func)(int, int, uint32_t));
    
    // ——— Variables internas del módulo (declaraciones externas) ———
    extern OneWire* oneWire;
    extern DallasTemperature* sensors;
    extern bool initialized;
    extern uint32_t last_reading_time;
    extern TemperatureReading last_reading;
    extern uint16_t* total_readings_counter;
    extern void (*error_logger)(int code, int severity, uint32_t context);
}

#endif // TEMPERATURE_SENSOR_H