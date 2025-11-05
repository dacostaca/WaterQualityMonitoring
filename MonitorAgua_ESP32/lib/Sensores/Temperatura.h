/**
 * @file Temperatura.h
 * @brief Definición del módulo de sensor de temperatura DS18B20 para ESP32
 * @details Este header contiene todas las definiciones, estructuras, constantes y
 *          prototipos de funciones para el manejo del sensor digital de temperatura
 *          DS18B20 mediante protocolo OneWire. Incluye control de timeout, validación
 *          de rangos y configuración de intervalos de muestreo.
 * @author Daniel Acosta - Santiago Erazo
 * @date 01/10/2025
 * @version 1.0
 */

 #ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ——— Configuración del sensor de temperatura  ———

/**
 * @def TEMP_SENSOR_PIN
 * @brief Pin GPIO del ESP32 para comunicación OneWire con el DS18B20
 * @details Debe configurarse con resistencia pull-up externa de 4.7kΩ a Vcc.
 *          GPIO25 es típicamente usado en módulos de desarrollo ESP32.
 * @note El protocolo OneWire permite múltiples sensores en el mismo pin (bus compartido).
 */
#define TEMP_SENSOR_PIN         25   

/**
 * @def TEMP_OPERATION_TIMEOUT
 * @brief Timeout máximo para operación completa de lectura en milisegundos
 * @details Si la función takeReadingWithTimeout() excede este tiempo durante la
 *          conversión del DS18B20, retorna error de timeout (TEMP_STATUS_TIMEOUT).
 * @note 5000 ms permite completar conversión con margen (DS18B20 típicamente 750ms @ 12bits).
 * @warning Si usa resolución reducida (9-11 bits), el timeout puede reducirse proporcionalmente.
 */
#define TEMP_OPERATION_TIMEOUT  5000  

// >>> CAMBIO: Intervalo de muestreo configurable (ms)
/**
 * @var TEMP_INTERVAL_MS
 * @brief Intervalo sugerido entre lecturas completas de temperatura en milisegundos
 * @details 🔹 LÍNEA CRÍTICA PARA CONFIGURACIÓN DE TIEMPO DE MUESTREO 🔹
 *          Define cada cuánto tiempo el sistema debe solicitar una nueva lectura
 *          completa del sensor DS18B20. Solo cambiar este valor para ajustar
 *          frecuencia de monitoreo.
 * @note Valor actual: 10000 ms = 10 segundos. Ajustar según:
 *       - Monitoreo continuo: 5000-15000 ms (5-15 segundos)
 *       - Sistemas de bajo consumo: 60000-300000 ms (1-5 minutos)
 *       - Aplicaciones críticas: 1000-5000 ms (1-5 segundos)
 * @warning El DS18B20 tiene inercia térmica. Lecturas muy frecuentes (<1s) no aportan
 *          información adicional debido a constante de tiempo del sensor (~5-10s).
 */
const unsigned long TEMP_INTERVAL_MS = 10000UL; // >>> Línea para modificar el tiempo de lectura del sensor

/**
 * @var TEMP_MIN_SAMPLE_SPACING_MS
 * @brief Espaciado mínimo recomendado entre muestras individuales en milisegundos
 * @details Evita saturación del bus OneWire con solicitudes excesivamente rápidas.
 *          Aunque el DS18B20 puede responder más rápido, respetar este spacing mejora
 *          estabilidad en sistemas con múltiples sensores o buses ruidosos.
 * @note Valor típico: 20 ms. Puede ajustarse para aplicaciones específicas.
 */
const unsigned long TEMP_MIN_SAMPLE_SPACING_MS = 20UL; 

// ——— Estructura de datos del sensor ———

/**
 * @struct TemperatureReading
 * @brief Estructura empaquetada que representa una lectura completa del sensor DS18B20
 * @details Contiene todos los datos relevantes de una medición: temperatura en °C,
 *          timestamp, estado de validez y códigos de error. Empaquetada con
 *          __attribute__((packed)) para optimizar memoria en almacenamiento persistente
 *          (RTC Memory, EEPROM, etc.).
 * 
 * @note Tamaño aproximado: 11 bytes (packed)
 * @note El DS18B20 proporciona resolución configurable: 9-12 bits (0.5°C - 0.0625°C).
 *       Por defecto librería DallasTemperature usa 12 bits (0.0625°C).
 */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;         // Tiempo en milisegundos
    float temperature;          // Temperatura en °C
    uint16_t reading_number;    // Número de lectura
    uint8_t sensor_status;      // Estado del sensor (flags)
    bool valid;                 // Indica si la lectura es válida
} TemperatureReading;


