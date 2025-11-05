/**
 * @file WatchDogManager.cpp
 * @brief Implementación del gestor de watchdog y monitoreo de salud del sistema ESP32
 * @details Este archivo contiene la lógica completa para supervisión del sistema mediante
 *          watchdog hardware/software, tracking de errores persistente en RTC Memory,
 *          sistema de puntuación de salud (0-100), y mecanismos de recuperación automática.
 *          Diseñado para sobrevivir deep sleep y mantener historial de errores críticos.
 * @author Daniel Acosta - Santiago Erazo
 * @date 01/10/2025
 * @version 1.0
 */

#include "WatchDogManager.h"
#include <stdarg.h>

// ——— Variables RTC persistentes al deep sleep ———

/**
 * @var wdt_system_health_score
 * @brief Puntuación de salud del sistema (0-100), persistente en RTC Memory
 * @details Métrica compuesta que refleja estado global del sistema. Incrementa con
 *          operaciones exitosas, decrementa con fallos. Almacenada en RTC_DATA_ATTR
 *          para sobrevivir deep sleep y resets suaves.
 * @note Valor inicial: 100 (primera ejecución), 85 (después de reset parcial).
 */
RTC_DATA_ATTR uint32_t wdt_system_health_score = 100;

/**
 * @var wdt_consecutive_failures
 * @brief Contador de fallos consecutivos sin éxito intermedio, persistente en RTC Memory
 * @details Incrementa con cada recordFailure(), resetea a 0 con recordSuccess().
 *          Usado para detectar condiciones de pánico (≥10 fallos → emergencia).
 */
RTC_DATA_ATTR uint32_t wdt_consecutive_failures = 0;

/**
 * @var wdt_last_successful_operation
 * @brief Timestamp (millis()) de la última operación exitosa, persistente en RTC Memory
 * @details Usado para detectar deadlocks o cuelgues prolongados. Si han pasado >10 minutos
 *          sin éxito, se loguea warning de timing issue.
 */
RTC_DATA_ATTR uint32_t wdt_last_successful_operation = 0;

/**
 * @var wdt_total_errors
 * @brief Contador acumulativo de errores registrados, persistente en RTC Memory
 * @details Incrementa monotónicamente con cada logError(). Útil para estadísticas
 *          a largo plazo sobre estabilidad del sistema.
 */
RTC_DATA_ATTR uint16_t wdt_total_errors = 0;

/**
 * @var wdt_critical_errors
 * @brief Buffer circular de errores críticos en RTC Memory
 * @details Almacena hasta MAX_CRITICAL_ERRORS (8) errores críticos. Cuando está lleno,
 *          sobrescribe el error más antiguo (posición 0). Sobrevive deep sleep.
 */
RTC_DATA_ATTR WatchdogManager::ErrorEntry wdt_critical_errors[WatchdogManager::MAX_CRITICAL_ERRORS];

/**
 * @var wdt_warning_errors
 * @brief Buffer FIFO de errores warning en RTC Memory
 * @details Almacena hasta MAX_WARNING_ERRORS (16) warnings. Cuando está lleno, hace
 *          shift FIFO descartando el más antiguo. Sobrevive deep sleep.
 */
RTC_DATA_ATTR WatchdogManager::ErrorEntry wdt_warning_errors[WatchdogManager::MAX_WARNING_ERRORS];

/**
 * @var wdt_info_errors
 * @brief Buffer simple de errores informativos en RTC Memory
 * @details Almacena hasta MAX_INFO_ERRORS (32) errores info. Cuando está lleno,
 *          descarta nuevos errores info (no hace shift). Sobrevive deep sleep.
 */
RTC_DATA_ATTR WatchdogManager::ErrorEntry wdt_info_errors[WatchdogManager::MAX_INFO_ERRORS];

// Variable para detectar modo de watchdog

/**
 * @var hardware_watchdog_available
 * @brief Bandera estática que indica si watchdog hardware está disponible
 * @details true: Usando esp_task_wdt hardware del ESP32
 *          false: Fallback a modo software (solo tracking, sin reset automático)
 * @note NO persistente en RTC (se reinicia en cada boot).
 */
