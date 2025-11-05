/**
 * @file Temperatura.cpp
 * @brief Implementación del sensor de temperatura DS18B20 para ESP32
 * @details Este archivo contiene la lógica completa para inicialización, lectura
 *          y validación del sensor digital de temperatura DS18B20 usando protocolo
 *          OneWire. Implementa control de timeout, validación de rangos y gestión
 *          dinámica de memoria para los objetos de comunicación.
 * @author Daniel Acosta - Santiago Erazo
 * @date 01/10/2025
 * @version 1.0
 */

#include "Temperatura.h"



/**
 * @namespace TemperatureSensor
 * @brief Espacio de nombres para todas las funcionalidades del sensor de temperatura
 * @details Contiene variables globales internas, funciones de lectura, validación
 *          y utilidades para manejo completo del sensor DS18B20 conectado mediante
 *          protocolo OneWire al ESP32.
 */
namespace TemperatureSensor {

    // ——— Variables internas del módulo ———
    
    /**
     * @brief Puntero al objeto OneWire para comunicación con el bus 1-Wire
     * @details Gestiona el protocolo de comunicación de bajo nivel con el sensor DS18B20.
     *          Se crea dinámicamente en initialize() y se libera en cleanup().
     * @note nullptr cuando no está inicializado. Verificar antes de usar.
     */ 
    OneWire* oneWire = nullptr;

    /**
     * @brief Puntero al objeto DallasTemperature para gestión del sensor DS18B20
     * @details Proporciona API de alto nivel para solicitar y leer temperaturas del
     *          sensor DS18B20. Se crea dinámicamente en initialize() sobre oneWire.
     * @note nullptr cuando no está inicializado. Verificar antes de usar.
     */
    DallasTemperature* sensors = nullptr;

    /**
     * @brief Bandera de estado de inicialización del sensor de temperatura
     * @details Indica si initialize() fue llamado exitosamente y los objetos OneWire
     *          y DallasTemperature fueron creados correctamente.
     */
    bool initialized = false;

    /**
     * @brief Timestamp de la última lectura válida realizada
     * @details Almacena millis() del momento en que se completó exitosamente una
     *          lectura. Útil para calcular intervalos entre mediciones.
     */
    uint32_t last_reading_time = 0;

    /**
     * @brief Última estructura de lectura capturada por el sensor
     * @details Contiene temperatura, timestamp, estado y número de lectura.
     *          Se actualiza en cada llamada a takeReadingWithTimeout().
     */
    TemperatureReading last_reading = {0};
    
    // Punteros a funciones externas

    /**
     * @brief Puntero al contador global de lecturas del sistema
     * @details Permite incrementar un contador externo cada vez que se realiza una
     *          lectura válida. nullptr si no se ha vinculado con sistema principal.
     */
    uint16_t* total_readings_counter = nullptr;

    /**
     * @brief Puntero a función de logging de errores del sistema
     * @details Callback para reportar errores (timeout, lectura inválida, etc.) al
     *          sistema principal. nullptr si no está configurado.
     */
    void (*error_logger)(int code, int severity, uint32_t context) = nullptr;
    
    
    
    // ——— Implementación de funciones ———
    
    /**
     * @brief Inicializa el sensor de temperatura DS18B20 en el pin especificado
     * @details Crea dinámicamente objetos OneWire y DallasTemperature, configura el
     *          bus 1-Wire y prepara el sensor para lecturas. Es seguro llamar múltiples
     *          veces (verifica si ya está inicializado). Gestiona memoria dinámicamente.
     * @param pin Pin GPIO del ESP32 para comunicación OneWire (por defecto TEMP_SENSOR_PIN)
     * @return true si inicialización exitosa o ya estaba inicializado, false si error
     * @note Requiere llamarse una vez en setup() antes de usar otras funciones.
     * @warning Si falla la creación de objetos, libera memoria automáticamente y retorna false.
     * @note El sensor DS18B20 requiere resistencia pull-up de 4.7kΩ en el bus OneWire.
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
     * @brief Limpia y libera recursos del sensor de temperatura
     * @details Elimina dinámicamente los objetos DallasTemperature y OneWire,
     *          liberando memoria heap. Marca el sensor como no inicializado.
     * @note Es seguro llamar aunque no esté inicializado (verifica punteros).
     * @note Útil para reset de sistema o cambio de configuración.
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
     * @brief Realiza una lectura de temperatura (wrapper de takeReadingWithTimeout)
     * @details Función de conveniencia que llama internamente a takeReadingWithTimeout().
     * @return Estructura TemperatureReading con resultado completo de la medición
     * @see takeReadingWithTimeout() para detalles completos del proceso de lectura
     */
    TemperatureReading takeReading() {
        return takeReadingWithTimeout();
    }
    
