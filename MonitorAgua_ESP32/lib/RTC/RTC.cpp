#include "RTC.h"

MAX31328RTC::MAX31328RTC() {
    i2c_address = MAX31328_I2C_ADDRESS;
    initialized = false;
}

bool MAX31328RTC::begin(int sda_pin, int scl_pin, uint8_t address) {
    i2c_address = address;
    
    //Serial.printf("MAX31328: Inicializando I2C (SDA=%d, SCL=%d, Addr=0x%02X)\n", 
                  //sda_pin, scl_pin, i2c_address);
    
    // Inicializar I2C con pines específicos
    Wire.end(); // Terminar sesión previa si existe
    delay(100);
    
    if (!Wire.begin(sda_pin, scl_pin)) {
        Serial.println("MAX31328: Error inicializando I2C");
        return false;
    }
    
    // Configurar velocidad I2C (100kHz para mayor compatibilidad)
    Wire.setClock(MAX31328_I2C_SPEED);
    delay(200); // Dar tiempo para estabilización
    
    // Verificar presencia del dispositivo
    if (!isPresent()) {
        Serial.println("MAX31328: Dispositivo no detectado en I2C");
        return false;
    }
    
    Serial.println("MAX31328: Dispositivo detectado correctamente");
    
    // Verificar si el oscilador está funcionando
    if (!isRunning()) {
        //Serial.println("MAX31328: Oscilador detenido - Iniciando...");
        if (!startOscillator()) {
            Serial.println("MAX31328: Error iniciando oscilador");
            return false;
        }
        
    }
    
    initialized = true;
    Serial.println("MAX31328: Inicialización completada");
    
    return true;
}

bool MAX31328RTC::isPresent() {
    Wire.beginTransmission(i2c_address);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
        // Intentar leer el registro de estado para confirmar
        uint8_t status = readRegister(MAX31328_REG_STATUS);
        
        if (status == 0xFF) {
            Serial.println("MAX31328: Registro de estado inválido (0xFF)");
            return false;
        }
        
        Serial.printf("MAX31328: Dispositivo presente - Status: 0x%02X\n", status);
        return true;
    }
    
    Serial.printf("MAX31328: Error I2C: %d\n", error);
    switch (error) {
        case 2:
            Serial.println("MAX31328: NACK en dirección - dispositivo no responde");
            break;
        case 4:
            Serial.println("MAX31328: Error desconocido en I2C");
            break;
        case 5:
            Serial.println("MAX31328: Timeout en I2C");
            break;
        default:
            Serial.printf("MAX31328: Error I2C no documentado: %d\n", error);
            break;
    }
    
    return false;
}

bool MAX31328RTC::isRunning() {
    if (!initialized && !isPresent()) {
        return false;
    }
    
    uint8_t status = readRegister(MAX31328_REG_STATUS);
    
    // El bit OSF (bit 7) indica si el oscilador se detuvo
    bool running = !(status & MAX31328_STAT_OSF);
    
    Serial.printf("MAX31328: Oscilador %s (Status: 0x%02X)\n", 
                running ? "funcionando" : "detenido", status);
    
    return running;
}

bool MAX31328RTC::startOscillator() {
    Serial.println("MAX31328: Iniciando oscilador...");
    
    // Leer registro de control
    uint8_t control = readRegister(MAX31328_REG_CONTROL);
    Serial.printf("MAX31328: Control actual: 0x%02X\n", control);
    
    // Habilitar oscilador (EOSC = 0)
    control &= ~MAX31328_CTRL_EOSC;
    
    if (!writeRegister(MAX31328_REG_CONTROL, control)) {
        Serial.println("MAX31328: Error escribiendo registro de control");
        return false;
    }
    
    // Limpiar flag OSF
    if (!clearLostTimeFlag()) {
        Serial.println("MAX31328: Error limpiando flag OSF");
        return false;
    }
    
    delay(1000); // Dar tiempo para que se estabilice
    
    return isRunning();
}

bool MAX31328RTC::hasLostTime() {
    uint8_t status = readRegister(MAX31328_REG_STATUS);
    return (status & MAX31328_STAT_OSF) != 0;
}

