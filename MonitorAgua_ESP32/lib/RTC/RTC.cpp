#include "RTC.h" // Incluye el archivo de cabecera.

MAX31328RTC::MAX31328RTC() { // Constructor de la clase MAX31328RTC. Se ejecuta al crear un objeto de este tipo.
    i2c_address = MAX31328_I2C_ADDRESS; // Asigna la dirección I2C por defecto del chip RTC (definida en RTC.h).
    initialized = false; // Inicializa el estado del dispositivo como no inicializado.
}

bool MAX31328RTC::begin(int sda_pin, int scl_pin, uint8_t address) {
    // Función de inicio: configura la comunicación I2C y valida la presencia del RTC.
    i2c_address = address; // Se guarda la dirección I2C indicada por el usuario o por defecto.
    
    //Serial.printf("MAX31328: Inicializando I2C (SDA=%d, SCL=%d, Addr=0x%02X)\n", 
                  //sda_pin, scl_pin, i2c_address);
    
    // Inicializar I2C con pines específicos
    Wire.end(); // Terminar sesión previa si existe (// Finaliza cualquier comunicación I2C previa para reiniciar la conexión limpia.)
    delay(100); // Pequeña pausa para estabilizar el bus I2C después de cerrarlo.
    
    if (!Wire.begin(sda_pin, scl_pin)) { // Intenta iniciar la comunicación I2C en los pines especificados.
        Serial.println("MAX31328: Error inicializando I2C"); // Muestra error si no logra inicializar.
        return false; // Devuelve falso para indicar que no se pudo inicializar.
    }
    
    // Configurar velocidad I2C (100kHz para mayor compatibilidad)
    Wire.setClock(MAX31328_I2C_SPEED); // Configura la velocidad de transmisión I2C (ejemplo: 100kHz para mayor compatibilidad).
    delay(200); // Dar tiempo para estabilización
    
    // Verificar presencia del dispositivo
    if (!isPresent()) { // Verifica si el dispositivo está presente en el bus I2C.
        Serial.println("MAX31328: Dispositivo no detectado en I2C"); // Si no se encuentra, muestra error.
        return false; // Retorna falso porque el RTC no está conectado o no responde.
    }
    
    Serial.println("MAX31328: Dispositivo detectado correctamente"); // Mensaje indicando que el RTC respondió bien.
    
    // Verificar si el oscilador está funcionando
    if (!isRunning()) { // Verifica si el oscilador interno del RTC está funcionando.
        //Serial.println("MAX31328: Oscilador detenido - Iniciando...");
        if (!startOscillator()) { // Intenta iniciar el oscilador si estaba apagado.
            Serial.println("MAX31328: Error iniciando oscilador"); // Si falla, muestra mensaje de error.
            return false; // Retorna falso indicando que no se pudo iniciar el oscilador.
        }
        
    }
    
    initialized = true; // Marca el dispositivo como inicializado exitosamente.
    Serial.println("MAX31328: Inicialización completada"); // Mensaje de éxito en la inicialización.
    
    return true; // Devuelve verdadero, indicando que el RTC está listo para usarse.
}

bool MAX31328RTC::isPresent() { // Función que verifica si el dispositivo RTC está presente en el bus I2C
    Wire.beginTransmission(i2c_address); // Inicia la comunicación I2C con la dirección configurada
    uint8_t error = Wire.endTransmission(); // Finaliza la transmisión y obtiene el código de error
    
    if (error == 0) { // Si no hubo error, significa que el dispositivo respondió correctamente
        // Intentar leer el registro de estado para confirmar
        uint8_t status = readRegister(MAX31328_REG_STATUS); // Lee el registro de estado del RTC
        
        if (status == 0xFF) { // 0xFF indica una lectura inválida (error de comunicación o sin datos)
            Serial.println("MAX31328: Registro de estado inválido (0xFF)"); // Mensaje de error por puerto serie
            return false; // El dispositivo no respondió correctamente
        }
        
        Serial.printf("MAX31328: Dispositivo presente - Status: 0x%02X\n", status); // Imprime el estado leído  
        return true; // Retorna que el dispositivo está presente
    }
    
    Serial.printf("MAX31328: Error I2C: %d\n", error); // Si hubo error en la comunicación I2C, imprime el código
    switch (error) { // Evalúa el tipo de error según el valor devuelto
        case 2: // Error código 2
            Serial.println("MAX31328: NACK en dirección - dispositivo no responde"); // No hubo ACK en la dirección
            break;
        case 4: // Error código 4
            Serial.println("MAX31328: Error desconocido en I2C"); // Se detectó un error no identificado
            break;
        case 5: // Error código 5
            Serial.println("MAX31328: Timeout en I2C"); // Tiempo de espera agotado en la comunicación
            break;
        default: // Cualquier otro error no documentado
            Serial.printf("MAX31328: Error I2C no documentado: %d\n", error); // Muestra el error tal cual
            break;
    }
    
    return false; // Si llegamos aquí, el dispositivo no está presente o no respondió bien
}