// ——— Códigos de estado del sensor  ———
/**
 * @def TEMP_STATUS_OK
 * @brief Código de estado: lectura exitosa sin errores
 * @details Todos los bits en 0. Indica que la lectura se completó correctamente,
 *          sensor está conectado y temperatura está dentro de rango válido.
 */
#define TEMP_STATUS_OK              0x00  // Sin errores

/**
 * @def TEMP_STATUS_TIMEOUT
 * @brief Flag de timeout: conversión del DS18B20 excedió TEMP_OPERATION_TIMEOUT
 * @details Bit 0. Indica que takeReadingWithTimeout() no recibió respuesta del sensor
 *          en el tiempo asignado. Posibles causas:
 *          - Sensor desconectado o cable roto
 *          - Resistencia pull-up faltante o incorrecta (requiere 4.7kΩ)
 *          - Interferencia electromagnética en el bus OneWire
 *          - Sensor dañado o en cortocircuito
 */
#define TEMP_STATUS_TIMEOUT         0x01  // Flag de timeout

/**
 * @def TEMP_STATUS_INVALID_READING
 * @brief Flag de lectura inválida: datos inconsistentes o sensor desconectado
 * @details Bit 1. Indica problemas genéricos en la lectura:
 *          - Sensor no inicializado (punteros nullptr)
 *          - Temperatura fuera de rango válido (MIN_VALID_TEMP - MAX_VALID_TEMP)
 *          - Lectura DEVICE_DISCONNECTED_C (-127°C) indicando desconexión física
 *          - Valores NaN por error de comunicación OneWire
 */
#define TEMP_STATUS_INVALID_READING 0x02  // Flag de lectura inválida

// ——— Namespace para el sensor de temperatura  ———

/**
 * @namespace TemperatureSensor
 * @brief Espacio de nombres para todas las funcionalidades del sensor de temperatura
 * @details Encapsula variables, constantes y funciones relacionadas con el sensor DS18B20.
 *          Evita contaminación del namespace global y facilita integración modular.
 */
namespace TemperatureSensor {
    
    // Constantes internas 

    /**
     * @brief Temperatura mínima válida en °C
     * @details Límite inferior del rango de medición para aplicaciones de monitoreo de agua.
     *          -50°C es un margen conservador (DS18B20 soporta hasta -55°C).
     * @note Ajustar según aplicación específica:
     *       - Agua potable: 0°C a 40°C típico
     *       - Procesos industriales: -20°C a 85°C
     *       - Aplicaciones extremas: -50°C a 85°C
     */
    constexpr float MIN_VALID_TEMP = -50.0;

    /**
     * @brief Temperatura máxima válida en °C
     * @details Límite superior del rango de medición para aplicaciones de monitoreo de agua.
     *          85°C es un margen conservador (DS18B20 soporta hasta +125°C).
     * @note Ajustar según aplicación específica:
     *       - Agua potable: 0°C a 40°C típico
     *       - Agua caliente sanitaria: 40°C a 85°C
     *       - Procesos industriales: hasta 125°C (límite del DS18B20)
     * @warning Por encima de 85°C considerar sensores de alta temperatura (PT100, termopares).
     */
    constexpr float MAX_VALID_TEMP = 85.0;


    // ——— Funciones principales ———