bool MAX31328RTC::clearLostTimeFlag() {
    uint8_t status = readRegister(MAX31328_REG_STATUS);
    status &= ~MAX31328_STAT_OSF; // Limpiar bit OSF
    return writeRegister(MAX31328_REG_STATUS, status);
}

bool MAX31328RTC::setDateTime(uint16_t year, uint8_t month, uint8_t day, 
                            uint8_t hour, uint8_t minute, uint8_t second) {
    
    if (!initialized && !isPresent()) {
        Serial.println("MAX31328: RTC no inicializado");
        return false;
    }
    
    // Validar rangos
    if (year < 2000 || year > 2099 || month < 1 || month > 12 || 
        day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59) {
        Serial.println("MAX31328: Fecha/hora fuera de rango");
        return false;
    }
    
    Serial.printf("MAX31328: Configurando %04d-%02d-%02d %02d:%02d:%02d\n",
                year, month, day, hour, minute, second);
    
    // Preparar datos en formato BCD
    uint8_t timeRegs[7];
    timeRegs[0] = decToBcd(second);
    timeRegs[1] = decToBcd(minute);
    timeRegs[2] = decToBcd(hour);       
    timeRegs[3] = 1;                     
    timeRegs[4] = decToBcd(day);
    timeRegs[5] = decToBcd(month);
    timeRegs[6] = decToBcd(year - 2000);
    
    // Escribir todos los registros de tiempo
    if (!writeMultipleRegisters(MAX31328_REG_SECONDS, timeRegs, 7)) {
        Serial.println("MAX31328: Error escribiendo fecha/hora");
        return false;
    }
    
    // Limpiar flag OSF
    clearLostTimeFlag();
    
    Serial.println("MAX31328: Fecha/hora configurada correctamente");
    return true;
}

bool MAX31328RTC::getDateTime(uint16_t& year, uint8_t& month, uint8_t& day,
                            uint8_t& hour, uint8_t& minute, uint8_t& second) {
    
    if (!initialized && !isPresent()) {
        return false;
    }
    
    // Leer registros de tiempo
    uint8_t timeRegs[7];
    if (!readMultipleRegisters(MAX31328_REG_SECONDS, timeRegs, 7)) {
        Serial.println("MAX31328: Error leyendo fecha/hora");
        return false;
    }
    
    // Convertir de BCD a decimal
    second = bcdToDec(timeRegs[0] & 0x7F);  
    minute = bcdToDec(timeRegs[1] & 0x7F);
    hour = bcdToDec(timeRegs[2] & 0x3F);    
    day = bcdToDec(timeRegs[4] & 0x3F);
    month = bcdToDec(timeRegs[5] & 0x1F);   
    year = 2000 + bcdToDec(timeRegs[6]);
    
    return true;
}

uint32_t MAX31328RTC::getUnixTimestamp() {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    
    if (!getDateTime(year, month, day, hour, minute, second)) {
        return 0;
    }
    
    // Convertir a timestamp Unix
    struct tm timeinfo = {0};
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    timeinfo.tm_isdst = -1;
    
    time_t timestamp = mktime(&timeinfo);
    return (uint32_t)timestamp;
}

bool MAX31328RTC::setUnixTimestamp(uint32_t timestamp) {
    time_t rawtime = timestamp;
    struct tm* timeinfo = localtime(&rawtime);
    
    return setDateTime(
        timeinfo->tm_year + 1900,
        timeinfo->tm_mon + 1,
        timeinfo->tm_mday,
        timeinfo->tm_hour,
        timeinfo->tm_min,
        timeinfo->tm_sec
    );
}

float MAX31328RTC::getTemperature() {
    if (!initialized && !isPresent()) {
        return -999.0f;
    }
    
    // Leer registros de temperatura
    uint8_t tempMSB = readRegister(MAX31328_REG_TEMP_MSB);
    uint8_t tempLSB = readRegister(MAX31328_REG_TEMP_LSB);
    
    // Convertir a temperatura 
    int16_t tempRaw = (tempMSB << 8) | tempLSB;
    tempRaw >>= 6;  
    
    float temperature = tempRaw * 0.25f;  
    
    return temperature;
}

String MAX31328RTC::getFormattedDateTime() {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    
    if (!getDateTime(year, month, day, hour, minute, second)) {
        return "Error leyendo RTC";
    }
    
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
            year, month, day, hour, minute, second);
    
    return String(buffer);
}