bool MAX31328RTC::isRunning() { // Función que verifica si el oscilador interno del RTC está funcionando
    if (!initialized && !isPresent()) { // Si no está inicializado y tampoco se detecta el dispositivo
        return false; // No está funcionando
    }
    
    uint8_t status = readRegister(MAX31328_REG_STATUS); // Lee el registro de estado del RTC
    
    // El bit OSF (bit 7) indica si el oscilador se detuvo
    bool running = !(status & MAX31328_STAT_OSF); // running será true si el bit OSF está en 0 (oscilador activo)
    
    Serial.printf("MAX31328: Oscilador %s (Status: 0x%02X)\n", 
                  running ? "funcionando" : "detenido", status); // Imprime si está activo o detenido
    
    return running; // Retorna el estado del oscilador
}

bool MAX31328RTC::startOscillator() { // Función que inicia el oscilador del RTC si estaba detenido
    Serial.println("MAX31328: Iniciando oscilador..."); // Mensaje informativo
    
    // Leer registro de control
    uint8_t control = readRegister(MAX31328_REG_CONTROL); // Se obtiene el valor actual del registro de control
    Serial.printf("MAX31328: Control actual: 0x%02X\n", control); // Se imprime el valor leído
    
    // Habilitar oscilador (EOSC = 0). Se limpia el bit de apagado del oscilador
    control &= ~MAX31328_CTRL_EOSC; // Apaga el bit EOSC para activar el oscilador
    
    if (!writeRegister(MAX31328_REG_CONTROL, control)) { // Escribe el nuevo valor en el registro de control
        Serial.println("MAX31328: Error escribiendo registro de control"); // Si falla, muestra error
        return false; // Y retorna false
    }
    
    // Limpiar flag OSF
    if (!clearLostTimeFlag()) { // Se intenta borrar el flag que indica pérdida de tiempo
        Serial.println("MAX31328: Error limpiando flag OSF"); // Error al limpiar el flag
        return false; // Retorna falso si no pudo
    }
    
    delay(1000); // Dar tiempo para que se estabilice. Se espera 1 segundo para que el oscilador se estabilice
    
    return isRunning(); // Se comprueba y retorna si el oscilador ya está corriendo
}

bool MAX31328RTC::hasLostTime() { // Indica si el RTC ha marcado que perdió tiempo (OSF)
    uint8_t status = readRegister(MAX31328_REG_STATUS); // Lee el registro STATUS
    return (status & MAX31328_STAT_OSF) != 0; // Retorna true si el bit OSF está activo
}

bool MAX31328RTC::clearLostTimeFlag() { // Limpia el flag de pérdida de tiempo (OSF)
    uint8_t status = readRegister(MAX31328_REG_STATUS); // Lee STATUS
    status &= ~MAX31328_STAT_OSF; // Limpiar bit OSF. - Borra el bit OSF para indicar que se reseteó la condición
    return writeRegister(MAX31328_REG_STATUS, status); // Escribe nuevamente STATUS y retorna si tuvo éxito
}

