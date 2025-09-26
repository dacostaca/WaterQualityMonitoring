#include "WatchDogManager.h"
#include <stdarg.h>

// ——— Variables RTC persistentes al deep sleep ———
RTC_DATA_ATTR uint32_t wdt_system_health_score = 100;
RTC_DATA_ATTR uint32_t wdt_consecutive_failures = 0;
RTC_DATA_ATTR uint32_t wdt_last_successful_operation = 0;
RTC_DATA_ATTR uint16_t wdt_total_errors = 0;
RTC_DATA_ATTR WatchdogManager::ErrorEntry wdt_critical_errors[WatchdogManager::MAX_CRITICAL_ERRORS];
RTC_DATA_ATTR WatchdogManager::ErrorEntry wdt_warning_errors[WatchdogManager::MAX_WARNING_ERRORS];
RTC_DATA_ATTR WatchdogManager::ErrorEntry wdt_info_errors[WatchdogManager::MAX_INFO_ERRORS];

// Variable para detectar modo de watchdog
static bool hardware_watchdog_available = false;

// Constructor
WatchdogManager::WatchdogManager(bool enableSerial) 
    : _enableSerialOutput(enableSerial), _logCallback(nullptr), _errorCallback(nullptr),
      _lastHealthCheck(0), _watchdogInitialized(false) {
}

// Inicialización
void WatchdogManager::begin() {
    if (_enableSerialOutput && !Serial) {
        Serial.begin(115200);
        delay(100);
    }
    
    log("=== Watchdog Manager Inicializado ===");
    
    // Intentar inicializar watchdog
    if (initializeHardwareWatchdog()) {
        _watchdogInitialized = true;
        if (hardware_watchdog_available) {
            log(" Hardware watchdog inicializado");
        } else {
            log(" Watchdog en modo software inicializado");
        }
    } else {
        _watchdogInitialized = false;
        log(" Fallo en inicialización de watchdog");
    }
    
    // Inicializar timestamp si es primera ejecución
    if (wdt_last_successful_operation == 0) {
        wdt_last_successful_operation = millis();
        wdt_system_health_score = 85;
        //log(" Primera ejecución - inicializando variables de salud");
    }
    
    _lastHealthCheck = millis();
    
    logf(" Salud inicial del sistema: %d%%", wdt_system_health_score);
    logf(" Fallos consecutivos: %d", wdt_consecutive_failures);
    logf(" Modo watchdog: %s", hardware_watchdog_available ? "Hardware" : "Software");
}

// Alimentar watchdog
void WatchdogManager::feedWatchdog() {
    if (!_watchdogInitialized) {
        return;
    }
    
    if (hardware_watchdog_available) {
        esp_err_t result = esp_task_wdt_reset();
        if (result != ESP_OK && result != ESP_ERR_NOT_FOUND) {
            hardware_watchdog_available = false;
            log(" Watchdog hardware falló - cambiando a modo software");
        }
    }
}