bool MAX31328RTC::syncWithNTP(const char* ntpServer, int gmtOffset) {
    Serial.println("MAX31328: Sincronizando con NTP...");
    
    configTime(gmtOffset * 3600, 0, ntpServer);
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10000)) {  // 10 segundos timeout
        Serial.println("MAX31328: Error obteniendo tiempo NTP");
        return false;
    }
    
    // Actualizar RTC con tiempo NTP
    bool result = setDateTime(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec
    );
    
    if (result) {
        Serial.println("MAX31328: Sincronizado con NTP exitosamente");
    }
    
    return result;
}

void MAX31328RTC::printDebugInfo() {
    Serial.println("=== MAX31328 DEBUG INFO ===");
    Serial.printf("Inicializado: %s\n", initialized ? "Sí" : "No");
    Serial.printf("Dirección I2C: 0x%02X\n", i2c_address);
    Serial.printf("Presente: %s\n", isPresent() ? "Sí" : "No");
    Serial.printf("Funcionando: %s\n", isRunning() ? "Sí" : "No");
    Serial.printf("Tiempo perdido: %s\n", hasLostTime() ? "Sí" : "No");
    
    if (isPresent()) {
        Serial.printf("Fecha/Hora: %s\n", getFormattedDateTime().c_str());
        Serial.printf("Unix timestamp: %u\n", getUnixTimestamp());
        Serial.printf("Temperatura: %.2f°C\n", getTemperature());
        
        printRegisters();
    }
    
    Serial.println("==========================");
}

void MAX31328RTC::printRegisters() {
    Serial.println("Registros principales:");
    
    // Registros de tiempo
    for (uint8_t i = 0; i <= 6; i++) {
        uint8_t value = readRegister(i);
        Serial.printf("  0x%02X: 0x%02X (%d BCD)\n", i, value, bcdToDec(value & 0x7F));
    }
    
    // Registros de control
    uint8_t control = readRegister(MAX31328_REG_CONTROL);
    uint8_t status = readRegister(MAX31328_REG_STATUS);
    Serial.printf("  Control (0x0E): 0x%02X\n", control);
    Serial.printf("  Status (0x0F): 0x%02X\n", status);
}

// ——— FUNCIONES AUXILIARES ———

uint8_t MAX31328RTC::decToBcd(uint8_t val) {
    return ((val / 10) << 4) + (val % 10);
}

uint8_t MAX31328RTC::bcdToDec(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

bool MAX31328RTC::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(i2c_address);
    Wire.write(reg);
    Wire.write(value);
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("MAX31328: Error escribiendo reg 0x%02X: %d\n", reg, error);
        return false;
    }
    
    return true;
}

uint8_t MAX31328RTC::readRegister(uint8_t reg) {
    Wire.beginTransmission(i2c_address);
    Wire.write(reg);
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("MAX31328: Error en transmisión reg 0x%02X: %d\n", reg, error);
        return 0xFF;
    }
    
    Wire.requestFrom(i2c_address, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    
    Serial.printf("MAX31328: Sin datos disponibles reg 0x%02X\n", reg);
    return 0xFF;
}

bool MAX31328RTC::writeMultipleRegisters(uint8_t startReg, uint8_t* buffer, uint8_t length) {
    Wire.beginTransmission(i2c_address);
    Wire.write(startReg);
    
    for (uint8_t i = 0; i < length; i++) {
        Wire.write(buffer[i]);
    }
    
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("MAX31328: Error escribiendo múltiples registros desde 0x%02X: %d\n", 
                    startReg, error);
        return false;
    }
    
    return true;
}

bool MAX31328RTC::readMultipleRegisters(uint8_t startReg, uint8_t* buffer, uint8_t length) {
    Wire.beginTransmission(i2c_address);
    Wire.write(startReg);
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("MAX31328: Error en transmisión múltiple reg 0x%02X: %d\n", 
                    startReg, error);
        return false;
    }
    
    Wire.requestFrom(i2c_address, length);
    
    for (uint8_t i = 0; i < length; i++) {
        if (Wire.available()) {
            buffer[i] = Wire.read();
        } else {
            Serial.printf("MAX31328: Datos insuficientes en lectura múltiple\n");
            return false;
        }
    }
    
    return true;
}