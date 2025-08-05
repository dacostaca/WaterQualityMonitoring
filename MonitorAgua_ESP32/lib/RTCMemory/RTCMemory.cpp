#include "RTCMemory.h"
#include <stdarg.h>

// ——— Variables en RTC Memory ———
RTC_DATA_ATTR RTCMemoryManager::RTCDataStructure rtc_data;
RTC_DATA_ATTR int currentIndex = 0;
RTC_DATA_ATTR uint16_t totalReadings = 0;

// Constructor
RTCMemoryManager::RTCMemoryManager(bool enableSerial) 
    : _enableSerialOutput(enableSerial), _logCallback(nullptr) {
}

// Inicialización
void RTCMemoryManager::begin() {
    if (_enableSerialOutput && !Serial) {
        Serial.begin(115200);
        delay(100);
    }
    
    //log(" RTC Memory Manager Inicializado ");
}

// Validar integridad de datos RTC
bool RTCMemoryManager::validateIntegrity() {
    //log(" Validando integridad RTC...");
    
    // Verificar magic numbers
    if (rtc_data.magic_start != MAGIC_START || rtc_data.magic_end != MAGIC_END) {
        //log(" Magic numbers inválidos");
        return false;
    }
    
    // Verificar CRC del header
    uint32_t calculated_header_crc = calculateCRC32(&rtc_data.sequence_number, 
                                                   sizeof(uint32_t) * 2);
    if (rtc_data.header_crc != calculated_header_crc) {
        //logf(" Header CRC mismatch: calc=0x%08X, stored=0x%08X", 
          //   calculated_header_crc, rtc_data.header_crc);
        return false;
    }
    
    // Verificar CRC de datos
    uint32_t calculated_data_crc = calculateCRC32(&rtc_data.readings, 
                                                 sizeof(rtc_data.readings));
    if (rtc_data.data_crc != calculated_data_crc) {
        //logf(" Data CRC mismatch: calc=0x%08X, stored=0x%08X", 
          //   calculated_data_crc, rtc_data.data_crc);
        return false;
    }
    
    // Verificar rangos lógicos
    if (!validateLogicalRanges()) {
        return false;
    }
    
    //log(" Validación RTC exitosa");
    return true;
}

// Inicializar datos RTC
void RTCMemoryManager::initialize() {
    //log(" Inicializando estructura RTC...");
    
    // Limpiar toda la estructura
    memset(&rtc_data, 0, sizeof(RTCDataStructure));
    
    // Configurar magic numbers
    rtc_data.magic_start = MAGIC_START;
    rtc_data.magic_end = MAGIC_END;
    
    // Inicializar metadatos
    rtc_data.sequence_number = 1;
    rtc_data.boot_timestamp = millis();
    
    // Calcular CRCs iniciales
    updateCRCs();
    
    // Resetear contadores
    currentIndex = 0;
    totalReadings = 0;
    
    log(" RTC Memory inicializada correctamente");
}

// Almacenar lectura de sensores con validación
bool RTCMemoryManager::storeReading(const SensorReading &reading) {
    if (!reading.valid) {
        log(" No se almacena lectura inválida");
        return false;
    }
    
    // Intentar escribir con verificación
    uint32_t start_time = millis();
    
    // Copia de seguridad antes de escribir
    SensorReading backup;
    memcpy(&backup, (void*)&rtc_data.readings[currentIndex], sizeof(SensorReading));
    
    // Escribir datos
    memcpy((void*)&rtc_data.readings[currentIndex], &reading, sizeof(SensorReading));
    
    // Verificar escritura inmediatamente
    SensorReading verification;
    memcpy(&verification, (void*)&rtc_data.readings[currentIndex], sizeof(SensorReading));
    
    // Verificar timeout de escritura
    if (millis() - start_time > 100) {  // 100ms timeout para escritura
        logf(" Escritura RTC lenta: %u ms", millis() - start_time);
        // Nota: Error reporting manejado por WatchdogManager
    }
    
    // Verificar integridad de la escritura
    if (memcmp(&reading, &verification, sizeof(SensorReading)) != 0) {
        log(" Fallo en verificación de escritura RTC");
        // Restaurar backup
        memcpy((void*)&rtc_data.readings[currentIndex], &backup, sizeof(SensorReading));
        // Nota: Error reporting manejado por WatchdogManager
        return false;
    }
    
    // Actualizar metadatos
    rtc_data.sequence_number++;
    totalReadings++;
    
    // Actualizar CRCs
    updateCRCs();
    
    logf(" Lectura #%d almacenada en posición %d", 
         reading.reading_number, currentIndex);
    
    // Actualizar índice (buffer circular)
    currentIndex = (currentIndex + 1) % MAX_READINGS;
    
    return true;
}