bool MAX31328RTC::setDateTime(uint16_t year, uint8_t month, uint8_t day, 
                              uint8_t hour, uint8_t minute, uint8_t second) { // Configura fecha y hora en el RTC
    
    if (!initialized && !isPresent()) { // Verifica que el dispositivo esté inicializado o presente
        Serial.println("MAX31328: RTC no inicializado"); // Mensaje de error si no lo está
        return false; // No continuar si no está listo
    }
    
    // Validar rangos
    if (year < 2000 || year > 2099 || month < 1 || month > 12 || 
        day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59) { // Comprueba límites razonables
        Serial.println("MAX31328: Fecha/hora fuera de rango"); // Log si alguno está fuera de rango
        return false; // Rechaza configuración inválida
    }
    
    Serial.printf("MAX31328: Configurando %04d-%02d-%02d %02d:%02d:%02d\n", 
                  year, month, day, hour, minute, second); // Muestra la fecha/hora que se va a programar
    
    // Preparar datos en formato BCD
    uint8_t timeRegs[7]; // Buffer con los 7 registros (seg, min, hora, diaSem, dia, mes, año)
    timeRegs[0] = decToBcd(second); // Segundos en BCD
    timeRegs[1] = decToBcd(minute); // Minutos en BCD
    timeRegs[2] = decToBcd(hour);   // Hora en BCD     
    timeRegs[3] = 1;                // Día de la semana: se coloca 1 (valor no crítico en este uso)                    
    timeRegs[4] = decToBcd(day);    // Día del mes en BCD
    timeRegs[5] = decToBcd(month);  // Mes en BCD
    timeRegs[6] = decToBcd(year - 2000); // Año offset 2000 en BCD
    
    // Escribir todos los registros de tiempo
    if (!writeMultipleRegisters(MAX31328_REG_SECONDS, timeRegs, 7)) { // Escribe desde registro SECONDS los 7 bytes
        Serial.println("MAX31328: Error escribiendo fecha/hora"); // Error si falla escritura
        return false; // Indica fallo
    }
    
    // Limpiar flag OSF
    clearLostTimeFlag(); // Limpia el flag de pérdida de tiempo después de configurar fecha/hora
    
    Serial.println("MAX31328: Fecha/hora configurada correctamente"); // Confirmación
    return true; // Operación exitosa
}

bool MAX31328RTC::getDateTime(uint16_t& year, uint8_t& month, uint8_t& day,
                              uint8_t& hour, uint8_t& minute, uint8_t& second) {
    
    if (!initialized && !isPresent()) {
        return false;
    }
    
    // Leer registros de tiempo
    uint8_t timeRegs[7];
    if (!readMultipleRegisters(MAX31328_REG_SECONDS, timeRegs, 7)) { // Leer varios registros desde SECONDS
        Serial.println("MAX31328: Error leyendo fecha/hora"); // Error de lectura
        return false; // Retorna fallo
    }
    
    // Convertir de BCD a decimal
    second = bcdToDec(timeRegs[0] & 0x7F);  // Segundos: mask 0x7F para limpiar flag de reloj
    minute = bcdToDec(timeRegs[1] & 0x7F);  // Minutos
    hour = bcdToDec(timeRegs[2] & 0x3F);    // Hora: mask 0x3F para manejar formato 24h/12h
    day = bcdToDec(timeRegs[4] & 0x3F);     // Día del mes
    month = bcdToDec(timeRegs[5] & 0x1F);   // Mes: mask 0x1F por si hay flags en los bits superiores
    year = 2000 + bcdToDec(timeRegs[6]);    // Año: se suma 2000 al BCD almacenado (offset)
    
    return true;
}

uint32_t MAX31328RTC::getUnixTimestamp() { // Devuelve timestamp Unix (segundos desde 1970)
    uint16_t year;
    uint8_t month, day, hour, minute, second; // Variables locales para recibir fecha/hora del RTC
    
    if (!getDateTime(year, month, day, hour, minute, second)) { // Llama a getDateTime; si falla...
        return 0; // Retorna 0 para indicar error
    }
    
    // Convertir a timestamp Unix
    struct tm timeinfo = {0};       // Estructura tm inicializada a cero
    timeinfo.tm_year = year - 1900; // tm_year espera años desde 1900
    timeinfo.tm_mon = month - 1;    // tm_mon es 0-11
    timeinfo.tm_mday = day;         // Día del mes 1-31
    timeinfo.tm_hour = hour;        // Hora 0-23
    timeinfo.tm_min = minute;       // Minutos 0-59
    timeinfo.tm_sec = second;       // Segundos 0-59
    timeinfo.tm_isdst = -1;         // Desconoce info de DST (dejar que mktime lo determine)

    time_t timestamp = mktime(&timeinfo); // Convierte tm a time_t (segundos desde 1970)
    return (uint32_t)timestamp;           // Retorna resultado como uint32_t
}

bool MAX31328RTC::setUnixTimestamp(uint32_t timestamp) { // Establece fecha/hora a partir de timestamp Unix
    time_t rawtime = timestamp; // Convierte a tipo time_t
    struct tm* timeinfo = localtime(&rawtime); // Obtiene estructura tm local desde timestamp
    
    return setDateTime( // Llama a setDateTime con los campos extraídos de timeinfo
        timeinfo->tm_year + 1900, 
        timeinfo->tm_mon + 1,
        timeinfo->tm_mday,
        timeinfo->tm_hour,
        timeinfo->tm_min,
        timeinfo->tm_sec
    );
}