// Logging de errores
void WatchdogManager::logError(error_code_t code, error_severity_t severity, uint32_t context) {
    //crea entrada de error, guarda según severidad
    //critico borra el más antiguo si buffer lleno
    //warning hace shift FIFO
    //Info simplemente descarta
    //aumenta contador de errores y llama al callback de error si fue configurado
    logf(" Logging error: code=%d, severity=%d, context=%u", code, severity, context);
    
    ErrorEntry error;
    error.error_code = code;
    error.severity = severity;
    error.timestamp_min = millis() / 60000;
    
    error.context[0] = (context >> 24) & 0xFF;
    error.context[1] = (context >> 16) & 0xFF;
    error.context[2] = (context >> 8) & 0xFF;
    error.context[3] = context & 0xFF;
    
    bool stored = false;
    switch (severity) {
        case SEVERITY_CRITICAL:
            for (int i = 0; i < MAX_CRITICAL_ERRORS; i++) {
                if (wdt_critical_errors[i].error_code == ERROR_NONE) {
                    wdt_critical_errors[i] = error;
                    stored = true;
                    break;
                }
            }
            if (!stored) {
                wdt_critical_errors[0] = error;
                stored = true;
                log(" Buffer crítico lleno - sobrescribiendo error más antiguo");
            }
            break;
            
        case SEVERITY_WARNING:
            for (int i = 0; i < MAX_WARNING_ERRORS; i++) {
                if (wdt_warning_errors[i].error_code == ERROR_NONE) {
                    wdt_warning_errors[i] = error;
                    stored = true;
                    break;
                }
            }
            if (!stored) {
                for (int i = 0; i < MAX_WARNING_ERRORS - 1; i++) {
                    wdt_warning_errors[i] = wdt_warning_errors[i + 1];
                }
                wdt_warning_errors[MAX_WARNING_ERRORS - 1] = error;
                stored = true;
            }
            break;
            
        case SEVERITY_INFO:
            for (int i = 0; i < MAX_INFO_ERRORS; i++) {
                if (wdt_info_errors[i].error_code == ERROR_NONE) {
                    wdt_info_errors[i] = error;
                    stored = true;
                    break;
                }
            }
            if (!stored) {
                log("ℹ Buffer de info lleno - error descartado");
                return;
            }
            break;
    }
    
    if (stored) {
        wdt_total_errors++;
        log(" Error almacenado en RTC Memory");
        
        if (_errorCallback) {
            _errorCallback(code, severity, context);
        }
    }
}

// Verificación de salud del sistema
bool WatchdogManager::performHealthCheck() {
    //verifica memoria, tiempos, cantidad de fallos consecutivos y suma o resta al puntaje de salud
    log(" Verificando salud del sistema...");
    
    bool system_ok = true;
    _lastHealthCheck = millis();
    
    if (!checkMemoryHealth()) {
        system_ok = false;
    }
    
    if (!checkTimingHealth()) {
        system_ok = false;
    }
    
    if (wdt_consecutive_failures >= 3) {
        logf(" Fallos consecutivos: %d", wdt_consecutive_failures);
        system_ok = false;
    }
    
    if (system_ok) {
        if (wdt_system_health_score < 90) {
            wdt_system_health_score = wdt_system_health_score + 5;
        } else if (wdt_system_health_score < 100) {
            wdt_system_health_score = wdt_system_health_score + 1;
        }
    } else {
        if (wdt_system_health_score > 10) {
            wdt_system_health_score = wdt_system_health_score - 5;
        } else {
            wdt_system_health_score = 0;
        }
    }
    
    logf(" Salud del sistema: %d%%", wdt_system_health_score);
    
    return (wdt_system_health_score > 20 || wdt_consecutive_failures < 5);
}

// Registrar éxito
void WatchdogManager::recordSuccess() {
    //informan al watchdog del resultado de una operación
    //resetea contador de fallos consecutivos y aumenta salud con exito y con fallos baja la salud
    wdt_consecutive_failures = 0;
    wdt_last_successful_operation = millis();
    
    if (wdt_system_health_score < 100) {
        wdt_system_health_score = wdt_system_health_score + 1;
    }
    
    //logf(" Operación exitosa registrada (Health: %d%%)", wdt_system_health_score);
}

// Registrar fallo
void WatchdogManager::recordFailure() {
    wdt_consecutive_failures++;
    
    if (wdt_system_health_score > 5) {
        wdt_system_health_score = wdt_system_health_score - 5;
    } else {
        wdt_system_health_score = 0;
    }
    
    logf(" Fallo registrado - Consecutivos: %d (Health: %d%%)", 
         wdt_consecutive_failures, wdt_system_health_score);
}

// Verificar fallos críticos
bool WatchdogManager::hasCriticalFailures() {
    //+10 fallos consecutivos o salud <10%
    return (wdt_consecutive_failures >= MAX_CONSECUTIVE_FAILURES) || 
           (wdt_system_health_score < 10);
}

