#include "WifiManager.h"
#include <stdarg.h>
#include <time.h>

// Añadir variable para modo manual
static bool manual_download_mode = true;  

// Variable estática para acceso desde callback
WiFiManager* WiFiManager::_instance = nullptr;

// Constructor
WiFiManager::WiFiManager(bool enableSerial) 
    : _enableSerialOutput(enableSerial), _currentStatus(WIFI_DISCONNECTED),
      _wifiInitialized(false), _websocketConnected(false), _connectionStartTime(0),
      _totalDataSent(0), _lastErrorCode(0), _logCallback(nullptr), 
      _errorCallback(nullptr), _statusCallback(nullptr), _rtcMemory(nullptr),
      _watchdog(nullptr), _dataTransmissionComplete(false) {
    
    // Configurar instancia estática para callback
    _instance = this;
}

// Inicialización
void WiFiManager::begin(const wifi_config_t &config) {
    _config = config;
    
    if (_enableSerialOutput && !Serial) {
        Serial.begin(115200);
        delay(100);
    }
    
    log("=== WiFi Manager Inicializado (Modo Manual) ===");
    logf("SSID: %s", _config.ssid);
    logf("Servidor: %s:%d", _config.server_ip, _config.server_port);
    logf("Timeout WiFi: %u ms", _config.connect_timeout_ms);
    logf("Timeout WebSocket: %u ms", _config.websocket_timeout_ms);
    log(" Modo descarga: MANUAL (por solicitud)");
    
    // Configurar modo WiFi
    WiFi.mode(WIFI_STA);
    
    // Configurar callback estático para WebSocket
    _webSocket.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        this->webSocketEvent(type, payload, length);
    });
    
    _wifiInitialized = true;
    updateStatus(WIFI_DISCONNECTED, "Inicializado correctamente");
}

// Configurar managers
void WiFiManager::setManagers(RTCMemoryManager* rtcMemory, WatchdogManager* watchdog) {
    //metodo para configurar referencias a otros managers dando acceso a rtc y al watchdog
    _rtcMemory = rtcMemory;
    _watchdog = watchdog;
    log(" Referencias a managers configuradas");
}

// Conectar WiFi
bool WiFiManager::connectWiFi() {
    if (!_wifiInitialized) {
        //aborta la conexión si no se ha inicializado con begin()
        reportError(WatchdogManager::ERROR_WIFI_FAIL, WatchdogManager::SEVERITY_CRITICAL, 1);
        return false;
    }
    
    updateStatus(WIFI_CONNECTING, "Conectando a WiFi...");
    log(" Conectando a WiFi...");
    
    _connectionStartTime = millis(); //guarda instante de conexión
    
    // Intentar conectar
    WiFi.begin(_config.ssid, _config.password); // Inicia conexión WiFi
    
    // Esperar conexión con timeout
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        //se queda en bucle mientras no esté conectado y calcula el tiempo
        //si excede el tiempo máximo de espera, aborta y marca error wifi
        //reporta al watchdog
        uint32_t elapsed = millis() - startTime;
        
        if (elapsed > _config.connect_timeout_ms) {
            logf(" Timeout conectando WiFi (%u ms)", elapsed);
            updateStatus(WIFI_ERROR, "Timeout WiFi");
            reportError(WatchdogManager::ERROR_WIFI_FAIL, WatchdogManager::SEVERITY_WARNING, elapsed);
            return false;
        }
        
        // Alimentar watchdog durante espera
        if (_watchdog) {
            //igual alimenta el watchdog para que no se reinicie
            _watchdog->feedWatchdog();
        }
        
        delay(100);
        
        // Log de progreso cada 2 segundos
        if (elapsed % 2000 < 100) {
            logf("⏳ Conectando WiFi... %u ms", elapsed);
        }
    }
    
    uint32_t connectionTime = millis() - startTime;
    logf(" WiFi conectado en %u ms", connectionTime);
    logf(" IP: %s", WiFi.localIP().toString().c_str());
    logf(" RSSI: %d dBm", WiFi.RSSI());
    
    updateStatus(WIFI_CONNECTED, "WiFi conectado");
    
    return true;
}