    /**
     * @brief Realiza lectura de temperatura con control de timeout y validación exhaustiva
     * @details Función principal para toma de datos. Proceso completo:
     *          1. Verifica inicialización del sensor y punteros válidos
     *          2. Incrementa contador global de lecturas
     *          3. Solicita conversión de temperatura al DS18B20 (requestTemperatures)
     *          4. Espera completitud de conversión con polling de 10ms
     *          5. Verifica timeout de operación (< TEMP_OPERATION_TIMEOUT)
     *          6. Lee temperatura en °C usando getTempCByIndex(0)
     *          7. Valida rango de temperatura y detecta desconexión (DEVICE_DISCONNECTED_C)
     *          8. Actualiza last_reading y registra errores si corresponde
     * @return Estructura TemperatureReading con campos:
     *         - temperature: Temperatura en °C (0.0 si inválida)
     *         - timestamp: millis() al momento de la lectura
     *         - reading_number: Número secuencial de lectura
     *         - valid: true si lectura válida y dentro de rangos
     *         - sensor_status: Código bit-field de estado (ver TEMP_STATUS_*)
     * @note Si hay timeout o valores fuera de rango, decrementa el contador global.
     * @note La conversión del DS18B20 tarda típicamente 750ms con resolución de 12 bits.
     * @warning Función bloqueante durante conversión + polling (típicamente <1 segundo).
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
     * @brief Consulta si el sensor está inicializado y listo para uso
     * @return true si initialize() fue llamado exitosamente
     */
    bool isInitialized() {
        return initialized;
    }
    
    /**
     * @brief Consulta validez de la última lectura almacenada
     * @return true si last_reading.valid es true
     */
    bool isLastReadingValid() {
        return last_reading.valid;
    }
    
    /**
     * @brief Obtiene el valor de temperatura de la última lectura
     * @return Temperatura en °C (0.0 si última lectura fue inválida)
     */
    float getLastTemperature() {
        return last_reading.temperature;
    }
    
    /**
     * @brief Obtiene timestamp de la última lectura válida
     * @return millis() del momento de última lectura exitosa
     */
    uint32_t getLastReadingTime() {
        return last_reading_time;
    }
    
    /**
     * @brief Obtiene el total de lecturas realizadas desde el contador global
     * @return Número total de lecturas o 0 si contador no está vinculado
     */
    uint16_t getTotalReadings() {
        return total_readings_counter ? *total_readings_counter : 0;
    }
    
    /**
     * @brief Imprime por Serial la última lectura almacenada en formato estructurado
     * @details Muestra: número de lectura, temperatura, timestamp, estado de validez.
     *          Útil para depuración y monitoreo en tiempo real.
     * @note Si no hay lecturas previas (reading_number == 0), informa al usuario.
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
     * @brief Valida si una temperatura está dentro del rango aceptable
     * @param temp Temperatura a validar en °C
     * @return true si temp está entre MIN_VALID_TEMP (-50°C) y MAX_VALID_TEMP (85°C) y no es NaN
     * @note Rango basado en especificaciones del sensor DS18B20 (-55°C a +125°C),
     *       ajustado a rangos prácticos para aplicaciones de monitoreo de agua.
     */
    bool isTemperatureInRange(float temp) {
        return (temp > MIN_VALID_TEMP && temp < MAX_VALID_TEMP && !isnan(temp));
    }
    
    /**
     * @brief Vincula el sensor con un contador global de lecturas del sistema
     * @details Permite que el módulo de temperatura incremente automáticamente un
     *          contador externo en cada lectura válida. Útil para estadísticas globales.
     * @param total_readings_ptr Puntero a uint16_t que será incrementado en cada lectura válida
     * @note El puntero debe apuntar a memoria válida durante toda la vida útil del sensor.
     * @warning No pasar punteros a variables locales que puedan salir de scope.
     */
    void setReadingCounter(uint16_t* total_readings_ptr) {
        total_readings_counter = total_readings_ptr;
    }
    
    /**
     * @brief Vincula el sensor con un sistema de logging de errores externo
     * @details Permite que el módulo de temperatura reporte errores (timeout, lectura
     *          inválida, etc.) a un sistema centralizado de gestión de errores o logger.
     * @param log_error_func Puntero a función con firma: void(int code, int severity, uint32_t context)
     *        - code: Código de error (1=timeout, 2=lectura inválida, etc.)
     *        - severity: Nivel de severidad (1=warning, 2=error, 3=crítico, etc.)
     *        - context: Información contextual (tiempo transcurrido, temperatura*100, etc.)
     * @note La función debe ser thread-safe si se usa en entorno multitarea (FreeRTOS).
     * @warning No pasar punteros a funciones lambda sin captura estática.
     */
    void setErrorLogger(void (*log_error_func)(int, int, uint32_t)) {
        error_logger = log_error_func;
    }
    
} // namespace TemperatureSensor