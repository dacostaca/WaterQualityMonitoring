#include "TDS.h"

// ——— Variables internas del módulo ———
namespace TDSSensor {
    
    // Variables del sensor
    bool initialized = false;
    uint8_t sensor_pin = TDS_SENSOR_PIN;
    uint32_t last_reading_time = 0;
    TDSReading last_reading = {0};
    esp_adc_cal_characteristics_t adc_chars;
    
    // Variables de calibración 
    float kValue = TDS_CALIBRATED_KVALUE;       //factor de calibración de la celda 
    float voltageOffset = TDS_CALIBRATED_VOFFSET; //ajuste de voltaje para compensar errores del sensor o ADC
    
    // Constantes del sensor 
    const float TDS_FACTOR = 0.5f;         // TDS = EC / 2
    const float TEMP_COEFFICIENT = 0.02f;  // 2% por °C
    const float COEFF_A3 = 133.42f;        // Coeficiente cúbico
    const float COEFF_A2 = -255.86f;       // Coeficiente cuadrático
    const float COEFF_A1 = 857.39f;        // Coeficiente lineal
    
    // Configuración ADC
    const int ADC_BITS = 12;
    const int ADC_MAX_VALUE = 4095;
    const int ADC_VREF = 1100;             // mV
    
    // Variables de integración con sistema principal
    uint16_t* total_readings_counter = nullptr;
    void (*error_logger)(int code, int severity, uint32_t context) = nullptr;
    
    // ——— FUNCIONES INTERNAS ———
    
    float readCalibratedVoltage() {
        long sum = 0;
        int validSamples = 0;
        
        for (int i = 0; i < SAMPLES; i++) {
            int rawValue = analogRead(sensor_pin);  
            if (rawValue >= 0 && rawValue <= ADC_MAX_VALUE) {
                sum += rawValue;
                validSamples++;
            }
            delayMicroseconds(1000); // 1ms entre muestras
        }
        
        if (validSamples == 0) return 0.0f;
        
        float avgRaw = (float)sum / validSamples;
        uint32_t voltage_mv = esp_adc_cal_raw_to_voltage((uint32_t)avgRaw, &adc_chars);
        
        // Aplicar offset calibrado directamente
        float voltage_v = (voltage_mv / 1000.0f) - voltageOffset;
        
        return voltage_v;
        //toma n muestras, descarta valores fuera de rango y promedia los datos obtenidos de datos crudos 
        //convierte a voltaje en mv y luego a voltaje en V y resta el offset
    }
    
    float compensateTemperature(float voltage, float temperature) {
        // Compensación de temperatura 
        float compensationFactor = 1.0f + TEMP_COEFFICIENT * (temperature - 25.0f);
        return voltage / compensationFactor;
        //ajusta el voltaje respecto a la temperatura 
    }
    
    float calculateECRaw(float compensatedVoltage) {
        // Ecuación polinómica de la librería GravityTDS
        return COEFF_A3 * compensatedVoltage * compensatedVoltage * compensatedVoltage +
               COEFF_A2 * compensatedVoltage * compensatedVoltage +
               COEFF_A1 * compensatedVoltage;
               //convierte el voltaje compensado a una CONDUCTIVIDAD ELÉCTRICA
               //polinomio cúbico de calibración de la librería original Gravity TDS
    }
    
    float calculateEC(float compensatedVoltage) {
        // multiplica por el factor de calibración del electrodo kValue
        return calculateECRaw(compensatedVoltage) * kValue;
    }
    
    float calculateTDS(float ec) {
        // convierte conductividad a TDS usando factor TDS (definido en 0.5)
        return ec * TDS_FACTOR;
    }
    
    // ——— IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS ———
    