// Conectar WebSocket
bool WiFiManager::connectWebSocket() {
    if (!isWiFiConnected()) {
        //verificca conexión wifi activa
        log(" WiFi no conectado");
        return false;
    }
    
    updateStatus(WEBSOCKET_CONNECTING, "Conectando WebSocket...");
    log(" Conectando WebSocket...");
    
    // Configurar y conectar WebSocket
    _webSocket.begin(_config.server_ip, _config.server_port, "/");
    _webSocket.setReconnectInterval(1000);
    
    // Esperar conexión con timeout
    uint32_t startTime = millis();
    _dataTransmissionComplete = false;
    
    while (!_websocketConnected) {
        //permanece en bucle hasta conectar o timeout usando la bandera _websocketConnected
        //si supera el máximo de espera, aborta y marca error wifi
        //reporta al watchdog
        uint32_t elapsed = millis() - startTime;
        
        if (elapsed > _config.websocket_timeout_ms) {
            logf(" Timeout conectando WebSocket (%u ms)", elapsed);
            updateStatus(WEBSOCKET_ERROR, "Timeout WebSocket");
            reportError(WatchdogManager::ERROR_WIFI_FAIL, WatchdogManager::SEVERITY_WARNING, elapsed);
            return false;
        }
        
        // Procesar eventos WebSocket
        _webSocket.loop();
        
        // Alimentar watchdog
        if (_watchdog) {
            _watchdog->feedWatchdog();
        }
        
        delay(50);
        
        // Log de progreso cada 1 segundo
        if (elapsed % 1000 < 50) {
            logf("⏳ Conectando WebSocket... %u ms", elapsed);
        }
    }
    //luego calcula el tiempo total hasta la conexión
    uint32_t connectionTime = millis() - startTime;
    logf(" WebSocket conectado en %u ms", connectionTime);
    
    updateStatus(WEBSOCKET_CONNECTED, "WebSocket conectado");
    
    return true;
}

// función para esperar solicitud de datos
bool WiFiManager::waitForDataRequest(uint32_t timeout_ms) {
    if (!isWebSocketConnected()) {
        //confirma conexión websocket activa
        log(" WebSocket no conectado");
        return false;
    }
    
    log(" Esperando solicitud de descarga del servidor...");
    updateStatus(WEBSOCKET_CONNECTED, "Esperando solicitud");
    
    uint32_t startTime = millis();
    bool requestReceived = false;
    
    while (!requestReceived && (millis() - startTime < timeout_ms)) {
        // Procesar eventos WebSocket
        //permanece en bucle hasta recibir solicitud del servidor o timeout
        _webSocket.loop();
        
        // Verificar si recibimos solicitud
        if (_lastServerResponse.indexOf("request_all_data") != -1) {
            log(" ¡Solicitud de datos recibida!");
            requestReceived = true;
            _lastServerResponse = ""; 
            break;
        }
        
        // Alimentar watchdog
        if (_watchdog) {
            _watchdog->feedWatchdog();
        }
        
        delay(100);
        
        // Status cada 5 segundos
        if ((millis() - startTime) % 5000 < 100) {
            logf("⏳ Esperando solicitud... %u s", (millis() - startTime) / 1000);
        }
    }
    
    if (!requestReceived) {
        log(" Timeout esperando solicitud de datos");
        return false;
    }
    
    return true;
}