// Getters
uint32_t WatchdogManager::getHealthScore() { 
    return wdt_system_health_score; 
}

uint32_t WatchdogManager::getConsecutiveFailures() { 
    return wdt_consecutive_failures; 
}

// Intento de recuperación
bool WatchdogManager::attemptRecovery() {
    //restea parcialmente el sistema
    //limpia info y warnings, reduce a la mitad los fallos consecutivos, fija la salud al 50%
    //actualiza el timestamp de la última operación exitosa
    log(" Intentando recuperación del sistema...");
    
    memset(&wdt_warning_errors, 0, sizeof(wdt_warning_errors));
    memset(&wdt_info_errors, 0, sizeof(wdt_info_errors));
    
    if (wdt_consecutive_failures > 2) {
        wdt_consecutive_failures = wdt_consecutive_failures / 2;
    }
    
    wdt_system_health_score = 50;
    wdt_last_successful_operation = millis();
    
    logf(" Recovery completado - Health: %d%%, Fallos: %d", 
         wdt_system_health_score, wdt_consecutive_failures);
    
    return true;
}

// Manejo de emergencia
void WatchdogManager::handleEmergency() {
    //se ejecuta cuando el sistema está en pánico, intenta recuperar el sistema
    //en caso de fallo notifica por callback y queda en modo emergencia 
    log(" MANEJO DE EMERGENCIA DEL SISTEMA");
    
    logError(ERROR_SYSTEM_PANIC, SEVERITY_CRITICAL, wdt_consecutive_failures);
    
    if (attemptRecovery()) {
        log(" Recovery de emergencia exitoso");
        return;
    }
    
    log(" Recovery falló - Sistema en modo de emergencia");
    
    if (_errorCallback) {
        _errorCallback(ERROR_SYSTEM_PANIC, SEVERITY_CRITICAL, wdt_consecutive_failures);
    }
}

// Mostrar salud del sistema
void WatchdogManager::displaySystemHealth() {
    log("\n --- ESTADO DE SALUD DEL SISTEMA ---");
    logf("Salud general: %d%%", wdt_system_health_score);
    logf("Fallos consecutivos: %d", wdt_consecutive_failures);
    logf("Última operación exitosa: %u ms", wdt_last_successful_operation);
    logf("Total errores: %d", wdt_total_errors);
    logf("Memoria libre: %d bytes", ESP.getFreeHeap());
    logf("Watchdog: %s (%s)", 
         _watchdogInitialized ? "Funcionando" : "Inactivo",
         hardware_watchdog_available ? "Hardware" : "Software");
    log("----------------------------------");
}

// Mostrar log de errores
void WatchdogManager::displayErrorLog(int maxErrors) {
    log("\n --- LOG DE ERRORES ---");
    logf("Total errores registrados: %d", wdt_total_errors);
    
    log("Errores CRÍTICOS:");
    bool found_critical = false;
    for (int i = 0; i < MAX_CRITICAL_ERRORS; i++) {
        if (wdt_critical_errors[i].error_code != ERROR_NONE) {
            uint32_t context = (wdt_critical_errors[i].context[0] << 24) |
                              (wdt_critical_errors[i].context[1] << 16) |
                              (wdt_critical_errors[i].context[2] << 8) |
                              wdt_critical_errors[i].context[3];
            logf("  🔴 Código:%d | Tiempo:%dm | Contexto:%u",
                 wdt_critical_errors[i].error_code,
                 wdt_critical_errors[i].timestamp_min,
                 context);
            found_critical = true;
        }
    }
    if (!found_critical) log("   Sin errores críticos");
    
    logf("Errores WARNING (últimos %d):", maxErrors);
    int warning_count = 0;
    for (int i = MAX_WARNING_ERRORS - 1; i >= 0 && warning_count < maxErrors; i--) {
        if (wdt_warning_errors[i].error_code != ERROR_NONE) {
            uint32_t context = (wdt_warning_errors[i].context[0] << 24) |
                              (wdt_warning_errors[i].context[1] << 16) |
                              (wdt_warning_errors[i].context[2] << 8) |
                              wdt_warning_errors[i].context[3];
            logf("  🟡 Código:%d | Tiempo:%dm | Contexto:%u",
                 wdt_warning_errors[i].error_code,
                 wdt_warning_errors[i].timestamp_min,
                 context);
            warning_count++;
        }
    }
    if (warning_count == 0) log("   Sin warnings recientes");
    
    log("---------------------------");
}

