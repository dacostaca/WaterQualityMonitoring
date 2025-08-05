#include "Temperatura.h"

// ——— Variables internas del módulo ———
namespace TemperatureSensor {
    
    // Variables del sensor 
    OneWire* oneWire = nullptr;
    DallasTemperature* sensors = nullptr;
    bool initialized = false;
    uint32_t last_reading_time = 0;
    TemperatureReading last_reading = {0};
    
    // Punteros a funciones externas
    uint16_t* total_readings_counter = nullptr;
    void (*error_logger)(int code, int severity, uint32_t context) = nullptr;
    
    
    
    // ——— Implementación de funciones ———
    
    /**
     * Inicializar sensor de temperatura
     */
    bool initialize(uint8_t pin) {
        if (initialized) {
            //Serial.println(" Sensor temperatura ya inicializado");
            return true;
        }
        
        //Serial.printf(" Inicializando sensor temperatura (pin %d)...\n", pin);
        
        // Crear objetos 
        oneWire = new OneWire(pin);
        if (!oneWire) {
            Serial.println(" Error creando OneWire");
            return false;
        }
        
        sensors = new DallasTemperature(oneWire);
        if (!sensors) {
            Serial.println(" Error creando DallasTemperature");
            delete oneWire;
            oneWire = nullptr;
            return false;
        }
        
        // Inicializar sensor
        sensors->begin();
        
        initialized = true;
        last_reading_time = millis();
        
        //Serial.println(" Sensor temperatura inicializado correctamente");
        return true;
    }
    
    /**
     * Limpiar recursos del sensor
     */
    void cleanup() {
        if (sensors) {
            delete sensors;
            sensors = nullptr;
        }
        if (oneWire) {
            delete oneWire;
            oneWire = nullptr;
        }
        initialized = false;
        Serial.println(" Sensor temperatura limpiado");
    }
    
    /**
     * Tomar lectura con timeout por defecto
     */
    TemperatureReading takeReading() {
        return takeReadingWithTimeout();
    }
    
    /**
     * Tomar lectura de temperatura con timeout 
     */
    TemperatureReading takeReadingWithTimeout() {
        TemperatureReading reading = {0};
        
        if (!initialized || !sensors) {
            Serial.println(" Sensor temperatura no inicializado");
            reading.valid = false;
            reading.sensor_status = TEMP_STATUS_INVALID_READING;
            return reading;
        }
        
        // Incrementar contador 
        if (total_readings_counter) {
            (*total_readings_counter)++;
            reading.reading_number = *total_readings_counter;
        }
        
        reading.timestamp = millis();
        
        // Timeout para operación del sensor 
        uint32_t start_time = millis();
        
        sensors->requestTemperatures();
        
        // Verificar timeout 
        while (!sensors->isConversionComplete()) {
            if (millis() - start_time > TEMP_OPERATION_TIMEOUT) {
                Serial.println(" Timeout en lectura de sensor");
                
                if (error_logger) {
                    error_logger(1, 1, millis() - start_time); // ERROR_SENSOR_TIMEOUT, SEVERITY_WARNING
                }
                
                reading.valid = false;
                reading.sensor_status = TEMP_STATUS_TIMEOUT;  
                
                if (total_readings_counter) {
                    (*total_readings_counter)--;
                }
                
                last_reading = reading;
                return reading;
            }
            
            delay(10);
            
        }
        
        float tempC = sensors->getTempCByIndex(0);
        
        // Validar lectura
        if (tempC != DEVICE_DISCONNECTED_C && tempC > MIN_VALID_TEMP && tempC < MAX_VALID_TEMP) {
            reading.temperature = tempC;
            reading.valid = true;
            reading.sensor_status = TEMP_STATUS_OK;  
            
            last_reading_time = millis();
            Serial.printf(" Temperatura: %.2f °C (%.0f ms)\n", tempC, millis() - start_time);
        } else {
            reading.temperature = 0.0;
            reading.valid = false;
            reading.sensor_status = TEMP_STATUS_INVALID_READING;  
            
            if (error_logger) {
                error_logger(2, 1, (uint32_t)(tempC * 100)); // ERROR_SENSOR_INVALID_READING, SEVERITY_WARNING
            }
            
            // Revertir incremento 
            if (total_readings_counter) {
                (*total_readings_counter)--;
            }
            
            Serial.printf(" Lectura inválida: %.2f °C\n", tempC);
        }
        
        // Guardar última lectura
        last_reading = reading;
        
        return reading;
    }
    
    /**
     * Verificar si el sensor está inicializado
     */
    bool isInitialized() {
        return initialized;
    }
    
    /**
     * Verificar si la última lectura fue válida
     */
    bool isLastReadingValid() {
        return last_reading.valid;
    }
    
    /**
     * Obtener última temperatura leída
     */
    float getLastTemperature() {
        return last_reading.temperature;
    }
    
    /**
     * Obtener tiempo de última lectura
     */
    uint32_t getLastReadingTime() {
        return last_reading_time;
    }
    
    /**
     * Obtener total de lecturas realizadas
     */
    uint16_t getTotalReadings() {
        return total_readings_counter ? *total_readings_counter : 0;
    }
    
    /**
     * Imprimir información de la última lectura
     */
    void printLastReading() {
        if (last_reading.reading_number == 0) {
            Serial.println(" No hay lecturas previas");
            return;
        }
        
        Serial.println(" --- ÚLTIMA LECTURA TEMPERATURA ---");
        Serial.printf("Lectura #%d\n", last_reading.reading_number);
        Serial.printf("Temperatura: %.2f °C\n", last_reading.temperature);
        Serial.printf("Timestamp: %u ms\n", last_reading.timestamp);
        Serial.printf("Estado: 0x%02X (%s)\n", 
                     last_reading.sensor_status,
                     last_reading.valid ? "VÁLIDA" : "INVÁLIDA");
        Serial.println("---------------------------------------");
    }
    
    /**
     * Verificar si temperatura está en rango válido
     */
    bool isTemperatureInRange(float temp) {
        return (temp > MIN_VALID_TEMP && temp < MAX_VALID_TEMP && !isnan(temp));
    }
    
    /**
     * Configurar puntero al contador de lecturas totales
     */
    void setReadingCounter(uint16_t* total_readings_ptr) {
        total_readings_counter = total_readings_ptr;
    }
    
    /**
     * Configurar función de logging de errores
     */
    void setErrorLogger(void (*log_error_func)(int, int, uint32_t)) {
        error_logger = log_error_func;
    }
    
} // namespace TemperatureSensor