float MAX31328RTC::getTemperature() { // Lee temperatura interna del RTC y la convierte a grados C
    if (!initialized && !isPresent()) { // Si no está listo o no responde...
        return -999.0f; // Retorna valor sentinela que indica error de lectura
    }
    
    // Leer registros de temperatura
    uint8_t tempMSB = readRegister(MAX31328_REG_TEMP_MSB); // Lee el byte MSB de temperatura
    uint8_t tempLSB = readRegister(MAX31328_REG_TEMP_LSB); // Lee el byte LSB de temperatura
    
    // Convertir a temperatura 
    int16_t tempRaw = (tempMSB << 8) | tempLSB; // Combina MSB y LSB en un entero de 16 bits
    tempRaw >>= 6;   // Ajusta los bits según formato del MAX31328 (temperatura en 10-bits con signo)
    
    float temperature = tempRaw * 0.25f;  // Cada increment representa 0.25°C → multiplicar por 0.25 
    
    return temperature; // Retorna temperatura en °C
}

String MAX31328RTC::getFormattedDateTime() { // Devuelve una cadena formateada "YYYY-MM-DD HH:MM:SS"
    uint16_t year;
    uint8_t month, day, hour, minute, second; // Variables locales para recibir la fecha
    
    if (!getDateTime(year, month, day, hour, minute, second)) { // Si falla la lectura...
        return "Error leyendo RTC"; // Retorna mensaje de error
    }
    
    char buffer[20]; // Buffer para formato de fecha/hora (suficiente para "YYYY-MM-DD HH:MM:SS")
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", 
             year, month, day, hour, minute, second); // Formatea la fecha en el buffer
    
    return String(buffer); // Convierte el buffer C a String de Arduino y lo retorna
}

bool MAX31328RTC::syncWithNTP(const char* ntpServer, int gmtOffset) { // Sincroniza el RTC usando NTP
    Serial.println("MAX31328: Sincronizando con NTP..."); // Mensaje indicativo
    
    configTime(gmtOffset * 3600, 0, ntpServer); // Configura la librería de tiempo del sistema para NTP (offset en segundos)
    
    struct tm timeinfo; // Estructura para recibir la hora local del sistema
    if (!getLocalTime(&timeinfo, 10000)) {  // 10 segundos timeout -  10 segundos timeout para obtener tiempo NTP
        Serial.println("MAX31328: Error obteniendo tiempo NTP"); // Error si no llega respuesta NTP
        return false; // Retorna fallo si no hay respuesta
    }
    
    // Actualizar RTC con tiempo NTP
    bool result = setDateTime( // Actualiza RTC con la hora NTP obtenida
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec
    );
    
    if (result) { // Si la actualización fue exitosa...
        Serial.println("MAX31328: Sincronizado con NTP exitosamente"); // Mensaje
    }
    
    return result; // Retorna true/false según resultado
}

void MAX31328RTC::printDebugInfo() { // Imprime información de diagnóstico del RTC
    Serial.println("=== MAX31328 DEBUG INFO ==="); 
    Serial.printf("Inicializado: %s\n", initialized ? "Sí" : "No"); // Imprime si objeto fue inicializado
    Serial.printf("Dirección I2C: 0x%02X\n", i2c_address); // Imprime dirección I2C
    Serial.printf("Presente: %s\n", isPresent() ? "Sí" : "No"); // Indica si el dispositivo responde
    Serial.printf("Funcionando: %s\n", isRunning() ? "Sí" : "No"); // Indica si oscilador corre
    Serial.printf("Tiempo perdido: %s\n", hasLostTime() ? "Sí" : "No"); // Indica bandera OSF
    
    if (isPresent()) { // Si el dispositivo responde...
        Serial.printf("Fecha/Hora: %s\n", getFormattedDateTime().c_str()); // Imprime fecha formateada
        Serial.printf("Unix timestamp: %u\n", getUnixTimestamp()); // Imprime timestamp
        Serial.printf("Temperatura: %.2f°C\n", getTemperature()); // Imprime temperatura interna
        
        printRegisters(); // Imprime registros para depuración
    }
    
    Serial.println("==========================");
}

void MAX31328RTC::printRegisters() { // Lee e imprime registros principales del RTC
    Serial.println("Registros principales:");
    
    // Registros de tiempo
    for (uint8_t i = 0; i <= 6; i++) { // Itera primeros 7 registros (segundos..año)
        uint8_t value = readRegister(i); // Lee registro i
        Serial.printf("  0x%02X: 0x%02X (%d BCD)\n", i, value, bcdToDec(value & 0x7F)); // Imprime valor y su conversión BCD
    }
    
    // Registros de control
    uint8_t control = readRegister(MAX31328_REG_CONTROL);   // Lee registro de control
    uint8_t status = readRegister(MAX31328_REG_STATUS);     // Lee registro de status
    Serial.printf("  Control (0x0E): 0x%02X\n", control);   // Imprime control
    Serial.printf("  Status (0x0F): 0x%02X\n", status);     // Imprime status
}

