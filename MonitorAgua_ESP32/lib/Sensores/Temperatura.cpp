#include "Temperatura.h"

// ——— Variables internas del módulo ———
namespace TemperatureSensor { // Abre el namespace 'TemperatureSensor' que encapsula todas las variables y funciones del módulo.
    
    // Variables del sensor 
    OneWire* oneWire = nullptr; // Puntero al objeto OneWire que maneja la comunicación 1-Wire con el sensor DS18B20. Inicialmente nulo.
    DallasTemperature* sensors = nullptr; // Puntero al objeto DallasTemperature (wrapper de OneWire) para operaciones del sensor.
    bool initialized = false; // Bandera que indica si el sensor ya fue inicializado (true) o no (false).
    uint32_t last_reading_time = 0; // Marca de tiempo (ms) de la última lectura válida registrada.
    TemperatureReading last_reading = {0}; // Estructura que guarda la última lectura (timestamp, temperatura, flags, etc.), inicializada a cero.
    
    // Punteros a funciones externas
    uint16_t* total_readings_counter = nullptr; // Puntero opcional a un contador global de lecturas (gestionado por el sistema principal).
    void (*error_logger)(int code, int severity, uint32_t context) = nullptr; // Puntero a función para registrar errores en el Watchdog/Logger.
    
    
    
    // ——— Implementación de funciones ———
    
    /**
     * Inicializar sensor de temperatura
     */
    bool initialize(uint8_t pin) { // Función pública que inicializa los objetos OneWire y DallasTemperature usando el pin indicado.
        if (initialized) { // Si ya fue inicializado, evitar reinicializar.
            //Serial.println(" Sensor temperatura ya inicializado");
            return true; // Retornar true porque ya está inicializado.

        }
        
        //Serial.printf(" Inicializando sensor temperatura (pin %d)...\n", pin);
        
        // Crear objetos 
        oneWire = new OneWire(pin); // Reserva dinámicamente un objeto OneWire asociado al pin pasado.
        if (!oneWire) { // Si la asignación falló (puntero nulo)...
            Serial.println(" Error creando OneWire"); // Informar por consola.
            return false; // Retornar false para indicar fallo en inicialización.
        }
        
        sensors = new DallasTemperature(oneWire); // Crea el wrapper DallasTemperature que usa el bus OneWire.
        if (!sensors) { // Si no se pudo crear el wrapper...
            Serial.println(" Error creando DallasTemperature"); // Imprime error.
            delete oneWire;  // Libera el objeto OneWire para evitar fuga de memoria.
            oneWire = nullptr; // Establece puntero a nulo por seguridad.
            return false; // Retorna false por falla.
        }
        
        // Inicializar sensor
        sensors->begin(); // Inicializa internamente la librería DallasTemperature (detecta dispositivos, prepara bus).
        
        initialized = true; // Marca el módulo como inicializado.
        last_reading_time = millis(); // Asigna el tiempo actual (ms) como tiempo de referencia para última lectura.
        
        //Serial.println(" Sensor temperatura inicializado correctamente");
        return true; // Retorna true indicando inicialización exitosa.
    }
    
    /**
     * Limpiar recursos del sensor
     */
    void cleanup() { // Función para liberar recursos y poner el módulo en estado limpio/no inicializado.
        if (sensors) { // Si existe objeto DallasTemperature...
            delete sensors; // Libera el objeto.
            sensors = nullptr; // Evita dangling pointer (puntero colgante).
        }
        if (oneWire) {  // Si existe objeto OneWire...
            delete oneWire; // Libera el objeto.
            oneWire = nullptr; // Evita puntero colgante.
        }
        initialized = false; // Marca el módulo como no inicializado.
        Serial.println(" Sensor temperatura limpiado"); // Imprime mensaje informativo.
    }
    
    /**
     * Tomar lectura con timeout por defecto
     */
    TemperatureReading takeReading() { // Función pública que simplifica la llamada devolviendo la versión con timeout por defecto.
        return takeReadingWithTimeout(); // Llama a la función que implementa timeout (la versión completa).
    }
    