    bool initialize(uint8_t pin) {
        if (initialized) {
            //Serial.println(" Sensor TDS ya inicializado");
            return true;
        }
        
        //Serial.printf(" Inicializando sensor TDS (pin %d)...\n", pin);
        
        sensor_pin = pin;
        
        // Configurar ADC con calibración ESP32
        //resolución de 12bits
        //atenuación de 6db para medir hasta 2.2V (el sensor puede entregar hasta 2.0V)
        analogReadResolution(ADC_BITS);
        analogSetPinAttenuation(sensor_pin, ADC_6db); 
        
        // Calibrar ADC específico para ESP32
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
            ADC_UNIT_1,
            ADC_ATTEN_DB_6,
            ADC_WIDTH_BIT_13,
            ADC_VREF,
            &adc_chars
        );
        
        initialized = true;
        last_reading_time = millis();
        
        //Serial.println(" Sensor TDS inicializado correctamente");
        //Serial.printf("   kValue calibrado: %.6f\n", kValue);
        //Serial.printf("   Offset calibrado: %.6fV\n", voltageOffset);
        
        return true;
    }
    
    
    TDSReading takeReading(float temperature) {
        return takeReadingWithTimeout(temperature);
    }
    
    TDSReading takeReadingWithTimeout(float temperature) {
        //funcion principal para toma de datos
        TDSReading reading = {0};
        
        if (!initialized) {
            Serial.println(" Sensor TDS no inicializado");
            reading.valid = false;
            reading.sensor_status = TDS_STATUS_INVALID_READING;
            return reading;
        }
        
        // Incrementar contador
        if (total_readings_counter) {
            (*total_readings_counter)++;
            reading.reading_number = *total_readings_counter;
        }
        
        reading.timestamp = millis();
        reading.temperature = temperature;
        
        // Timeout para operación del sensor
        uint32_t start_time = millis();
        
        // Leer voltaje calibrado (ya con offset aplicado)
        float voltage = readCalibratedVoltage();
        
        // Verificar timeout
        if (millis() - start_time > TDS_OPERATION_TIMEOUT) {
            Serial.println(" Timeout en lectura de sensor TDS");
            
            if (error_logger) {
                error_logger(1, 1, millis() - start_time); // ERROR_SENSOR_TIMEOUT, SEVERITY_WARNING
            }
            
            reading.valid = false;
            reading.sensor_status = TDS_STATUS_TIMEOUT;
            
            if (total_readings_counter) {
                (*total_readings_counter)--;
            }
            
            last_reading = reading;
            return reading;
        }
        
        // Validar voltaje
        if (!isVoltageInRange(voltage)) {
            if (voltage < MIN_VALID_VOLTAGE) {
                reading.sensor_status = TDS_STATUS_VOLTAGE_LOW;
                Serial.printf(" Voltaje TDS muy bajo: %.3fV\n", voltage);
            } else {
                reading.sensor_status = TDS_STATUS_VOLTAGE_HIGH;
                Serial.printf(" Voltaje TDS muy alto: %.3fV\n", voltage);
            }
            
            reading.valid = false;
            reading.tds_value = 0.0;
            reading.ec_value = 0.0;
            
            if (error_logger) {
                error_logger(2, 1, (uint32_t)(voltage * 1000)); // ERROR_SENSOR_INVALID_READING
            }
            
            if (total_readings_counter) {
                (*total_readings_counter)--;
            }
            
            last_reading = reading;
            return reading;
        }
        
        // Compensar temperatura
        float compensatedVoltage = compensateTemperature(voltage, temperature);
        
        // Calcular EC y TDS usando valores calibrados
        float ec = calculateEC(compensatedVoltage);
        float tds = calculateTDS(ec);
        
        // Validar resultados
        if (isTDSInRange(tds) && isECInRange(ec)) {
            reading.tds_value = tds;
            reading.ec_value = ec;
            reading.valid = true;
            reading.sensor_status = TDS_STATUS_OK;
            
            last_reading_time = millis();
            
            Serial.printf(" TDS: %.1f ppm | EC: %.1f µS/cm | V: %.3fV | T: %.1f°C (%.0f ms)\n", 
                         tds, ec, voltage + voltageOffset, temperature, millis() - start_time);
        } else {
            reading.tds_value = 0.0;
            reading.ec_value = 0.0;
            reading.valid = false;
            reading.sensor_status = TDS_STATUS_INVALID_READING;
            
            if (error_logger) {
                error_logger(2, 1, (uint32_t)tds); // ERROR_SENSOR_INVALID_READING
            }
            
            if (total_readings_counter) {
                (*total_readings_counter)--;
            }
            
            Serial.printf(" Lectura TDS inválida: %.1f ppm (EC: %.1f µS/cm)\n", tds, ec);
        }
        
        last_reading = reading;
        return reading;
    }
    
    void debugVoltageReading() {
    if (!initialized) return;
    
    Serial.println(" === DEBUG VOLTAJE TDS ===");
    
    // Leer voltaje crudo (SIN offset)
    long sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sum += analogRead(sensor_pin);
        delayMicroseconds(1000);
    }
    
    float avgRaw = (float)sum / SAMPLES;
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage((uint32_t)avgRaw, &adc_chars);
    float voltajeCrudo = voltage_mv / 1000.0f;
    
    Serial.printf("Voltaje crudo (sin offset): %.6fV\n", voltajeCrudo);
    Serial.printf("Offset actual: %.6fV\n", voltageOffset);
    Serial.printf("Voltaje final: %.6fV\n", voltajeCrudo - voltageOffset);
    
    if (voltajeCrudo - voltageOffset < 0) {
        Serial.println(" PROBLEMA: Offset demasiado alto!");
        float offsetSugerido = voltajeCrudo * 0.8f;  
        Serial.printf("   Offset sugerido: %.6fV\n", offsetSugerido);
    }
    
    Serial.println("==============================");
}
    // ——— FUNCIONES DE CALIBRACIÓN  ———
    
    void setCalibration(float kVal, float vOffset) {
        kValue = kVal;
        voltageOffset = vOffset;
        Serial.printf(" Calibración TDS actualizada: k=%.6f, offset=%.6fV\n", kValue, voltageOffset);
    }
    
    void getCalibration(float& kVal, float& vOffset) {
        kVal = kValue;
        vOffset = voltageOffset;

        //Serial.printf(" getCalibration() - k=%.6f, offset=%.6fV\n", kVal, vOffset);
    }
    
    void resetToDefaultCalibration() {
        kValue = TDS_CALIBRATED_KVALUE;
        voltageOffset = TDS_CALIBRATED_VOFFSET;
        Serial.printf(" Calibración restaurada a valores por defecto: k=%.6f, offset=%.6fV\n", 
                     kValue, voltageOffset);
    }
    
    // ——— FUNCIONES DE ESTADO ———
    
    bool isInitialized() { 
        return initialized; 
    }
    
    bool isLastReadingValid() { 
        return last_reading.valid; 
    }
    
    float getLastTDS() { 
        return last_reading.tds_value; 
    }
    
    float getLastEC() { 
        return last_reading.ec_value; 
    }
    
    uint32_t getLastReadingTime() { 
        return last_reading_time; 
    }
    
    uint16_t getTotalReadings() {
        return total_readings_counter ? *total_readings_counter : 0;
    }
    
    // ——— FUNCIONES DE UTILIDAD ———
    
    void printLastReading() {
        if (last_reading.reading_number == 0) {
            Serial.println("📊 No hay lecturas TDS previas");
            return;
        }
        
        Serial.println("📊 --- ÚLTIMA LECTURA TDS ---");
        Serial.printf("Lectura #%d\n", last_reading.reading_number);
        Serial.printf("TDS: %.1f ppm\n", last_reading.tds_value);
        Serial.printf("EC: %.1f µS/cm\n", last_reading.ec_value);
        Serial.printf("Temperatura: %.1f °C\n", last_reading.temperature);
        Serial.printf("Timestamp: %u ms\n", last_reading.timestamp);
        Serial.printf("Estado: 0x%02X (%s)\n", 
                     last_reading.sensor_status,
                     last_reading.valid ? "VÁLIDA" : "INVÁLIDA");
        Serial.println("---------------------------");
    }
    
    bool isTDSInRange(float tds) {
        return (tds >= MIN_VALID_TDS && tds <= MAX_VALID_TDS && !isnan(tds));
    }
    
    bool isECInRange(float ec) {
        return (ec >= MIN_VALID_EC && ec <= MAX_VALID_EC && !isnan(ec));
    }
    
    bool isVoltageInRange(float voltage) {
        return (voltage >= MIN_VALID_VOLTAGE && voltage <= MAX_VALID_VOLTAGE && !isnan(voltage));
    }
    
    String getWaterQuality(float tds) {
        if (tds < 50) return "Muy pura";
        else if (tds < 150) return "Excelente";
        else if (tds < 300) return "Buena";
        else if (tds < 500) return "Aceptable";
        else if (tds < 900) return "Pobre";
        else return "Muy pobre";
    }
    
    // ——— FUNCIONES DE INTEGRACIÓN ———
    
    void setReadingCounter(uint16_t* total_readings_ptr) {
        total_readings_counter = total_readings_ptr;
    }
    
    void setErrorLogger(void (*log_error_func)(int, int, uint32_t)) {
        error_logger = log_error_func;
    }
    
    // ——— FUNCIONES ADICIONALES ———
    
    void showCalibrationInfo() {
        Serial.println(" === INFORMACIÓN DE CALIBRACIÓN TDS ===");
        Serial.printf("Estado: %s\n", initialized ? "Inicializado" : "No inicializado");
        Serial.printf("Pin ADC: %d\n", sensor_pin);
        Serial.printf("kValue: %.6f (valor calibrado fijo)\n", kValue);
        Serial.printf("Offset voltaje: %.6fV (valor calibrado fijo)\n", voltageOffset);
        Serial.printf("TDS Factor: %.1f (EC/%.0f)\n", TDS_FACTOR, 1.0f/TDS_FACTOR);
        Serial.printf("Coeficientes: A3=%.2f, A2=%.2f, A1=%.2f\n", COEFF_A3, COEFF_A2, COEFF_A1);
        
        if (last_reading.valid) {
            Serial.printf("Última lectura: %.1f ppm (%.1f µS/cm) - %s\n", 
                         last_reading.tds_value, last_reading.ec_value,
                         getWaterQuality(last_reading.tds_value).c_str());
        } else {
            Serial.println("Sin lecturas válidas recientes");
        }
        Serial.println("=========================================");
    }
    
    void testReading() {
        if (!initialized) {
            Serial.println(" Sensor no inicializado");
            return;
        }
        
        Serial.println(" === TEST LECTURA TDS ===");
        
        float voltage = readCalibratedVoltage();
        Serial.printf("Voltaje calibrado: %.6fV\n", voltage);
        Serial.printf("Voltaje crudo estimado: %.6fV\n", voltage + voltageOffset);
        
        if (isVoltageInRange(voltage)) {
            float compensated = compensateTemperature(voltage, 25.0f);
            float ec = calculateEC(compensated);
            float tds = calculateTDS(ec);
            
            Serial.printf("Voltaje compensado: %.6fV\n", compensated);
            Serial.printf("EC calculado: %.1f µS/cm\n", ec);
            Serial.printf("TDS calculado: %.1f ppm\n", tds);
            Serial.printf("Calidad: %s\n", getWaterQuality(tds).c_str());
        } else {
            Serial.printf(" Voltaje fuera de rango válido (%.3f-%.3fV)\n", 
                         MIN_VALID_VOLTAGE, MAX_VALID_VOLTAGE);
        }
        
        Serial.println("========================");
    }
    
} // namespace TDSSensor