// Crear lectura completa
RTCMemoryManager::SensorReading RTCMemoryManager::createFullReading(float temperature, float ph, float turbidity, float tds, float ec, uint8_t sensorStatus) {
    SensorReading reading = {0};
    reading.timestamp = millis();
    reading.temperature = temperature;
    reading.ph = ph;
    reading.turbidity = turbidity;
    reading.tds = tds;
    reading.ec = ec;
    reading.reading_number = totalReadings + 1;
    reading.sensor_status = sensorStatus;
    
    // Validar rangos de cada sensor
    bool tempValid = (temperature > -50.0 && temperature < 85.0);
    bool phValid = (ph >= 0.0 && ph <= 14.0);
    bool turbidityValid = (turbidity >= 0.0 && turbidity <= 3000.0);
    bool tdsValid = (tds >= 0.0 && tds <= 2000.0);
    bool ecValid = (ec >= 0.0 && ec <= 4000.0);
    
    reading.valid = tempValid && phValid && turbidityValid && tdsValid && ecValid;    ;
    
    return reading;
}

// Getters básicos
uint16_t RTCMemoryManager::getTotalReadings() { return totalReadings; }
int RTCMemoryManager::getCurrentIndex() { return currentIndex; }
uint32_t RTCMemoryManager::getSequenceNumber() { return rtc_data.sequence_number; }

// Verificar si debe enviar datos
bool RTCMemoryManager::shouldSendData(int readingsThreshold) {
    return (totalReadings % readingsThreshold == 0) && (totalReadings > 0);
}

// Marcar datos enviados
void RTCMemoryManager::markDataSent() {
    rtc_data.sequence_number++;
    updateCRCs();
    log(" Datos marcados como enviados");
}

// Obtener última lectura
bool RTCMemoryManager::getLastReading(SensorReading &reading) {
    if (totalReadings == 0) {
        return false;
    }
    
    int lastIndex = (currentIndex - 1 + MAX_READINGS) % MAX_READINGS;
    memcpy(&reading, (void*)&rtc_data.readings[lastIndex], sizeof(SensorReading));
    
    return reading.valid;
}

// Obtener múltiples lecturas recientes
int RTCMemoryManager::getRecentReadings(SensorReading readings[], int maxReadings) {
    int count = 0;
    int totalAvailable = (totalReadings < MAX_READINGS) ? totalReadings : MAX_READINGS;
    int toRetrieve = (maxReadings < totalAvailable) ? maxReadings : totalAvailable;
    
    logf(" getRecentReadings: Solicitados=%d, Disponibles=%d, ARecuperar=%d", 
         maxReadings, totalAvailable, toRetrieve);
    
    // Crear array dinámico del tamaño correcto
    SensorReading* tempBuffer = new SensorReading[toRetrieve];
    int tempCount = 0;
    
    // Recopilar lecturas válidas desde la más reciente hacia atrás
    for (int i = 0; i < toRetrieve && tempCount < toRetrieve; i++) {
        int index = (currentIndex - 1 - i + MAX_READINGS) % MAX_READINGS;
        SensorReading temp;
        memcpy(&temp, (void*)&rtc_data.readings[index], sizeof(SensorReading));
        
        if (temp.valid && temp.reading_number > 0) {
            tempBuffer[tempCount] = temp;
            tempCount++;
        }
    }
    
    logf(" Lecturas válidas recuperadas: %d de %d solicitadas", tempCount, toRetrieve);
    
    // Copiar en orden cronológico (reversa del buffer)
    for (int i = tempCount - 1; i >= 0; i--) {
        readings[count] = tempBuffer[i];
        count++;
    }
    
    // Liberar memoria
    delete[] tempBuffer;
    
    logf(" getRecentReadings completado: %d lecturas retornadas", count);
    return count;
}