    /**
     * Tomar lectura de temperatura con timeout 
     */
    TemperatureReading takeReadingWithTimeout() { // Función que realiza la lectura real con manejo de errores y timeout.
        TemperatureReading reading = {0}; // Inicializa la estructura de lectura a ceros (timestamp, temperatura, flags...).
        
        if (!initialized || !sensors) { // Comprueba que el módulo esté correctamente inicializado y los objetos existan.
            Serial.println(" Sensor temperatura no inicializado"); // Mensaje de error si no está listo.
            reading.valid = false; // Marca la lectura como inválida.
            reading.sensor_status = TEMP_STATUS_INVALID_READING; // Establece el código de estado indicando error.
            return reading; // Retorna la estructura con invalid flag.
        }
        
        // Incrementar contador 
        if (total_readings_counter) { // Si el sistema principal ha proporcionado un puntero al contador global...
            (*total_readings_counter)++; // Incrementa el contador global en 1.
            reading.reading_number = *total_readings_counter; // Guarda el número de lectura actual en la estructura.
        }
        
        reading.timestamp = millis(); // Registra la marca de tiempo (ms) de inicio de la lectura.
        
        // Timeout para operación del sensor 
        uint32_t start_time = millis(); // Marca el tiempo de inicio para comprobar timeout luego.
        
        sensors->requestTemperatures(); // Instruye al sensor a iniciar la conversión de temperatura (inicio de conversión).
        
        // Verificar timeout 
        while (!sensors->isConversionComplete()) { // Espera activa hasta que la conversión finalice o exceda el timeout.
            if (millis() - start_time > TEMP_OPERATION_TIMEOUT) { // Si el tiempo transcurrido supera el timeout permitido...
                Serial.println(" Timeout en lectura de sensor"); // Imprime mensaje de timeout.
                
                if (error_logger) { // Si hay función registrada para logging de errores...
                    error_logger(1, 1, millis() - start_time); // ERROR_SENSOR_TIMEOUT, SEVERITY_WARNING
                    // Llama al logger con código 1 (timeout) y severidad 1.
                }
                
                reading.valid = false; // Marca la lectura como inválida por timeout.
                reading.sensor_status = TEMP_STATUS_TIMEOUT;   // Establece el estado de timeout.
                
                if (total_readings_counter) { // Si se incrementó el contador al inicio...
                    (*total_readings_counter)--; // Revertir el incremento para no contar lecturas fallidas.
                }
                
                last_reading = reading; // Guarda la lectura (inválida) como última lectura para trazabilidad.
                return reading; // Retorna la lectura inválida por timeout.
            }
            
            delay(10); // Espera 10 ms antes de volver a comprobar isConversionComplete() (reduce CPU busy-wait).
            
        }
        
        float tempC = sensors->getTempCByIndex(0); // Obtiene la temperatura del primer sensor en el bus (índice 0).
        
        // Validar lectura
        if (tempC != DEVICE_DISCONNECTED_C && tempC > MIN_VALID_TEMP && tempC < MAX_VALID_TEMP) {
            // Comprueba que el sensor no esté desconectado y que la temperatura esté dentro de límites razonables.
            reading.temperature = tempC; // Almacena la temperatura medida en la estructura.
            reading.valid = true; // Marca la lectura como válida.
            reading.sensor_status = TEMP_STATUS_OK; // Estado OK.
            
            last_reading_time = millis(); // Actualiza la marca de tiempo del último dato válido.
            Serial.printf(" Temperatura: %.2f °C (%.0f ms)\n", tempC, millis() - start_time);
            // Imprime la temperatura con 2 decimales y el tiempo que tardó la operación.
        } else {
            reading.temperature = 0.0; // En caso de lectura inválida, pone temperatura a 0.0 para indicar fallo.
            reading.valid = false; // Marca lectura inválida.
            reading.sensor_status = TEMP_STATUS_INVALID_READING;   // Estado de lectura inválida.
            
            if (error_logger) { // Si hay un logger definido...
                error_logger(2, 1, (uint32_t)(tempC * 100)); // ERROR_SENSOR_INVALID_READING, SEVERITY_WARNING
                // Reporta error tipo 2 (lectura inválida), severidad 1, contexto: tempC*100.
            }
            
            // Revertir incremento 
            if (total_readings_counter) { // Si se incrementó el contador antes...
                (*total_readings_counter)--; // Revierte el incremento porque la lectura no es válida.
            }
            
            Serial.printf(" Lectura inválida: %.2f °C\n", tempC); // Imprime por consola la lectura inválida para diagnóstico.
        }
        
        // Guardar última lectura
        last_reading = reading; // Guarda la estructura 'reading' en la variable global 'last_reading' para consulta posterior.
        
        return reading; // Devuelve la lectura (válida o inválida) al llamador.
    }
    