// Enviar datos con notificación de inicio/fin
bool WiFiManager::sendStoredData(int maxReadings) {
    if (!isWebSocketConnected()) {
        log(" WebSocket no conectado");
        return false;
    }
    
    if (!_rtcMemory) {
        log(" RTCMemory no configurada");
        return false;
    }
    
    updateStatus(DATA_SENDING, "Enviando datos...");
    log(" Iniciando envío de datos almacenados...");
    
    // Notificar inicio de envío
    String startMsg = "{\"action\":\"sending_data\",\"timestamp\":\"" + 
                      String(millis()) + "\"}";
    _webSocket.sendTXT(startMsg);
    delay(100);
    
    // Obtener lecturas recientes
    //buffer local para 120 lecturas definidas aquí mismo, probar cambios para aumentar cantidad de muestras envíadas
    //trae las lecturas desde RTC Memory
    RTCMemoryManager::SensorReading readings[120]; // Aumentar capacidad
    int count = _rtcMemory->getRecentReadings(readings, maxReadings);
    
    if (count == 0) {
        log(" No hay datos para enviar");
        
        // Notificar que no hay datos
        String noDataMsg = "{\"action\":\"data_complete\",\"total\":0}";
        _webSocket.sendTXT(noDataMsg);
        
        updateStatus(DATA_SENT, "Sin datos para enviar");
        return true;
    }
    
    logf(" Enviando %d lecturas...", count);
    

    //NUCLEO DEL PROCESO DE ENVÍO DE DATOS
    bool allSent = true;
    uint32_t sendStartTime = millis();
    int successCount = 0;
    
    for (int i = 0; i < count; i++) {
        //envía cada lectura individualmente
        //si falla alguna, marca allSent como false
        //alimenta el watchdog durante el envío de datos
        //cada 10 lecturas muestra progreso
        if (!sendReading(readings[i])) {
            logf(" Error enviando lectura #%d", readings[i].reading_number);
            allSent = false;
        } else {
            successCount++;
        }
        
        // Pequeña pausa entre envíos
        delay(50);
        
        // Alimentar watchdog
        if (_watchdog) {
            _watchdog->feedWatchdog();
        }
        
        // Mostrar progreso
        if (i % 10 == 0 && i > 0) {
            logf(" Progreso: %d/%d lecturas enviadas", i, count);
        }
        
        // Timeout general para todo el envío
        if (millis() - sendStartTime > (_config.websocket_timeout_ms * 3)) {
            log(" Timeout general enviando datos");
            allSent = false;
            break;
        }
    }
    
    // Notificar fin de envío
    String endMsg = "{\"action\":\"data_complete\",\"total\":" + 
                    String(successCount) + "}";
    _webSocket.sendTXT(endMsg);
    delay(100);
    
    if (allSent && successCount == count) {
        logf(" Todos los datos enviados exitosamente (%u ms)", millis() - sendStartTime);
        updateStatus(DATA_SENT, "Datos enviados");
        _totalDataSent += count;
        
        // Marcar datos como enviados en RTC Memory
        _rtcMemory->markDataSent();
        
        return true;
    } else {
        logf(" Enviados %d de %d datos", successCount, count);
        updateStatus(DATA_ERROR, "Envío parcial");
        
        if (successCount > 0) {
            _totalDataSent += successCount;
            _rtcMemory->markDataSent(); 
        }
        
        return successCount > 0; // Éxito parcial
    }
    //fin del proceso de envío de datos
    //si todo sale bien actualiza el estado y marca los datos como enviados en el RTC para no intentarlo otra vez
    //en caso de errores parciales, también marca los que se enviaron correctamente
}

// Enviar una lectura específica 
bool WiFiManager::sendReading(const RTCMemoryManager::SensorReading &reading) {
    if (!isWebSocketConnected()) {
        return false;
    }
    
    String jsonData = createDataJSON(reading);
    
    // No mostrar cada envío individual en modo manual
    if (!manual_download_mode) {
        logf(" Enviando: %s", jsonData.c_str());
    }
    
    // Enviar datos
    _webSocket.sendTXT(jsonData);
    
    // En modo manual, no esperar confirmación individual
    if (manual_download_mode) {
        delay(20); // Pequeña pausa para no saturar
        return true;
    }
    
    // Modo automático: esperar confirmación
    uint32_t startTime = millis();
    _dataTransmissionComplete = false;
    
    while (!_dataTransmissionComplete && (millis() - startTime < 3000)) {
        _webSocket.loop();
        delay(10);
        
        if (_watchdog) {
            _watchdog->feedWatchdog();
        }
    }
    
    return _dataTransmissionComplete;
}

// Crear JSON para envío