    /**
     * @brief Inicializa el sensor de temperatura DS18B20 en el pin especificado
     * @details Crea dinámicamente objetos OneWire y DallasTemperature, configura el bus
     *          1-Wire y prepara el sensor para lecturas. Es seguro llamar múltiples veces
     *          (verifica si ya está inicializado). Gestiona memoria dinámicamente.
     * @param pin Pin GPIO del ESP32 para comunicación OneWire (por defecto TEMP_SENSOR_PIN)
     * @return true si inicialización exitosa o ya estaba inicializado, false si error
     * @note Requiere llamarse una vez en setup() antes de usar otras funciones.
     * @warning Si falla la creación de objetos, libera memoria automáticamente y retorna false.
     * @note El sensor DS18B20 requiere resistencia pull-up externa de 4.7kΩ en el bus OneWire.
     *       Sin pull-up, el sensor no funcionará correctamente (lecturas erróneas o timeouts).
     */
    bool initialize(uint8_t pin = TEMP_SENSOR_PIN);

    /**
     * @brief Limpia y libera recursos del sensor de temperatura
     * @details Elimina dinámicamente los objetos DallasTemperature y OneWire, liberando
     *          memoria heap. Marca el sensor como no inicializado.
     * @note Es seguro llamar aunque no esté inicializado (verifica punteros antes de liberar).
     * @note Útil para reset de sistema, cambio de configuración o liberación de recursos.
     */
    void cleanup();

    /**
     * @brief Realiza una lectura de temperatura (wrapper de takeReadingWithTimeout)
     * @details Función de conveniencia que llama internamente a takeReadingWithTimeout().
     * @return Estructura TemperatureReading con resultado completo de la medición
     * @see takeReadingWithTimeout() para detalles completos del proceso de lectura
     */
    TemperatureReading takeReading();

    /**
     * @brief Realiza lectura de temperatura con control de timeout y validación exhaustiva
     * @details Proceso completo:
     *          1. Verifica inicialización del sensor y punteros válidos
     *          2. Incrementa contador global de lecturas
     *          3. Solicita conversión de temperatura al DS18B20 (requestTemperatures)
     *          4. Espera completitud de conversión con polling de 10ms
     *          5. Verifica timeout de operación (< TEMP_OPERATION_TIMEOUT)
     *          6. Lee temperatura en °C usando getTempCByIndex(0)
     *          7. Valida rango y detecta desconexión (DEVICE_DISCONNECTED_C = -127°C)
     *          8. Actualiza last_reading y registra errores si corresponde
     * @return Estructura TemperatureReading con campos:
     *         - temperature: Temperatura en °C (0.0 si inválida)
     *         - timestamp: millis() al momento de la lectura
     *         - reading_number: Número secuencial de lectura
     *         - valid: true si lectura válida y dentro de rangos
     *         - sensor_status: Código bit-field de estado (ver TEMP_STATUS_*)
     * @note Si hay timeout o valores fuera de rango, decrementa el contador global.
     * @note La conversión del DS18B20 tarda típicamente:
     *       - 9 bits (0.5°C): ~94 ms
     *       - 10 bits (0.25°C): ~188 ms
     *       - 11 bits (0.125°C): ~375 ms
     *       - 12 bits (0.0625°C): ~750 ms (por defecto)
     * @warning Función bloqueante durante conversión + polling (típicamente <1 segundo).
     *          Considerar ejecutar en tarea separada para sistemas de tiempo real.
     */
    TemperatureReading takeReadingWithTimeout();
    
    // ——— Funciones de estado ———

    /**
     * @brief Consulta si el sensor está inicializado y listo para uso
     * @return true si initialize() fue llamado exitosamente
     */
    bool isInitialized();

    /**
     * @brief Consulta si la última lectura almacenada es válida
     * @return true si last_reading.valid es true
     */
    bool isLastReadingValid();

    /**
     * @brief Obtiene el valor de temperatura de la última lectura almacenada
     * @return Temperatura en °C (0.0 si última lectura fue inválida)
     */
    float getLastTemperature();

    /**
     * @brief Obtiene timestamp de la última lectura válida realizada
     * @return millis() del momento de última lectura exitosa
     */
    uint32_t getLastReadingTime();

    /**
     * @brief Obtiene el total de lecturas realizadas desde el contador global
     * @return Número total de lecturas o 0 si contador no está vinculado
     */
    uint16_t getTotalReadings();
    
    // ——— Funciones de utilidad ———