static bool hardware_watchdog_available = false;

// ——— IMPLEMENTACIÓN DE MÉTODOS PÚBLICOS ———

/**
 * @brief Constructor de WatchdogManager
 * @details Inicializa variables internas y configura salida Serial opcional.
 *          No inicializa watchdog hardware (se hace en begin()).
 * @param enableSerial true para habilitar salida por Serial, false para modo silencioso
 * @note Constructor no realiza operaciones bloqueantes ni accede a hardware.
 */
WatchdogManager::WatchdogManager(bool enableSerial) 
    : _enableSerialOutput(enableSerial), _logCallback(nullptr), _errorCallback(nullptr),
      _lastHealthCheck(0), _watchdogInitialized(false) {
}

// Inicialización

/**
 * @brief Inicializa el sistema de watchdog y monitoreo de salud
 * @details Proceso completo:
 *          1. Configura Serial si está habilitado
 *          2. Intenta inicializar watchdog hardware (15 segundos timeout)
 *          3. Fallback a modo software si hardware falla
 *          4. Inicializa variables RTC si es primera ejecución
 *          5. Imprime estado inicial del sistema
 * @note Debe llamarse una vez en setup() antes de cualquier otra operación.
 * @note Si es primera ejecución post-flash, inicializa health score a 85%.
 */
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

/**
 * @brief Alimenta el watchdog hardware para evitar reset automático
 * @details Si watchdog hardware está disponible, llama a esp_task_wdt_reset().
 *          Si falla, cambia automáticamente a modo software. Debe llamarse
 *          periódicamente (<15 segundos) en el loop principal.
 * @note En modo software, esta función no hace nada (no hay reset automático).
 * @note Llamar esta función es seguro incluso si watchdog no está inicializado.
 */
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

/**
 * @brief Registra un error en el sistema con severidad y contexto
 * @details Proceso completo:
 *          1. Crea estructura ErrorEntry con código, severidad y contexto
 *          2. Almacena en buffer apropiado según severidad:
 *             - CRITICAL: Sobrescribe más antiguo si lleno
 *             - WARNING: Hace shift FIFO si lleno
 *             - INFO: Descarta si lleno
 *          3. Incrementa contador total de errores
 *          4. Llama callback de error si está configurado
 * @param code Código de error (ver error_code_t enum)
 * @param severity Nivel de severidad (INFO/WARNING/CRITICAL)
 * @param context Información contextual de 32 bits (ej: voltaje*1000, tiempo, etc.)
 * @note Errores se almacenan en RTC Memory y sobreviven deep sleep.
 * @note Timestamp se guarda en minutos (millis()/60000) para ahorrar espacio.
 */
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
    error.timestamp_min = millis() / 60000; // Convertir a minutos
    
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

/**
 * @brief Realiza verificación completa de salud del sistema
 * @details Verifica:
 *          1. Memoria disponible (checkMemoryHealth)
 *          2. Timing desde última operación exitosa (checkTimingHealth)
 *          3. Contador de fallos consecutivos (≥3 → fallo)
 *          Actualiza health score:
 *          - Éxito: +5 si <90%, +1 si 90-99%
 *          - Fallo: -5 si >10%, 0 si ≤10%
 * @return true si sistema saludable (health >20% y fallos <5), false si crítico
 * @note Actualiza _lastHealthCheck con millis() actual.
 * @note Llamar periódicamente (ej: cada ciclo de medición) para monitoreo continuo.
 */
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

/**
 * @brief Registra una operación exitosa en el sistema
 * @details Efectos:
 *          - Resetea contador de fallos consecutivos a 0
 *          - Actualiza timestamp de última operación exitosa
 *          - Incrementa health score en 1 punto (hasta máximo 100)
 * @note Llamar después de cada operación crítica exitosa (lectura sensores, envío WiFi, etc.).
 */
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

