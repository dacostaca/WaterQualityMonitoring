#ifndef WATCHDOG_MANAGER_H
#define WATCHDOG_MANAGER_H

#include <Arduino.h>
#include "esp_task_wdt.h"
#include "esp_system.h"
#include <string.h>

/**
 * @brief Clase para manejo completo de Watchdog y Health Monitoring en ESP32
 */
class WatchdogManager {
public:
    // ——— Códigos de Error ———
    typedef enum {
        ERROR_NONE = 0,
        ERROR_SENSOR_TIMEOUT = 1,
        ERROR_SENSOR_INVALID_READING = 2,
        ERROR_RTC_CORRUPTION = 3,
        ERROR_RTC_WRITE_FAIL = 4,
        ERROR_MEMORY_FULL = 5,
        ERROR_WDT_TIMEOUT = 6,
        ERROR_SYSTEM_PANIC = 7,
        ERROR_CRC_MISMATCH = 8,
        ERROR_WIFI_FAIL = 9,
        ERROR_SENSOR_INIT_FAIL = 10,
        ERROR_MEMORY_LOW = 11,
        ERROR_TIMING_ISSUE = 12
    } error_code_t;

    // ——— Severidad de Errores ———
    typedef enum {
        SEVERITY_INFO = 0,
        SEVERITY_WARNING = 1,
        SEVERITY_CRITICAL = 2
    } error_severity_t;

    // ——— Estructura para logging de errores ———
    typedef struct __attribute__((packed)) {
        uint8_t error_code;         // Código de error
        uint8_t severity;           // Severidad del error
        uint16_t timestamp_min;     // Timestamp relativo en minutos
        uint8_t context[4];         // Contexto adicional del error
    } ErrorEntry;

    // ——— Callbacks ———
    typedef void (*ErrorCallback)(error_code_t code, error_severity_t severity, uint32_t context);
    typedef void (*LogCallback)(const char* message);

    // ——— Constantes ———
    static const int MAX_CRITICAL_ERRORS = 8;
    static const int MAX_WARNING_ERRORS = 16;
    static const int MAX_INFO_ERRORS = 32;
    static const uint32_t MAX_CONSECUTIVE_FAILURES = 10;

private:
    // ——— Variables privadas ———
    bool _enableSerialOutput;
    LogCallback _logCallback;
    ErrorCallback _errorCallback;
    uint32_t _lastHealthCheck;
    bool _watchdogInitialized;

public:
    /**
     * @brief Constructor de WatchdogManager
     * @param enableSerial Habilitar salida por Serial (default: true)
     */
    WatchdogManager(bool enableSerial = true);
    
    /**
     * @brief Inicializar el sistema de watchdog y monitoreo
     */
    void begin();
    
    /**
     * @brief Alimentar el watchdog hardware
     */
    void feedWatchdog();
    
    /**
     * @brief Registrar un error en el sistema
     */
    void logError(error_code_t code, error_severity_t severity, uint32_t context = 0);
    
    /**
     * @brief Realizar verificación completa de salud del sistema
     */
    bool performHealthCheck();
    
    /**
     * @brief Registrar operación exitosa
     */
    void recordSuccess();
    
    /**
     * @brief Registrar fallo en operación
     */
    void recordFailure();
    
    /**
     * @brief Verificar si hay fallos críticos
     */
    bool hasCriticalFailures();
    
    /**
     * @brief Obtener score de salud del sistema (0-100)
     */
    uint32_t getHealthScore();
    
    /**
     * @brief Obtener número de fallos consecutivos
     */
    uint32_t getConsecutiveFailures();
    
    /**
     * @brief Intentar recuperación automática del sistema
     */
    bool attemptRecovery();
    
    /**
     * @brief Manejar emergencia del sistema
     */
    void handleEmergency();
    
    /**
     * @brief Mostrar log de errores almacenados
     */
    void displayErrorLog(int maxErrors = 3);
    
    /**
     * @brief Mostrar estado de salud del sistema
     */
    void displaySystemHealth();
    
    /**
     * @brief Configurar callback para logging personalizado
     */
    void setLogCallback(LogCallback callback);
    
    /**
     * @brief Configurar callback para notificación de errores
     */
    void setErrorCallback(ErrorCallback callback);
    
    /**
     * @brief Habilitar/deshabilitar salida por Serial
     */
    void enableSerial(bool enable);
    
    /**
     * @brief Verificar si el watchdog está funcionando
     */
    bool isWatchdogHealthy();

private:
    /**
     * @brief Inicializar watchdog hardware del ESP32
     */
    bool initializeHardwareWatchdog();
    
    /**
     * @brief Verificar memoria disponible del sistema
     */
    bool checkMemoryHealth();
    
    /**
     * @brief Verificar tiempo desde última operación exitosa
     */
    bool checkTimingHealth();
    
    /**
     * @brief Enviar mensaje de log
     */
    void log(const char* message);
    
    /**
     * @brief Enviar mensaje de log con formato
     */
    void logf(const char* format, ...);
};

// ——— Variables RTC para persistencia ———
extern RTC_DATA_ATTR uint32_t wdt_system_health_score;
extern RTC_DATA_ATTR uint32_t wdt_consecutive_failures;
extern RTC_DATA_ATTR uint32_t wdt_last_successful_operation;
extern RTC_DATA_ATTR uint16_t wdt_total_errors;
extern RTC_DATA_ATTR WatchdogManager::ErrorEntry wdt_critical_errors[8];
extern RTC_DATA_ATTR WatchdogManager::ErrorEntry wdt_warning_errors[16];
extern RTC_DATA_ATTR WatchdogManager::ErrorEntry wdt_info_errors[32];

#endif // WATCHDOG_MANAGER_H