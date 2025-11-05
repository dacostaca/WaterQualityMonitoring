#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "RTCMemory.h"
#include "WatchDogManager.h"
#include "RTC.h"
#include "CalibrationManager.h"

/**
 * @brief Clase para manejo eficiente de WiFi y WebSocket en ESP32
 * 
 */

struct CalibrationValues {
    float ph_offset = 0.0f;
    float ec_offset = 0.0f;
    float turbidity_offset = 0.0f;
    // Agrega más sensores según tu proyecto
};
 class WiFiManager {
public:
    // ——— Estados de conexión ———
    typedef enum {
        //estados posibles de la conexión wifi/websocket
        WIFI_DISCONNECTED = 0,
        WIFI_CONNECTING = 1,
        WIFI_CONNECTED = 2,
        WIFI_ERROR = 3,
        WEBSOCKET_CONNECTING = 4,
        WEBSOCKET_CONNECTED = 5,
        WEBSOCKET_ERROR = 6,
        DATA_SENDING = 7,
        DATA_SENT = 8,
        DATA_ERROR = 9
    } wifi_status_t;

    // ——— Configuración WiFi ———
    typedef struct {
        //Estructura de configuración basica de wifi y websocket
        const char* ssid;
        const char* password;
        const char* server_ip;
        uint16_t server_port;
        uint32_t connect_timeout_ms;
        uint32_t websocket_timeout_ms;
        uint32_t max_retry_attempts;
    } wifi_config_t;

    // ——— Callbacks ———
    typedef void (*LogCallback)(const char* message);
    typedef void (*ErrorCallback)(WatchdogManager::error_code_t code, WatchdogManager::error_severity_t severity, uint32_t context);
    typedef void (*StatusCallback)(wifi_status_t status, const char* message);

private:
    // ——— Variables de configuración ———
    wifi_config_t _config;
    bool _enableSerialOutput;
    
    // ——— Variables de estado ———
    wifi_status_t _currentStatus;
    bool _wifiInitialized;
    bool _websocketConnected;
    uint32_t _connectionStartTime;
    uint32_t _totalDataSent;
    uint32_t _lastErrorCode;
    
    // ——— Callbacks ———
    LogCallback _logCallback;
    ErrorCallback _errorCallback;
    StatusCallback _statusCallback;
    
    // ——— Referencias a otros managers ———
    RTCMemoryManager* _rtcMemory;
    WatchdogManager* _watchdog;
    CalibrationManager* _calibrationManager;
    
    // ——— WebSocket ———
    WebSocketsClient _webSocket;
    bool _dataTransmissionComplete;
    String _lastServerResponse;

public:
    /**
     * @brief Constructor del WiFiManager
     * @param enableSerial Habilitar salida por Serial (default: true)
     */
    WiFiManager(bool enableSerial = true);
    
    /**
     * @brief Inicializar el WiFiManager
     * @param config Configuración WiFi y WebSocket
     */
    void begin(const wifi_config_t &config);
    
    /**
     * @brief Configurar referencias a otros managers
     * @param rtcMemory Puntero al RTCMemoryManager
     * @param watchdog Puntero al WatchdogManager  
     */
    void setManagers(RTCMemoryManager* rtcMemory, WatchdogManager* watchdog);
    
    /**
     * @brief Conectar a WiFi con timeout
     * @return true si conectó exitosamente
     */
    bool connectWiFi();
    
    /**
     * @brief Conectar WebSocket con timeout
     * @return true si conectó exitosamente
     */
    bool connectWebSocket();
    
    /**
     * @brief Enviar todos los datos almacenados en RTC Memory
     * @param maxReadings Máximo número de lecturas a enviar
     * @return true si envió exitosamente
     */
    bool sendStoredData(int maxReadings = 160);
    
    /**
     * @brief Enviar una lectura específica
     * @param reading Lectura de sensor a enviar
     * @return true si envió exitosamente
     */
    bool sendReading(const RTCMemoryManager::SensorReading &reading);
    
    /**
     * @brief Desconectar WiFi y liberar recursos
     */
    void disconnect();
    
    /**
     * @brief Obtener estado actual de conexión
     * @return Estado actual como enum
     */
    wifi_status_t getStatus();
    
    /**
     * @brief Obtener estado como string descriptivo
     * @return String con descripción del estado
     */
    String getStatusString();
    
    /**
     * @brief Verificar si WiFi está conectado
     * @return true si está conectado
     */
    bool isWiFiConnected();
    
    /**
     * @brief Configurar referencia al CalibrationManager
     */
    void setCalibrationManager(CalibrationManager* calibManager);


    /**
     * @brief Verificar si WebSocket está conectado
     * @return true si está conectado
     */
    bool isWebSocketConnected();
    
    /**
     * @brief Obtener información de conexión WiFi
     * @return String con IP, RSSI, etc.
     */
    String getConnectionInfo();
    
    /**
     * @brief Configurar callbacks
     */
    void setLogCallback(LogCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setStatusCallback(StatusCallback callback);
    
    /**
     * @brief Habilitar/deshabilitar salida por Serial
     */
    void enableSerial(bool enable);
    
    /**
     * @brief Proceso completo automático: conectar, enviar datos y desconectar
     * @param maxReadings Máximo número de lecturas a enviar
     * @return true si todo el proceso fue exitoso
     */
    bool transmitData(int maxReadings = 10);
    
    // ===== NUEVAS FUNCIONES PARA MODO MANUAL =====
    
    /**
     * @brief Esperar solicitud de descarga del servidor
     * @param timeout_ms Timeout en milisegundos (default: 60000 = 1 minuto)
     * @return true si se recibió solicitud, false si timeout
     */
    bool waitForDataRequest(uint32_t timeout_ms = 60000);
    
    /**
     * @brief Proceso manual: conectar, esperar solicitud, enviar si la hay
     * @param maxReadings Máximo número de lecturas a enviar
     * @param waitTimeout Timeout esperando solicitud (ms)
     * @return true si el proceso fue exitoso
     */
    bool transmitDataManual(int maxReadings = 160, uint32_t waitTimeout = 60000);
    
    /**
     * @brief Configurar modo de operación (manual o automático)
     * @param manual true para modo manual, false para automático
     */
    void setManualMode(bool manual);
    
    /**
     * @brief Obtener modo actual de operación
     * @return true si está en modo manual
     */
    bool isManualMode();
    
    /**
     * @brief Obtener estadísticas de transmisión
     * @return String con estadísticas
     */
    String getTransmissionStats();

private:
    /**
     * @brief Manejar eventos del WebSocket
     */
    void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);
    
    /**
     * @brief Crear JSON para envío de datos
     * @param reading Lectura de sensor
     * @return String JSON formateado
     */
    String createDataJSON(const RTCMemoryManager::SensorReading &reading);
    
    /**
     * @brief Actualizar estado y notificar
     * @param status Nuevo estado
     * @param message Mensaje opcional
     */
    void updateStatus(wifi_status_t status, const char* message = nullptr);
    
    /**
     * @brief Reportar error al sistema
     * @param code Código de error
     * @param severity Severidad del error
     * @param context Contexto adicional
     */
    void reportError(WatchdogManager::error_code_t code, WatchdogManager::error_severity_t severity, uint32_t context);
    
    /**
     * @brief Enviar mensaje de log
     */
    void log(const char* message);
    void logf(const char* format, ...);
    
    /**
     * @brief Función estática para eventos WebSocket (requerida por librería)
     */
    static void staticWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
    
    // ——— Variable estática para acceso desde callback estático ———
    static WiFiManager* _instance;
};

#endif // WIFI_MANAGER_H