/**
 * @brief Registra un fallo en una operación del sistema
 * @details Efectos:
 *          - Incrementa contador de fallos consecutivos
 *          - Decrementa health score en 5 puntos (mínimo 0)
 * @note Llamar después de cada operación crítica fallida.
 * @note Si fallos consecutivos ≥10, considerar llamar handleEmergency().
 */
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

/**
 * @brief Verifica si el sistema tiene fallos críticos que requieren acción inmediata
 * @return true si ≥10 fallos consecutivos O health score <10%, false en caso contrario
 * @note Usar este método para decidir si llamar handleEmergency() o attemptRecovery().
 */
bool WatchdogManager::hasCriticalFailures() {
    //+10 fallos consecutivos o salud <10%
    return (wdt_consecutive_failures >= MAX_CONSECUTIVE_FAILURES) || 
           (wdt_system_health_score < 10);
}

// Getters

/**
 * @brief Obtiene el score actual de salud del sistema
 * @return Valor 0-100 representando salud del sistema (100=perfecto, 0=crítico)
 */
uint32_t WatchdogManager::getHealthScore() { 
    return wdt_system_health_score; 
}

/**
 * @brief Obtiene el número de fallos consecutivos actuales
 * @return Contador de fallos sin éxito intermedio
 */
uint32_t WatchdogManager::getConsecutiveFailures() { 
    return wdt_consecutive_failures; 
}

// Intento de recuperación

/**
 * @brief Intenta recuperación parcial del sistema
 * @details Acciones de recuperación:
 *          1. Limpia buffers de errores WARNING e INFO (mantiene CRITICAL)
 *          2. Reduce fallos consecutivos a la mitad
 *          3. Fija health score a 50%
 *          4. Actualiza timestamp de última operación exitosa
 * @return true siempre (indica que recovery fue intentado)
 * @note Usar cuando health score <30% pero sistema aún responde.
 * @note NO limpia errores críticos para mantener evidencia de problemas serios.
 */
bool WatchdogManager::attemptRecovery() {
    //restea parcialmente el sistema
    //limpia info y warnings, reduce a la mitad los fallos consecutivos, fija la salud al 50%
    //actualiza el timestamp de la última operación exitosa
    log(" Intentando recuperación del sistema...");
    
    // Limpiar errores no críticos
    memset(&wdt_warning_errors, 0, sizeof(wdt_warning_errors));
    memset(&wdt_info_errors, 0, sizeof(wdt_info_errors));
    
    // Reducir fallos consecutivos
    if (wdt_consecutive_failures > 2) {
        wdt_consecutive_failures = wdt_consecutive_failures / 2;
    }
    
    // Resetear health score a nivel medio
    wdt_system_health_score = 50;
    wdt_last_successful_operation = millis();
    
    logf(" Recovery completado - Health: %d%%, Fallos: %d", 
         wdt_system_health_score, wdt_consecutive_failures);
    
    return true;
}

// Manejo de emergencia

/**
 * @brief Maneja situación de emergencia del sistema (pánico)
 * @details Secuencia de emergencia:
 *          1. Loguea ERROR_SYSTEM_PANIC como crítico
 *          2. Intenta attemptRecovery()
 *          3. Si recovery exitoso, retorna normalmente
 *          4. Si recovery falla, notifica mediante callback y entra en modo emergencia
 * @note Llamar cuando hasCriticalFailures() retorna true.
 * @note En modo emergencia, sistema puede requerir reset manual o watchdog timeout.
 */
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

/**
 * @brief Muestra estado completo de salud del sistema por Serial
 * @details Imprime:
 *          - Health score actual (0-100%)
 *          - Fallos consecutivos
 *          - Timestamp de última operación exitosa
 *          - Total de errores acumulados
 *          - Memoria libre (heap)
 *          - Estado del watchdog (activo/inactivo, hardware/software)
 * @note Útil para debugging y monitoreo en desarrollo.
 */
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