// Configurar callbacks
void WatchdogManager::setLogCallback(LogCallback callback) {
    _logCallback = callback;
}

void WatchdogManager::setErrorCallback(ErrorCallback callback) {
    _errorCallback = callback;
}

// Habilitar/deshabilitar Serial
void WatchdogManager::enableSerial(bool enable) {
    _enableSerialOutput = enable;
}

// Verificar salud del watchdog
bool WatchdogManager::isWatchdogHealthy() {
    return _watchdogInitialized && (wdt_system_health_score > 30);
}

// ——— MÉTODOS PRIVADOS ———

// Inicializar watchdog hardware
bool WatchdogManager::initializeHardwareWatchdog() {
    //intenta configurar el watchdog hardware por 15 segundos
    log(" Inicializando Watchdog...");
    
    hardware_watchdog_available = false;
    
    esp_task_wdt_deinit();
    delay(50);
    
    esp_err_t result = esp_task_wdt_add(NULL);
    if (result == ESP_OK) {
        hardware_watchdog_available = true;
        log(" Conectado a watchdog hardware existente");
        return true;
    }
    

    result = esp_task_wdt_init(15, false);
    if (result == ESP_OK) {
        result = esp_task_wdt_add(NULL);
        if (result == ESP_OK) {
            hardware_watchdog_available = true;
            log(" Watchdog hardware inicializado (15s)");
            return true;
        }
    }
    
    // Fallback: Modo software
    log("📱 Activando modo software");
    hardware_watchdog_available = false;
    return true;
}

// Verificar memoria
bool WatchdogManager::checkMemoryHealth() {
    //verifica si la memoria libre es menor a 10KB
    size_t free_heap = ESP.getFreeHeap();
    if (free_heap < 10000) {
        logError(ERROR_MEMORY_LOW, SEVERITY_WARNING, free_heap);
        logf(" Memoria baja: %d bytes libres", free_heap);
        return false;
    } else {
        logf(" Memoria disponible: %d bytes", free_heap);
        return true;
    }
}

// Verificar tiempo
bool WatchdogManager::checkTimingHealth() {
    //si no hay éxito en más de 10 minutos, loguea warning
    uint32_t current_time = millis();
    if (wdt_last_successful_operation > 0) {
        uint32_t time_diff;
        if (current_time >= wdt_last_successful_operation) {
            time_diff = current_time - wdt_last_successful_operation;
        } else {
            log(" Overflow de millis() detectado - reiniciando contador");
            wdt_last_successful_operation = current_time;
            time_diff = 0;
        }
        
        if (time_diff > 600000) {
            logf(" Tiempo desde última operación exitosa: %u ms", time_diff);
            logError(ERROR_TIMING_ISSUE, SEVERITY_WARNING, time_diff);
            return false;
        } else {
            logf(" Última operación exitosa hace: %u ms", time_diff);
            return true;
        }
    } else {
        log("ℹ Primera ejecución - no hay operaciones previas");
        wdt_last_successful_operation = current_time;
        return true;
    }
}

// Métodos de logging
void WatchdogManager::log(const char* message) {
    if (_logCallback) {
        _logCallback(message);
    } else if (_enableSerialOutput && Serial) {
        Serial.println(message);
    }
}

void WatchdogManager::logf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    log(buffer);
}