    /**
     * Verificar si el sensor está inicializado
     */
    bool isInitialized() { // Función que indica si el módulo fue inicializado correctamente.
        return initialized; // Devuelve la bandera 'initialized'.
    }
    
    /**
     * Verificar si la última lectura fue válida
     */
    bool isLastReadingValid() { // Indica si la última lectura registrada fue considerada válida.
        return last_reading.valid; // Lee el campo 'valid' de la estructura 'last_reading'.
    }
    
    /**
     * Obtener última temperatura leída
     */
    float getLastTemperature() { // Devuelve la última temperatura registrada por el sensor.
        return last_reading.temperature; // Retorna el valor almacenado en 'last_reading.temperature'.
    }
    
    /**
     * Obtener tiempo de última lectura
     */
    uint32_t getLastReadingTime() { // Devuelve el timestamp (ms) de la última lectura válida.
        return last_reading_time; // Retorna la variable 'last_reading_time'.
    }
    
    /**
     * Obtener total de lecturas realizadas
     */
    uint16_t getTotalReadings() { // Devuelve el total de lecturas acumuladas si existe el contador global.
        return total_readings_counter ? *total_readings_counter : 0;
        // Si 'total_readings_counter' es distinto de nulo devuelve su valor; si no, devuelve 0.
    }
    
    /**
     * Imprimir información de la última lectura
     */
    void printLastReading() { // Función utilitaria para imprimir por consola los detalles de la última lectura.
        if (last_reading.reading_number == 0) { // Función utilitaria para imprimir por consola los detalles de la última lectura.
            Serial.println(" No hay lecturas previas");
            return; // Sale de la función.
        }
        
        Serial.println(" --- ÚLTIMA LECTURA TEMPERATURA ---");
        Serial.printf("Lectura #%d\n", last_reading.reading_number); // Imprime el número de lectura.
        Serial.printf("Temperatura: %.2f °C\n", last_reading.temperature); // Imprime temperatura
        Serial.printf("Timestamp: %u ms\n", last_reading.timestamp); // Imprime el timestamp (ms).
        Serial.printf("Estado: 0x%02X (%s)\n", 
                     last_reading.sensor_status,
                     last_reading.valid ? "VÁLIDA" : "INVÁLIDA");
                     // Imprime el estado en hex y su interpretación (VÁLIDA o INVÁLIDA).
        Serial.println("---------------------------------------");
    }
    
    /**
     * Verificar si temperatura está en rango válido
     */
    bool isTemperatureInRange(float temp) { // Comprueba si una temperatura dada está dentro de los límites permitidos.
        return (temp > MIN_VALID_TEMP && temp < MAX_VALID_TEMP && !isnan(temp));
        // Devuelve true si temp está entre MIN_VALID_TEMP y MAX_VALID_TEMP y no es NaN.
    }
    
    /**
     * Configurar puntero al contador de lecturas totales
     */
    void setReadingCounter(uint16_t* total_readings_ptr) { // Permite al sistema principal pasar un puntero al contador global.
        total_readings_counter = total_readings_ptr; // Asigna el puntero al campo interno 'total_readings_counter'.
    }
    
    /**
     * Configurar función de logging de errores
     */
    void setErrorLogger(void (*log_error_func)(int, int, uint32_t)) { // Permite registrar el callback para logging de errores.
        error_logger = log_error_func; // Guarda el puntero a la función de logging en 'error_logger'.
    }
    
} // namespace TemperatureSensor 