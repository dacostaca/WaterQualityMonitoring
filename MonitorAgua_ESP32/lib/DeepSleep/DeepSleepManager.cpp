#include "DeepSleepManager.h"
#include <stdarg.h>

// Constructor
DeepSleepManager::DeepSleepManager(uint64_t sleepInterval, uint64_t activeTime, bool enableSerial) 
    : _sleepInterval(sleepInterval), _activeTime(activeTime), _enableSerialOutput(enableSerial), _logCallback(nullptr) {
}

// Inicialización
void DeepSleepManager::begin() {
    if (_enableSerialOutput && !Serial) {
        Serial.begin(115200);
        delay(100);
    }
    
    //log("=== Deep Sleep Manager Inicializado ===");
    //logf("Intervalo total: %llu segundos", _sleepInterval);
    //logf("Tiempo activo: %llu segundos", _activeTime);
    //logf("Tiempo de sleep: %llu segundos", calculateSleepTime());
}

// Configurar intervalo de sleep
void DeepSleepManager::setSleepInterval(uint64_t seconds) {
    _sleepInterval = seconds;
    //logf("Intervalo actualizado: %llu segundos", _sleepInterval);
}

// Configurar tiempo activo
void DeepSleepManager::setActiveTime(uint64_t seconds) {
    _activeTime = seconds;
    //logf("Tiempo activo actualizado: %llu segundos", _activeTime);
}

// Obtener razón del despertar como string
String DeepSleepManager::getWakeupReason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            return "Timer RTC";
        case ESP_SLEEP_WAKEUP_EXT0:
            return "Señal externa RTC_IO";
        case ESP_SLEEP_WAKEUP_EXT1:
            return "Señal externa RTC_CNTL";
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return "Touchpad";
        case ESP_SLEEP_WAKEUP_ULP:
            return "ULP program";
        case ESP_SLEEP_WAKEUP_GPIO:
            return "GPIO";
        case ESP_SLEEP_WAKEUP_UART:
            return "UART";
        default:
            return "Arranque normal/reset";
    }
}

// Obtener causa del despertar como enum
esp_sleep_wakeup_cause_t DeepSleepManager::getWakeupCause() {
    return esp_sleep_get_wakeup_cause();
}

// Imprimir razón del despertar
void DeepSleepManager::printWakeupReason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            log(" Desperté por temporizador RTC");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            log(" Desperté por señal externa RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            log(" Desperté por señal externa RTC_CNTL");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            log(" Desperté por touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            log(" Desperté por programa ULP");
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            log(" Desperté por GPIO");
            break;
        case ESP_SLEEP_WAKEUP_UART:
            log(" Desperté por UART");
            break;
        default:
            log(" Arranque normal (reset/programación)");
            break;
    }
}


// Habilitar despertar por temporizador
void DeepSleepManager::enableTimerWakeup(uint64_t seconds) {
    uint64_t sleepTime = (seconds == 0) ? calculateSleepTime() : seconds;
    esp_sleep_enable_timer_wakeup(sleepTime * US_TO_S_FACTOR);
    logf(" Timer wakeup configurado: %llu segundos", sleepTime);
}

// Habilitar despertar por pin externo
void DeepSleepManager::enableExternalWakeup(int pin, int level) {
    esp_sleep_enable_ext0_wakeup((gpio_num_t)pin, level);
    logf(" External wakeup configurado: GPIO%d, nivel %d", pin, level);
}

// Entrar en Deep Sleep
void DeepSleepManager::goToSleep(bool showCountdown) {
    uint64_t sleepTime = calculateSleepTime();
    
    // Configurar timer wakeup
    esp_sleep_enable_timer_wakeup(sleepTime * US_TO_S_FACTOR);
    
    if (showCountdown) {
        logf(" Entrando en Deep Sleep por %llu segundos...", sleepTime);
        logf("Ciclo: %llu min total (%llu min activo + %llu min sleep)", 
            _sleepInterval/60, _activeTime/60, sleepTime/60);
        
        log("==========================================");
        delay(100);  // Dar tiempo para que se envíe el mensaje
    }
    
    // Entrar en Deep Sleep
    esp_deep_sleep_start();
}

// Entrar en Deep Sleep por tiempo específico
void DeepSleepManager::goToSleepFor(uint64_t seconds, bool showCountdown) {
    esp_sleep_enable_timer_wakeup(seconds * US_TO_S_FACTOR);
    
    if (showCountdown) {
        logf(" Entrando en Deep Sleep por %llu segundos...", seconds);
        delay(100);
    }
    
    esp_deep_sleep_start();
}

// Calcular tiempo de sleep restante
uint64_t DeepSleepManager::calculateSleepTime() {
    if (_sleepInterval <= _activeTime) {
        log(" Warning: Tiempo activo >= intervalo total");
        return 10;  // Mínimo 10 segundos de sleep
    }
    return _sleepInterval - _activeTime;
}

// Obtener información del ciclo
void DeepSleepManager::getCycleInfo(uint64_t &totalCycle, uint64_t &activeTime, uint64_t &sleepTime) {
    totalCycle = _sleepInterval;
    activeTime = _activeTime;
    sleepTime = calculateSleepTime();
}

// Verificar si es primera ejecución
bool DeepSleepManager::isFirstBoot() {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED;
}

// Habilitar/deshabilitar Serial
void DeepSleepManager::enableSerial(bool enable) {
    _enableSerialOutput = enable;
}

// Configurar callback de logging
void DeepSleepManager::setLogCallback(LogCallback callback) {
    _logCallback = callback;
}

// Sleep de emergencia
void DeepSleepManager::emergencySleep(uint64_t emergencySeconds) {
    log(" MODO EMERGENCIA - Sleep reducido");
    logf("Durmiendo %llu segundos...", emergencySeconds);
    
    esp_sleep_enable_timer_wakeup(emergencySeconds * US_TO_S_FACTOR);
    delay(1000);
    esp_deep_sleep_start();
}

// Obtener estado actual
String DeepSleepManager::getStatus() {
    String status = "=== Deep Sleep Manager Status ===\n";
    status += "Intervalo total: " + String(_sleepInterval) + "s (" + String(_sleepInterval/60) + "min)\n";
    status += "Tiempo activo: " + String(_activeTime) + "s (" + String(_activeTime/60) + "min)\n";
    status += "Tiempo sleep: " + String(calculateSleepTime()) + "s (" + String(calculateSleepTime()/60) + "min)\n";
    status += "Duty cycle: " + String((_activeTime * 100.0) / _sleepInterval, 1) + "%\n";
    status += "Última causa despertar: " + getWakeupReason() + "\n";
    status += "Primera ejecución: " + String(isFirstBoot() ? "Sí" : "No") + "\n";
    status += "Serial habilitado: " + String(_enableSerialOutput ? "Sí" : "No") + "\n";
    status += "================================";
    
    return status;
}

// Métodos privados de logging
void DeepSleepManager::log(const char* message) {
    if (_logCallback) {
        _logCallback(message);
    } else if (_enableSerialOutput && Serial) {
        Serial.println(message);
    }
}

void DeepSleepManager::logf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    log(buffer);
}