// Mostrar lecturas almacenadas
void RTCMemoryManager::displayStoredReadings(int numReadings) {
    log("\n --- DATOS ALMACENADOS EN RTC MEMORY ---");
    logf("Total lecturas: %d | Posición actual: %d", 
         totalReadings, currentIndex);
    
    // Mostrar últimas N lecturas válidas
    int shown = 0;
    log("Últimas lecturas:");
    
    for (int i = 0; i < MAX_READINGS && shown < numReadings; i++) {
        int index = (currentIndex - 1 - i + MAX_READINGS) % MAX_READINGS;
        SensorReading temp;
        memcpy(&temp, (void*)&rtc_data.readings[index], sizeof(SensorReading));
        
        if (temp.valid && temp.reading_number > 0) {
            logf("  [%d] #%d: T:%.1f°C pH:%.1f Turb:%.1f TDS:%.0f EC:%.1f | Status:0x%02X | %ums",
                 index, temp.reading_number, temp.temperature, temp.ph, 
                 temp.turbidity, temp.tds, temp.ec, temp.sensor_status, temp.timestamp);
            shown++;
        }
    }
    
    if (shown == 0) {
        log("   No hay lecturas válidas");
    }
    
    log("---------------------------------------");
}

// Reset completo
void RTCMemoryManager::forceCompleteReset() {
    log(" FORZANDO RESET COMPLETO DEL SISTEMA...");
    log(" Todos los datos RTC serán eliminados");
    
    // Limpiar completamente toda la estructura RTC
    memset(&rtc_data, 0, sizeof(RTCDataStructure));
    currentIndex = 0;
    totalReadings = 0;
    
    log(" Reset completo realizado");
    log("El sistema se reiniciará como primera ejecución");
}

// Obtener estado
String RTCMemoryManager::getStatus() {
    String status = "=== RTC Memory Manager Status ===\n";
    status += "Total lecturas: " + String(totalReadings) + "\n";
    status += "Índice actual: " + String(currentIndex) + "\n";
    status += "Secuencia: " + String(rtc_data.sequence_number) + "\n";
    status += "Inicializado: " + String(isInitialized() ? "Sí" : "No") + "\n";
    status += "Tamaño estructura: " + String(sizeof(RTCDataStructure)) + " bytes\n";
    status += "SOLO DATOS - Sin logging de errores\n";
    status += "================================";
    
    return status;
}

// Obtener uso de memoria
String RTCMemoryManager::getMemoryUsage() {
    String usage = "=== Memory Usage ===\n";
    usage += "Estructura RTC: " + String(sizeof(RTCDataStructure)) + " bytes\n";
    usage += "Buffer lecturas: " + String(sizeof(rtc_data.readings)) + " bytes\n";
    usage += "Solo datos de sensores - sin buffers de errores\n";
    
    // Calcular uso actual de buffer de lecturas
    int usedReadings = (totalReadings < MAX_READINGS) ? totalReadings : MAX_READINGS;
    
    usage += "Lecturas usadas: " + String(usedReadings) + "/" + String(MAX_READINGS) + "\n";
    usage += "===================";
    
    return usage;
}

// Habilitar/deshabilitar Serial
void RTCMemoryManager::enableSerial(bool enable) {
    _enableSerialOutput = enable;
}

// Configurar callback de logging
void RTCMemoryManager::setLogCallback(LogCallback callback) {
    _logCallback = callback;
}

// Verificar si está inicializada
bool RTCMemoryManager::isInitialized() {
    return (rtc_data.magic_start == MAGIC_START && rtc_data.magic_end == MAGIC_END);
}

// ——— MÉTODOS PRIVADOS ———

// Calcular CRC32
uint32_t RTCMemoryManager::calculateCRC32(const void* data, size_t length) {
    return esp_crc32_le(0xFFFFFFFF, (const uint8_t*)data, length) ^ 0xFFFFFFFF;
}

// Actualizar CRCs
void RTCMemoryManager::updateCRCs() {
    rtc_data.header_crc = calculateCRC32(&rtc_data.sequence_number, sizeof(uint32_t) * 2);
    rtc_data.data_crc = calculateCRC32(&rtc_data.readings, sizeof(rtc_data.readings));
}

// Validar rangos lógicos
bool RTCMemoryManager::validateLogicalRanges() {
    if (currentIndex < 0 || currentIndex >= MAX_READINGS) {
        logf(" currentIndex fuera de rango: %d", currentIndex);
        return false;
    }
    
    if (totalReadings > MAX_TOTAL_READINGS) {
        logf(" totalReadings sospechoso: %d", totalReadings);
        return false;
    }
    
    return true;
}

// Métodos privados de logging
void RTCMemoryManager::log(const char* message) {
    if (_logCallback) {
        _logCallback(message);
    } else if (_enableSerialOutput && Serial) {
        Serial.println(message);
    }
}

void RTCMemoryManager::logf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    log(buffer);
}