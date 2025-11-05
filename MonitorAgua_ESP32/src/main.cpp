/**
 * Sistema de Monitoreo de Calidad del Agua
 */

#include <Arduino.h>
#include "WatchDogManager.h"
#include "RTCMemory.h"
#include "DeepSleepManager.h"
#include "Temperatura.h"
#include "TDS.h"
#include "Turbidez.h"
#include "WifiManager.h"
#include "RTC.h"
#include "pH.h"
#include "CalibrationManager.h"

// ——— Configuración del Sistema ———
#define SLEEP_INTERVAL_SECONDS 30
#define ACTIVE_TIME_SECONDS 10
#define WIFI_CHECK_INTERVAL 2
#define MANUAL_WAIT_TIMEOUT 20000

// ——— Pines de Sensores ———
#define TEMPERATURE_PIN 17
#define TDS_PIN 7
#define TURBIDITY_PIN 5
#define PH_PIN 1
#define led 2

// ——— Pines RTC
#define RTC_SDA_PIN 8
#define RTC_SCL_PIN 9

// ——— Configuración WiFi ———
const WiFiManager::wifi_config_t WIFI_CONFIG = {
    .ssid = "RED_MONITOREO",
    .password = "Holamundo6",
    .server_ip = "192.168.137.1",
    .server_port = 8765,
    .connect_timeout_ms = 15000,
    .websocket_timeout_ms = 10000,
    .max_retry_attempts = 3};

// ——— Instancias globales ———
WatchdogManager watchdog(true);
RTCMemoryManager rtcMemory(true);
DeepSleepManager deepSleep(SLEEP_INTERVAL_SECONDS, ACTIVE_TIME_SECONDS, true);
WiFiManager wifiManager(true);
MAX31328RTC rtcExterno;
CalibrationManager calibManager(true);

bool forceManualCheck = false;