    /**
     * @brief Imprime por Serial la última lectura almacenada en formato estructurado
     * @details Muestra: número de lectura, temperatura, timestamp, estado de validez.
     *          Útil para depuración y monitoreo en tiempo real.
     * @note Si no hay lecturas previas (reading_number == 0), informa al usuario.
     */
    void printLastReading();

    /**
     * @brief Valida si una temperatura está dentro del rango aceptable
     * @param temp Temperatura a validar en °C
     * @return true si temp está entre MIN_VALID_TEMP (-50°C) y MAX_VALID_TEMP (85°C) y no es NaN
     * @note Rango basado en especificaciones del sensor DS18B20 (-55°C a +125°C),
     *       ajustado a rangos prácticos para aplicaciones de monitoreo de agua.
     */
    bool isTemperatureInRange(float temp);
    
    // ——— Funciones para integración con sistema principal ———

    /**
     * @brief Vincula el sensor con un contador global de lecturas del sistema
     * @details Permite que el módulo de temperatura incremente automáticamente un contador
     *          externo en cada lectura válida. Útil para estadísticas globales del sistema.
     * @param total_readings_ptr Puntero a uint16_t que será incrementado en cada lectura válida
     * @note El puntero debe apuntar a memoria válida durante toda la vida útil del sensor.
     * @warning No pasar punteros a variables locales que puedan salir de scope.
     */
    void setReadingCounter(uint16_t* total_readings_ptr);

    /**
     * @brief Vincula el sensor con un sistema de logging de errores externo
     * @details Permite que el módulo de temperatura reporte errores (timeout, lectura inválida,
     *          etc.) a un sistema centralizado de gestión de errores o logger.
     * @param log_error_func Puntero a función con firma: void(int code, int severity, uint32_t context)
     *        - code: Código de error (1=timeout, 2=lectura inválida, etc.)
     *        - severity: Nivel de severidad (1=warning, 2=error, 3=crítico, etc.)
     *        - context: Información contextual (tiempo transcurrido, temperatura*100, etc.)
     * @note La función debe ser thread-safe si se usa en entorno multitarea (FreeRTOS).
     * @warning No pasar punteros a funciones lambda sin captura estática.
     */
    void setErrorLogger(void (*log_error_func)(int, int, uint32_t));
    
    // ——— Variables internas del módulo (declaraciones externas) ———

    /**
     * @brief Puntero al objeto OneWire para comunicación con el bus 1-Wire
     * @details Gestiona el protocolo de comunicación de bajo nivel con el sensor DS18B20.
     *          Se crea dinámicamente en initialize() y se libera en cleanup().
     * @note nullptr cuando no está inicializado. Verificar antes de usar.
     */
    extern OneWire* oneWire;

    /**
     * @brief Puntero al objeto DallasTemperature para gestión del sensor DS18B20
     * @details Proporciona API de alto nivel para solicitar y leer temperaturas del sensor.
     *          Se crea dinámicamente en initialize() sobre oneWire.
     * @note nullptr cuando no está inicializado. Verificar antes de usar.
     */
    extern DallasTemperature* sensors;

    /**
     * @brief Bandera de estado de inicialización del sensor
     * @details Indica si initialize() fue llamado exitosamente y los objetos OneWire
     *          y DallasTemperature fueron creados correctamente.
     */
    extern bool initialized;

    /**
     * @brief Timestamp de la última lectura válida realizada
     * @details Almacena millis() del momento de última lectura exitosa. Útil para
     *          calcular intervalos entre mediciones o detectar fallas prolongadas.
     */
    extern uint32_t last_reading_time;

    /**
     * @brief Última estructura de lectura capturada por el sensor
     * @details Contiene resultado completo de última llamada a takeReadingWithTimeout().
     *          Accesible mediante getLastTemperature(), etc.
     */
    extern TemperatureReading last_reading;

    /**
     * @brief Puntero al contador global de lecturas del sistema
     * @details Configurado mediante setReadingCounter(). nullptr si no está vinculado.
     */
    extern uint16_t* total_readings_counter;

    /**
     * @brief Puntero a función de logging de errores del sistema
     * @details Configurado mediante setErrorLogger(). nullptr si no está vinculado.
     */
    extern void (*error_logger)(int code, int severity, uint32_t context);
}

#endif // TEMPERATURE_SENSOR_H