// ——— FUNCIONES AUXILIARES ———

uint8_t MAX31328RTC::decToBcd(uint8_t val) { // Convierte decimal a BCD 
    return ((val / 10) << 4) + (val % 10); // Decenas en nibble alto, unidades en nibble bajo
}

uint8_t MAX31328RTC::bcdToDec(uint8_t val) { // Convierte BCD a decimal
    return ((val >> 4) * 10) + (val & 0x0F); // Combina nibble alto y bajo en valor decimal
}

bool MAX31328RTC::writeRegister(uint8_t reg, uint8_t value) { // Escribe un único registro por I2C
    Wire.beginTransmission(i2c_address); // Inicia transmisión I2C
    Wire.write(reg); // Apunta al registro destino
    Wire.write(value); // Envía el valor
    uint8_t error = Wire.endTransmission(); // Finaliza transmisión y captura código de error
    
    if (error != 0) { // Si hubo error...
        Serial.printf("MAX31328: Error escribiendo reg 0x%02X: %d\n", reg, error); // Imprime detalle
        return false; // Retorna false en fallo
    }
    
    return true; // Retorna true si escribe sin errores
}

uint8_t MAX31328RTC::readRegister(uint8_t reg) { // Lee un único registro por I2C
    Wire.beginTransmission(i2c_address); // Inicia transmisión para seleccionar registro
    Wire.write(reg); // Indica registro a leer
    uint8_t error = Wire.endTransmission(); // Finaliza selección y obtiene error
    
    if (error != 0) { // Si hay error...
        Serial.printf("MAX31328: Error en transmisión reg 0x%02X: %d\n", reg, error); // Mensaje de error
        return 0xFF; // Retorna 0xFF como indicador de fallo
    }
    
    Wire.requestFrom(i2c_address, (uint8_t)1); // Solicita 1 byte
    if (Wire.available()) { // Si hay datos disponibles...
        return Wire.read(); // Retorna el byte leído
    }
    
    Serial.printf("MAX31328: Sin datos disponibles reg 0x%02X\n", reg); // Mensaje si no hay datos
    return 0xFF; // Retorna 0xFF indicando fallo
}

bool MAX31328RTC::writeMultipleRegisters(uint8_t startReg, uint8_t* buffer, uint8_t length) { // Escribe múltiples registros
    Wire.beginTransmission(i2c_address); // Inicia transmisión I2C
    Wire.write(startReg); // Escribe registro inicial
    
    for (uint8_t i = 0; i < length; i++) { // Escribe cada byte del buffer
        Wire.write(buffer[i]); // Envía byte
    }
    
    uint8_t error = Wire.endTransmission(); // Finaliza y obtiene código de error
    
    if (error != 0) { // En caso de error...
        Serial.printf("MAX31328: Error escribiendo múltiples registros desde 0x%02X: %d\n", 
                      startReg, error); // Imprime detalle
        return false; // Retorna false
    }
    
    return true; // Retorna true si OK
}

bool MAX31328RTC::readMultipleRegisters(uint8_t startReg, uint8_t* buffer, uint8_t length) { // Lee múltiples registros
    Wire.beginTransmission(i2c_address); // Inicia transmisión para indicar registro inicial
    Wire.write(startReg); // Escribe registro inicial
    uint8_t error = Wire.endTransmission(); // Finaliza selección y captura error
    
    if (error != 0) { // Si hubo error, entonces...
        Serial.printf("MAX31328: Error en transmisión múltiple reg 0x%02X: %d\n", 
                      startReg, error); // Mensaje de error
        return false; // Retorna false
    }
    
    Wire.requestFrom(i2c_address, length); // Solicita 'length' bytes
    
    for (uint8_t i = 0; i < length; i++) { // Itera para leer cada byte
        if (Wire.available()) { // Si hay datos...
            buffer[i] = Wire.read(); // Almacena en buffer
        } else { // Si no hay suficientes datos...
            Serial.printf("MAX31328: Datos insuficientes en lectura múltiple\n"); // Mensaje
            return false; // Retorna false
        }
    }
    
    return true; // Retorna true si todo fue leído correctamente
}