void setup()
{
    Serial.begin(115200);
    delay(100);

    Serial.println("\n=== SISTEMA DE MONITOREO DE CALIDAD DEL AGUA ===");
    Serial.println("================================================\n");

    pinMode(led, OUTPUT);
    digitalWrite(led, HIGH);
    // ——— 1. INICIALIZAR WATCHDOG ———
    watchdog.begin();
    watchdog.feedWatchdog();

    // 1.5. INICIALIZAR CALIBRATION MANAGER
    calibManager.begin();
    watchdog.feedWatchdog();

    // ——— 2. VERIFICAR/INICIALIZAR RTC MEMORY ———
    rtcMemory.begin();
    if (!rtcMemory.validateIntegrity())
    {
        // Serial.println(" Datos RTC Memory corruptos - Inicializando");
        rtcMemory.initialize();
        watchdog.logError(WatchdogManager::ERROR_RTC_CORRUPTION,
                        WatchdogManager::SEVERITY_WARNING, 0);
    }
    else
    {
        Serial.println(" Datos RTC Memory válidos");

        // Mostrar información del sistema
        deepSleep.begin();
        deepSleep.printWakeupReason();
        watchdog.displaySystemHealth();

        if (watchdog.getHealthScore() < 20)
        {
            Serial.println("Salud muy baja - Intentando recovery");
            watchdog.attemptRecovery();
        }
    }

    watchdog.feedWatchdog();

    // ——— 3. INICIALIZAR RTC EXTERNO MAX31328 ———
    Serial.println("\n === INICIALIZANDO RTC MAX31328 ===");
    bool rtcAvailable = false;

    // Serial.printf("Inicializando MAX31328 (SDA=%d, SCL=%d)...\n", RTC_SDA_PIN, RTC_SCL_PIN);

    if (!rtcExterno.begin(RTC_SDA_PIN, RTC_SCL_PIN))
    {
        Serial.println(" Error inicializando RTC MAX31328");

        watchdog.logError(WatchdogManager::ERROR_SENSOR_INIT_FAIL,
                        WatchdogManager::SEVERITY_WARNING, 0x31328);
    }
    else
    {
        Serial.println(" RTC MAX31328 inicializado correctamente");
        rtcAvailable = true;

        rtcExterno.printDebugInfo();
        if (deepSleep.isFirstBoot() || rtcExterno.hasLostTime())
        {
            Serial.println(" RTC necesita sincronización");
        }

        watchdog.recordSuccess();
    }

    watchdog.feedWatchdog();

    // ——— 4. HEALTH CHECK ———
    bool health_ok = watchdog.performHealthCheck();
    if (!health_ok && watchdog.getConsecutiveFailures() >= 5)
    {
        Serial.println(" Sistema en falla crítica");
        watchdog.attemptRecovery();
    }

    // ——— 5. CONFIGURAR ERROR LOGGER PARA SENSORES ———
    auto errorLogger = [](int code, int severity, uint32_t context)
    {
        watchdog.logError(static_cast<WatchdogManager::error_code_t>(code),
                        static_cast<WatchdogManager::error_severity_t>(severity),
                        context);
    };

    // ——— 6. INICIALIZAR SENSOR DE TEMPERATURA ———
    // Serial.println("\n Inicializando sensor de temperatura...");
    TemperatureSensor::setErrorLogger(errorLogger);

    if (!TemperatureSensor::initialize(TEMPERATURE_PIN))
    {
        Serial.println(" Error inicializando sensor temperatura");
        watchdog.logError(WatchdogManager::ERROR_SENSOR_INIT_FAIL,
                        WatchdogManager::SEVERITY_CRITICAL, TEMPERATURE_PIN);
        watchdog.recordFailure();
    }
    else
    {
        Serial.println(" Sensor temperatura inicializado");
        watchdog.recordSuccess();
    }

    watchdog.feedWatchdog();

    // ——— 7. INICIALIZAR SENSOR TDS ———
    // Serial.println("\n Inicializando sensor TDS...");
    TDSSensor::setErrorLogger(errorLogger);

    if (!TDSSensor::initialize(TDS_PIN))
    {
        Serial.println(" Error inicializando sensor TDS");
        watchdog.logError(WatchdogManager::ERROR_SENSOR_INIT_FAIL,
                        WatchdogManager::SEVERITY_CRITICAL, TDS_PIN);
        watchdog.recordFailure();
    }
    else
    {
        Serial.println(" Sensor TDS inicializado");

        float kValue, vOffset;
        TDSSensor::getCalibration(kValue, vOffset);

        watchdog.recordSuccess();
    }

    watchdog.feedWatchdog();

    // ——— 8. INICIALIZAR SENSOR DE TURBIDEZ ———
    // Serial.println("\n Inicializando sensor de turbidez...");
    TurbiditySensor::setErrorLogger(errorLogger);

    if (!TurbiditySensor::initialize(TURBIDITY_PIN))
    {
        Serial.println(" Error inicializando sensor turbidez");
        watchdog.logError(WatchdogManager::ERROR_SENSOR_INIT_FAIL,
                        WatchdogManager::SEVERITY_CRITICAL, TURBIDITY_PIN);
        watchdog.recordFailure();
    }
    else
    {
        Serial.println(" Sensor turbidez inicializado");
        watchdog.recordSuccess();
    }

    watchdog.feedWatchdog();

    // ——— 9. INICIALIZAR SENSOR DE pH ———
    // Serial.println("\n Inicializando sensor pH...");
    pHSensor::setErrorLogger(errorLogger);

    if (!pHSensor::initialize(PH_PIN))
    {
        Serial.println(" Error inicializando sensor pH");
        watchdog.logError(WatchdogManager::ERROR_SENSOR_INIT_FAIL,
                        WatchdogManager::SEVERITY_CRITICAL, PH_PIN);
        watchdog.recordFailure();
    }
    else
    {
        Serial.println(" Sensor pH inicializado");

        float phOffset, phSlope;
        pHSensor::getCalibration(phOffset, phSlope);

        watchdog.recordSuccess();
    }

    watchdog.feedWatchdog();

    // ——— 10. TOMAR LECTURAS DE SENSORES ———
    Serial.println("\n === TOMANDO LECTURAS DE SENSORES ===");

    // Serial.println(" Leyendo temperatura...");
    TemperatureReading tempReading = TemperatureSensor::takeReadingWithTimeout();

    watchdog.feedWatchdog();

    // Serial.println(" Leyendo TDS...");
    float tempForTDS = tempReading.valid ? tempReading.temperature : 25.0f;
    TDSReading tdsReading = TDSSensor::takeReadingWithTimeout(tempForTDS);

    watchdog.feedWatchdog();

    // Serial.println(" Leyendo turbidez...");
    TurbidityReading turbidityReading = TurbiditySensor::takeReadingWithTimeout();

    watchdog.feedWatchdog();

    // Serial.println(" Leyendo pH...");
    float tempForPH = tempReading.valid ? tempReading.temperature : 25.0f;
    pHReading phReading = pHSensor::takeReadingWithTimeout(tempForPH);

    watchdog.feedWatchdog();

    // ——— 11. OBTENER TIMESTAMP DEL RTC MAX31328 ———
    uint32_t rtcTimestamp = 0;
    String rtcDateTime = "No disponible";

    if (rtcAvailable && rtcExterno.isPresent())
    {
        rtcTimestamp = rtcExterno.getUnixTimestamp();
        rtcDateTime = rtcExterno.getFormattedDateTime();

        if (rtcTimestamp < 1609459200)
        {
            Serial.println(" Timestamp RTC inválido - usando tiempo relativo");
            rtcTimestamp = millis() / 1000; // Segundos desde boot
        }
        else
        {
            Serial.printf(" Timestamp RTC: %u (%s)\n", rtcTimestamp, rtcDateTime.c_str());
        }
    }
    else
    {
        Serial.println(" RTC no disponible - usando timestamp relativo");
        rtcTimestamp = millis() / 1000;
    }

    // ——— 12. ALMACENAR EN RTC MEMORY ———
    bool readingStored = false;

    if (tempReading.valid || tdsReading.valid || turbidityReading.valid || phReading.valid)
    {

        RTCMemoryManager::SensorReading reading = rtcMemory.createFullReading(
            tempReading.valid ? tempReading.temperature : 0.0f,
            phReading.valid ? phReading.ph_value : 0.0f,
            turbidityReading.valid ? turbidityReading.turbidity_ntu : 0.0f,
            tdsReading.valid ? tdsReading.tds_value : 0.0f,
            tdsReading.valid ? tdsReading.ec_value : 0.0f,
            (tempReading.sensor_status << 0) |
                (tdsReading.sensor_status << 2) |
                (turbidityReading.sensor_status << 4) |
                (phReading.sensor_status << 6));

        reading.rtc_timestamp = rtcTimestamp;

        reading.valid = (tempReading.valid || tdsReading.valid || turbidityReading.valid || phReading.valid);

        if (rtcMemory.storeReading(reading))
        {
            Serial.println("\n === LECTURA ALMACENADA ===");
            Serial.printf(" Lectura #%d guardada exitosamente\n", rtcMemory.getTotalReadings());
            Serial.printf(" Timestamp: %s (Unix: %u)\n", rtcDateTime.c_str(), rtcTimestamp);

            if (tempReading.valid)
            {
                Serial.printf(" Temperatura: %.2f°C\n", tempReading.temperature);
            }
            if (tdsReading.valid)
            {
                Serial.printf(" TDS: %.1f ppm (EC: %.1f µS/cm)\n",
                            tdsReading.tds_value, tdsReading.ec_value);
            }
            if (turbidityReading.valid)
            {
                Serial.printf(" Turbidez: %.1f NTU (%s)\n",
                            turbidityReading.turbidity_ntu,
                            TurbiditySensor::getWaterQuality(turbidityReading.turbidity_ntu).c_str());
            }
            if (phReading.valid)
            {
                Serial.printf(" pH: %.2f (%s)\n",
                            phReading.ph_value,
                            pHSensor::getWaterType(phReading.ph_value).c_str());
            }
            Serial.println("==========================");

            watchdog.recordSuccess();
            readingStored = true;
        }
        else
        {
            Serial.println(" Error almacenando lecturas");
            watchdog.logError(WatchdogManager::ERROR_RTC_WRITE_FAIL,
                            WatchdogManager::SEVERITY_CRITICAL, 0);
            watchdog.recordFailure();
        }
    }
    else
    {
        Serial.println(" Todas las lecturas inválidas - no se almacena");
        watchdog.recordFailure();
    }

    // ——— 13. VERIFICAR SI ES MOMENTO DE CONECTAR WIFI ———
    bool shouldCheckWiFi = (rtcMemory.getTotalReadings() % WIFI_CHECK_INTERVAL == 0) && (rtcMemory.getTotalReadings() > 0);

    if (deepSleep.getWakeupCause() == ESP_SLEEP_WAKEUP_EXT0)
    {
        Serial.println(" Despertar por botón - Forzando verificación WiFi");
        forceManualCheck = true;
    }

    if (shouldCheckWiFi || forceManualCheck)
    {
        Serial.println("\n === VERIFICACIÓN WIFI PROGRAMADA ===");
        Serial.printf(" Datos almacenados: %d lecturas\n", rtcMemory.getTotalReadings());
        // Serial.println(" Conectando para verificar si hay solicitud de descarga...");

        wifiManager.begin(WIFI_CONFIG);
        wifiManager.setManagers(&rtcMemory, &watchdog);
        wifiManager.setCalibrationManager(&calibManager);
        wifiManager.setManualMode(true);

        wifiManager.setErrorCallback([](WatchdogManager::error_code_t code,
                                        WatchdogManager::error_severity_t severity,
                                        uint32_t context)
                                    { watchdog.logError(code, severity, context); });

        watchdog.feedWatchdog();

        bool wifiSuccess = wifiManager.transmitDataManual(160, MANUAL_WAIT_TIMEOUT);

        if (wifiSuccess)
        {
            Serial.println(" Proceso WiFi completado");

            if (rtcAvailable && wifiManager.isWiFiConnected())
            {
                if (rtcExterno.hasLostTime() || deepSleep.isFirstBoot())
                {
                    Serial.println("\n Sincronizando RTC con servidor NTP...");
                    if (rtcExterno.syncWithNTP("co.pool.ntp.org", -5))
                    {
                        Serial.println(" RTC sincronizado correctamente con NTP");
                        Serial.printf(" Nueva fecha/hora: %s\n", rtcExterno.getFormattedDateTime().c_str());
                    }
                    else
                    {
                        Serial.println(" No se pudo sincronizar RTC con NTP");
                    }
                }
            }

            String stats = wifiManager.getTransmissionStats();
            if (stats.indexOf("Datos enviados: 0") == -1)
            {
                Serial.println(" Datos descargados exitosamente por el usuario");
            }
            else
            {
                Serial.println(" No hubo solicitud de descarga");
            }

            watchdog.recordSuccess();
        }
        else
        {
            Serial.println(" Falló conexión WiFi");
            watchdog.logError(WatchdogManager::ERROR_WIFI_FAIL,
                            WatchdogManager::SEVERITY_WARNING, 0);
            watchdog.recordFailure();
        }

        // Mostrar estadísticas
        Serial.println(wifiManager.getTransmissionStats());

        watchdog.feedWatchdog();
        forceManualCheck = false;
    }
    else
    {
        Serial.printf(" Lecturas: %d/%d (WiFi check en %d lecturas)\n",
                    rtcMemory.getTotalReadings() % WIFI_CHECK_INTERVAL,
                    WIFI_CHECK_INTERVAL,
                    WIFI_CHECK_INTERVAL - (rtcMemory.getTotalReadings() % WIFI_CHECK_INTERVAL));
        Serial.println(" Sin verificación WiFi programada");
    }

    // ——— 14. MOSTRAR DATOS Y ERRORES ———
    rtcMemory.displayStoredReadings(5);
    watchdog.displayErrorLog(3);

    // ——— 15. VERIFICAR EMERGENCIA ———
    if (watchdog.getConsecutiveFailures() >= 10)
    {
        Serial.println(" DEMASIADOS FALLOS - MODO EMERGENCIA");
        watchdog.handleEmergency();
        deepSleep.goToSleepFor(300, true);
    }

    watchdog.feedWatchdog();

    // ——— 16. RESUMEN FINAL ———
    Serial.println("\n === RESUMEN DEL CICLO ===");

    Serial.println(" Lecturas de sensores:");
    if (tempReading.valid)
    {
        Serial.printf("    Temperatura: %.2f°C (VÁLIDA)\n", tempReading.temperature);
    }
    else
    {
        Serial.println("    Temperatura: --- (INVÁLIDA)");
    }

    if (tdsReading.valid)
    {
        Serial.printf("    TDS: %.1f ppm | EC: %.1f µS/cm (VÁLIDA)\n",
                    tdsReading.tds_value, tdsReading.ec_value);
    }
    else
    {
        Serial.println("    TDS: --- ppm (INVÁLIDA)");
    }

    if (turbidityReading.valid)
    {
        Serial.printf("    Turbidez: %.1f NTU | %s (VÁLIDA)\n",
                    turbidityReading.turbidity_ntu,
                    TurbiditySensor::getWaterQuality(turbidityReading.turbidity_ntu).c_str());
    }
    else
    {
        Serial.println("    Turbidez: --- NTU (INVÁLIDA)");
    }

    if (phReading.valid)
    {
        Serial.printf("    pH: %.2f | %s (VÁLIDA)\n",
                    phReading.ph_value,
                    pHSensor::getWaterType(phReading.ph_value).c_str());
    }
    else
    {
        Serial.println("    pH: -.-- (INVÁLIDA)");
    }

    Serial.println("\n === ESTADO RTC MAX31328 ===");
    if (rtcAvailable && rtcExterno.isPresent())
    {
        Serial.printf("Hora actual: %s\n", rtcExterno.getFormattedDateTime().c_str());
        Serial.printf("Unix timestamp: %u\n", rtcExterno.getUnixTimestamp());
        Serial.printf("Funcionando: %s\n", rtcExterno.isRunning() ? "Sí" : "No");

        if (rtcExterno.hasLostTime())
        {
            Serial.println(" RTC perdió la hora - Se sincronizará en próxima conexión WiFi");
        }
    }
    else
    {
        Serial.println(" RTC no disponible - usando timestamps relativos");
    }
    Serial.println("==========================");

    Serial.printf("\n Total lecturas almacenadas: %d\n", rtcMemory.getTotalReadings());
    Serial.printf(" Salud sistema: %d%%\n", watchdog.getHealthScore());
    Serial.printf(" Fallos consecutivos: %d\n", watchdog.getConsecutiveFailures());
    Serial.printf(" Próximo check WiFi en: %d lecturas\n",
                WIFI_CHECK_INTERVAL - (rtcMemory.getTotalReadings() % WIFI_CHECK_INTERVAL));

    uint64_t totalCycle, activeTime, sleepTime;
    deepSleep.getCycleInfo(totalCycle, activeTime, sleepTime);
    Serial.printf(" Duty Cycle: %.1f%% (%llu/%llu seg)\n",
                  (activeTime * 100.0) / totalCycle, activeTime, totalCycle);

    Serial.println("============================");

    // ——— 17. ENTRAR EN DEEP SLEEP ———
    Serial.printf("\n Entrando en Deep Sleep por %llu segundos\n",
                deepSleep.calculateSleepTime());
    Serial.printf(" Próximo despertar en %.1f minutos\n",
                deepSleep.calculateSleepTime() / 60.0);
    Serial.println("==========================================\n");

    delay(500);
    deepSleep.goToSleep(true);
}

void loop()
{
    // No se ejecuta con Deep Sleep
    Serial.println(" ERROR: No entró en Deep Sleep");
    delay(5000);
    ESP.restart();
}