/**
 * @brief Muestra log de errores almacenados en RTC Memory
 * @details Imprime:
 *          - Todos los errores CRITICAL almacenados
 *          - Últimos maxErrors WARNING (más recientes primero)
 *          - Total de errores registrados
 * @param maxErrors Número máximo de warnings a mostrar (por defecto 3)
 * @note Errores INFO no se muestran (demasiado verbosos).
 * @note Reconstruye contexto de 32 bits desde 4 bytes almacenados.
 */
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

/**
 * @brief Configura callback personalizado para logging de mensajes
 * @param callback Función con firma: void(const char* message)
 * @note Si callback está configurado, mensajes NO se imprimen por Serial automáticamente.
 * @note Útil para redirigir logs a display LCD, archivo SD, servidor remoto, etc.
 */
void WatchdogManager::setLogCallback(LogCallback callback) {
    _logCallback = callback;
}


/**
 * @brief Configura callback para notificación de errores en tiempo real
 * @param callback Función con firma: void(error_code_t, error_severity_t, uint32_t)
 * @note Se llama inmediatamente después de cada logError().
 * @note Útil para acciones inmediatas (ej: activar LED, enviar alerta, etc.).
 */
void WatchdogManager::setErrorCallback(ErrorCallback callback) {
    _errorCallback = callback;
}

// Habilitar/deshabilitar Serial

/**
 * @brief Habilita o deshabilita salida por Serial
 * @param enable true para habilitar, false para modo silencioso
 * @note Si callback está configurado, esta opción no tiene efecto.
 */
void WatchdogManager::enableSerial(bool enable) {
    _enableSerialOutput = enable;
}

// Verificar salud del watchdog

/**
 * @brief Verifica si el watchdog está funcionando correctamente
 * @return true si watchdog inicializado y health score >30%, false en caso contrario
 * @note Health score <30% indica que watchdog puede no ser confiable.
 */
bool WatchdogManager::isWatchdogHealthy() {
    return _watchdogInitialized && (wdt_system_health_score > 30);
}

// ——— MÉTODOS PRIVADOS ———

// Inicializar watchdog hardware

/**
 * @brief Inicializa watchdog hardware del ESP32
 * @details Secuencia de inicialización:
 *          1. Desactiva watchdog existente (esp_task_wdt_deinit)
 *          2. Intenta conectarse a watchdog hardware pre-existente
 *          3. Si falla, intenta crear nuevo watchdog (15 segundos timeout)
 *          4. Si todo falla, activa modo software (fallback)
 * @return true si inicialización exitosa (hardware O software), false solo si error grave
 * @note Watchdog hardware: Resetea ESP32 si no se alimenta en 15 segundos.
 * @note Modo software: Solo tracking, sin reset automático.
 */
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

/**
 * @brief Verifica salud de la memoria heap del ESP32
 * @details Si memoria libre < 10KB, loguea ERROR_MEMORY_LOW como warning.
 * @return true si memoria OK (≥10KB), false si memoria baja
 * @note 10KB es umbral conservador para operación estable con WiFi activo.
 */
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

/**
 * @brief Verifica tiempo transcurrido desde última operación exitosa
 * @details Si han pasado >10 minutos sin éxito, loguea ERROR_TIMING_ISSUE.
 *          Detecta overflow de millis() (aprox cada 49 días) y resetea contador.
 * @return true si timing OK (<10 min), false si excede umbral
 * @note 10 minutos es umbral razonable para sistema con ciclos de medición frecuentes.
 */
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

/**
 * @brief Envía mensaje de log mediante callback o Serial
 * @param message Cadena de texto a imprimir
 * @note Si _logCallback está configurado, lo usa; de lo contrario usa Serial si habilitado.
 */
void WatchdogManager::log(const char* message) {
    if (_logCallback) {
        _logCallback(message);
    } else if (_enableSerialOutput && Serial) {
        Serial.println(message);
    }
}

/**
 * @brief Envía mensaje de log con formato estilo printf
 * @param format Cadena de formato printf
 * @param ... Argumentos variables para format
 * @note Buffer interno de 256 caracteres. Mensajes más largos se truncan.
 * @note Usa vsnprintf para seguridad (previene buffer overflow).
 */
void WatchdogManager::logf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    log(buffer);
}