String WiFiManager::createDataJSON(const RTCMemoryManager::SensorReading &reading) {
    StaticJsonDocument<400> doc;
    
    // Información del dispositivo
    doc["device_id"] = "ESP32_WaterMonitor";
    doc["timestamp"] = reading.timestamp;
    doc["rtc_timestamp"] = reading.rtc_timestamp;
    doc["reading_number"] = reading.reading_number;
    doc["sequence"] = _rtcMemory ? _rtcMemory->getSequenceNumber() : 0;
    
    if (reading.rtc_timestamp > 1609459200) {
        time_t local_time = reading.rtc_timestamp;
        struct tm* timeinfo = localtime(&local_time);
        
        char datetime_buffer[20];
        char date_buffer[11];
        char time_buffer[9];
        
        // Formatear fecha/hora completa
        snprintf(datetime_buffer, sizeof(datetime_buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        
        // Formatear solo fecha
        snprintf(date_buffer, sizeof(date_buffer), "%04d-%02d-%02d",
                timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
        
        // Formatear solo hora
        snprintf(time_buffer, sizeof(time_buffer), "%02d:%02d:%02d",
                timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        
        doc["rtc_datetime"] = String(datetime_buffer);
        doc["rtc_date"] = String(date_buffer);
        doc["rtc_time"] = String(time_buffer);
    } else {
        doc["rtc_datetime"] = "No disponible";
        doc["rtc_date"] = "No disponible";
        doc["rtc_time"] = "No disponible";
    }
    
    // Datos de sensores
    doc["temperature"] = reading.temperature;
    doc["ph"] = reading.ph;
    doc["turbidity"] = reading.turbidity;
    doc["tds"] = reading.tds;
    doc["ec"] = reading.ec; 
    doc["sensor_status"] = reading.sensor_status;
    doc["valid"] = reading.valid;
    
    // Información del sistema
    doc["health_score"] = _watchdog ? _watchdog->getHealthScore() : 100;
    doc["rssi"] = WiFi.RSSI();
    doc["free_heap"] = ESP.getFreeHeap();
    
    String output;
    serializeJson(doc, output);
    
    return output;
}

// Desconectar
void WiFiManager::disconnect() {
    log("🔌 Desconectando WiFi...");
    
    // Cerrar WebSocket
    if (_websocketConnected) {
        _webSocket.disconnect();
        _websocketConnected = false;
    }
    
    // Desconectar WiFi
    if (WiFi.isConnected()) {
        WiFi.disconnect();
    }
    
    // Modo bajo consumo
    WiFi.mode(WIFI_OFF);
    
    updateStatus(WIFI_DISCONNECTED, "Desconectado");
    log(" WiFi desconectado completamente");
}

// Proceso manual de transmisión
bool WiFiManager::transmitDataManual(int maxReadings, uint32_t waitTimeout) {
    log("\n === INICIANDO TRANSMISIÓN MANUAL ===");
    
    uint32_t processStartTime = millis();
    bool success = false;
    
    do {
        // Conectar WiFi
        if (!connectWiFi()) {
            log(" Falló conexión WiFi");
            break;
        }
        
        // Conectar WebSocket
        if (!connectWebSocket()) {
            log(" Falló conexión WebSocket");
            break;
        }
        
        // Esperar solicitud de descarga
        if (!waitForDataRequest(waitTimeout)) {
            log(" No se recibió solicitud de descarga");
            //  no había solicitud
            success = true; // Conexión exitosa aunque no se enviaron datos
            break;
        }
        
        // Enviar datos
        if (!sendStoredData(maxReadings)) {
            log(" Falló envío de datos");
            break;
        }
        
        success = true;
        
    } while (false);
    
    // Siempre desconectar al final
    disconnect();
    
    uint32_t totalTime = millis() - processStartTime;
    
    if (success) {
        logf(" Proceso completado en %u ms", totalTime);
        if (_watchdog) {
            _watchdog->recordSuccess();
        }
    } else {
        logf(" Proceso falló en %u ms", totalTime);
        if (_watchdog) {
            _watchdog->recordFailure();
        }
    }
    
    log("=== FIN TRANSMISIÓN MANUAL ===\n");
    
    return success;
}

// Proceso automático original 
bool WiFiManager::transmitData(int maxReadings) {
    // Desactivar modo manual temporalmente
    bool previousMode = manual_download_mode;
    manual_download_mode = false;
    
    log("\n === INICIANDO TRANSMISIÓN AUTOMÁTICA ===");
    
    uint32_t processStartTime = millis();
    bool success = false;
    
    do {
        // Conectar WiFi
        if (!connectWiFi()) {
            log(" Falló conexión WiFi");
            break;
        }
        
        // Conectar WebSocket
        if (!connectWebSocket()) {
            log(" Falló conexión WebSocket");
            break;
        }
        
        // Enviar datos inmediatamente
        if (!sendStoredData(maxReadings)) {
            log(" Falló envío de datos");
            break;
        }
        
        success = true;
        
    } while (false);
    
    // Siempre desconectar al final
    disconnect();
    
    uint32_t totalTime = millis() - processStartTime;
    
    if (success) {
        logf("Transmisión exitosa en %u ms", totalTime);
        if (_watchdog) {
            _watchdog->recordSuccess();
        }
    } else {
        logf(" Transmisión falló en %u ms", totalTime);
        if (_watchdog) {
            _watchdog->recordFailure();
        }
    }
    
    log("=== FIN TRANSMISIÓN AUTOMÁTICA ===\n");
    
    // Restaurar modo
    manual_download_mode = previousMode;
    
    return success;
}

// Event handler del WebSocket
void WiFiManager::webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            log(" WebSocket desconectado");
            _websocketConnected = false;
            updateStatus(WEBSOCKET_ERROR, "WebSocket desconectado");
            break;
            
        case WStype_CONNECTED:
            logf(" WebSocket conectado a: %s", payload);
            _websocketConnected = true;
            updateStatus(WEBSOCKET_CONNECTED, "WebSocket conectado");
            break;
            
        case WStype_TEXT:
            _lastServerResponse = String((char*)payload);
            
            // En modo manual, solo mostrar mensajes importantes
            if (manual_download_mode) {
                if (_lastServerResponse.indexOf("request_all_data") != -1) {
                    log(" Servidor solicita los datos");
                } else if (_lastServerResponse.indexOf("success") != -1) {
                    // No mostrar confirmaciones individuales
                } else if (_lastServerResponse.indexOf("conectado") != -1) {
                    log(" Servidor confirma conexión");
                } else {
                    logf(" Servidor: %s", _lastServerResponse.c_str());
                }
            } else {
                logf(" Servidor responde: %s", _lastServerResponse.c_str());
            }
            
            // Verificar si es confirmación de recepción
            if (_lastServerResponse.indexOf("success") != -1 || 
                _lastServerResponse.indexOf("received") != -1) {
                _dataTransmissionComplete = true;
            }
            break;
            
        case WStype_ERROR:
            logf(" Error WebSocket: %s", payload);
            updateStatus(WEBSOCKET_ERROR, "Error WebSocket");
            reportError(WatchdogManager::ERROR_WIFI_FAIL, WatchdogManager::SEVERITY_WARNING, 0);
            break;
            
        default:
            break;
    }
}

// Configurar modo de operación
void WiFiManager::setManualMode(bool manual) {
    manual_download_mode = manual;
    logf(" Modo descarga: %s", manual ? "MANUAL" : "AUTOMÁTICO");
}

// Obtener modo actual
bool WiFiManager::isManualMode() {
    return manual_download_mode;
}

// Getters y utilidades (sin cambios)
bool WiFiManager::isWiFiConnected() {
    return WiFi.isConnected();
}

bool WiFiManager::isWebSocketConnected() {
    return _websocketConnected;
}

WiFiManager::wifi_status_t WiFiManager::getStatus() {
    return _currentStatus;
}

String WiFiManager::getStatusString() {
    switch(_currentStatus) {
        case WIFI_DISCONNECTED: return "Desconectado";
        case WIFI_CONNECTING: return "Conectando WiFi";
        case WIFI_CONNECTED: return "WiFi Conectado";
        case WIFI_ERROR: return "Error WiFi";
        case WEBSOCKET_CONNECTING: return "Conectando WebSocket";
        case WEBSOCKET_CONNECTED: return "WebSocket Conectado";
        case WEBSOCKET_ERROR: return "Error WebSocket";
        case DATA_SENDING: return "Enviando Datos";
        case DATA_SENT: return "Datos Enviados";
        case DATA_ERROR: return "Error Enviando";
        default: return "Estado Desconocido";
    }
}

String WiFiManager::getConnectionInfo() {
    if (isWiFiConnected()) {
        String info = "IP: " + WiFi.localIP().toString();
        info += " | RSSI: " + String(WiFi.RSSI()) + " dBm";
        info += " | SSID: " + String(_config.ssid);
        return info;
    }
    return "WiFi desconectado";
}

String WiFiManager::getTransmissionStats() {
    String stats = "=== Estadísticas WiFi ===\n";
    stats += "Estado: " + getStatusString() + "\n";
    stats += "Modo: " + String(manual_download_mode ? "MANUAL" : "AUTOMÁTICO") + "\n";
    stats += "Datos enviados: " + String(_totalDataSent) + " lecturas\n";
    stats += "Último error: " + String(_lastErrorCode) + "\n";
    stats += "Conexión: " + getConnectionInfo() + "\n";
    stats += "========================";
    return stats;
}

// Configurar callbacks
void WiFiManager::setLogCallback(LogCallback callback) {
    _logCallback = callback;
}

void WiFiManager::setErrorCallback(ErrorCallback callback) {
    _errorCallback = callback;
}

void WiFiManager::setStatusCallback(StatusCallback callback) {
    _statusCallback = callback;
}

void WiFiManager::enableSerial(bool enable) {
    _enableSerialOutput = enable;
}

// Métodos privados
void WiFiManager::updateStatus(wifi_status_t status, const char* message) {
    _currentStatus = status;
    
    if (_statusCallback) {
        _statusCallback(status, message);
    }
    
    if (message) {
        logf("📊 Estado: %s - %s", getStatusString().c_str(), message);
    }
}

void WiFiManager::reportError(WatchdogManager::error_code_t code, WatchdogManager::error_severity_t severity, uint32_t context) {
    _lastErrorCode = code;
    
    if (_errorCallback) {
        _errorCallback(code, severity, context);
    }
    
    if (_watchdog) {
        _watchdog->logError(code, severity, context);
    }
}

void WiFiManager::log(const char* message) {
    if (_logCallback) {
        _logCallback(message);
    } else if (_enableSerialOutput && Serial) {
        Serial.println(message);
    }
}

void WiFiManager::logf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    log(buffer);
}