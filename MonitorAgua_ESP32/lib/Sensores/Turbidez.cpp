#include "Turbidez.h"

// ——— Variables internas del módulo ———
namespace TurbiditySensor {
    
    // Variables del sensor
    bool initialized = false;
    uint8_t sensor_pin = TURBIDITY_SENSOR_PIN;
    uint32_t last_reading_time = 0;
    TurbidityReading last_reading = {0};
    esp_adc_cal_characteristics_t adc_chars;
    
    // Coeficientes de calibración 
    float calib_a = CALIB_COEFF_A;
    float calib_b = CALIB_COEFF_B;
    float calib_c = CALIB_COEFF_C;
    float calib_d = CALIB_COEFF_D;
    
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
        
        // Convertir a voltios
        float voltage_v = voltage_mv / 1000.0f;
        
        return voltage_v;
    }
    
    // ——— IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS ———
    
    bool initialize(uint8_t pin) {
        if (initialized) {
            //Serial.println(" Sensor turbidez ya inicializado");
            return true;
        }
        
        //Serial.printf(" Inicializando sensor turbidez (pin %d)...\n", pin);
        
        sensor_pin = pin;
        
        // Configurar ADC con calibración ESP32
        analogReadResolution(ADC_BITS);
        analogSetPinAttenuation(sensor_pin, ADC_11db); 
        
        // Calibrar ADC específico para ESP32
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
            ADC_UNIT_1,
            ADC_ATTEN_DB_12,
            ADC_WIDTH_BIT_13,
            ADC_VREF,
            &adc_chars
        );
        
        initialized = true;
        last_reading_time = millis();
        
        //Serial.println(" Sensor turbidez inicializado correctamente");
        return true;
    }
    
    void cleanup() {
        initialized = false;
        //Serial.println(" Sensor turbidez limpiado");
    }
    
    TurbidityReading takeReading() {
        return takeReadingWithTimeout();
    }
    
    TurbidityReading takeReadingWithTimeout() {
        TurbidityReading reading = {0};
        
        if (!initialized) {
            Serial.println(" Sensor turbidez no inicializado");
            reading.valid = false;
            reading.sensor_status = TURBIDITY_STATUS_INVALID_READING;
            return reading;
        }
        
        // Incrementar contador
        if (total_readings_counter) {
            (*total_readings_counter)++;
            reading.reading_number = *total_readings_counter;
        }
        
        reading.timestamp = millis();
        
        // Timeout para operación del sensor
        uint32_t start_time = millis();
        
        // Leer voltaje calibrado
        float voltage = readCalibratedVoltage();
        
        // Verificar timeout
        if (millis() - start_time > TURBIDITY_OPERATION_TIMEOUT) {
            Serial.println(" Timeout en lectura de sensor turbidez");
            
            if (error_logger) {
                error_logger(1, 1, millis() - start_time); // ERROR_SENSOR_TIMEOUT, SEVERITY_WARNING
            }
            
            reading.valid = false;
            reading.sensor_status = TURBIDITY_STATUS_TIMEOUT;
            
            if (total_readings_counter) {
                (*total_readings_counter)--;
            }
            
            last_reading = reading;
            return reading;
        }
        
        // Validar voltaje
        if (!isVoltageInRange(voltage)) {
            if (voltage < MIN_VALID_VOLTAGE) {
                reading.sensor_status = TURBIDITY_STATUS_VOLTAGE_LOW;
                Serial.printf(" Voltaje turbidez muy bajo: %.3fV\n", voltage);
            } else {
                reading.sensor_status = TURBIDITY_STATUS_VOLTAGE_HIGH;
                Serial.printf(" Voltaje turbidez muy alto: %.3fV\n", voltage);
            }
            
            reading.valid = false;
            reading.turbidity_ntu = 0.0;
            reading.voltage = voltage;
            
            if (error_logger) {
                error_logger(2, 1, (uint32_t)(voltage * 1000)); // ERROR_SENSOR_INVALID_READING
            }
            
            if (total_readings_counter) {
                (*total_readings_counter)--;
            }
            
            last_reading = reading;
            return reading;
        }
        
        // Calcular turbidez usando calibración
        float ntu = voltageToNTU(voltage);
        
        // Validar resultados
        if (isTurbidityInRange(ntu)) {
            reading.turbidity_ntu = ntu;
            reading.voltage = voltage;
            reading.valid = true;
            reading.sensor_status = TURBIDITY_STATUS_OK;
            
            last_reading_time = millis();
            
            Serial.printf(" Turbidez: %.1f NTU | V: %.3fV | %s (%.0f ms)\n", 
                         ntu, voltage, getWaterQuality(ntu).c_str(), millis() - start_time);
        } else {
            reading.turbidity_ntu = 0.0;
            reading.voltage = voltage;
            reading.valid = false;
            
            if (ntu > MAX_VALID_NTU) {
                reading.sensor_status = TURBIDITY_STATUS_OVERFLOW;
                Serial.printf(" Turbidez fuera de rango: %.1f NTU (máximo: %.0f)\n", ntu, MAX_VALID_NTU);
            } else {
                reading.sensor_status = TURBIDITY_STATUS_INVALID_READING;
                Serial.printf(" Lectura turbidez inválida: %.1f NTU\n", ntu);
            }
            
            if (error_logger) {
                error_logger(2, 1, (uint32_t)ntu); // ERROR_SENSOR_INVALID_READING
            }
            
            if (total_readings_counter) {
                (*total_readings_counter)--;
            }
        }
        
        last_reading = reading;
        return reading;
    }
    
    // ——— FUNCIONES DE CALIBRACIÓN ———
    
    float voltageToNTU(float voltage) {
        
        // Aplicar ecuación polinómica cúbica calibrada
        // NTU = a*V³ + b*V² + c*V + d
        float v = voltage;
        float ntu = calib_a * v * v * v + 
                   calib_b * v * v + 
                   calib_c * v + 
                   calib_d;
        
        // Asegurar que NTU no sea negativo
        if (ntu < 0) ntu = 0;
        
        if (voltage > 2.15f) {
            ntu = 3000.0f * (2.2f - voltage) / (2.2f - 0.65f);
            if (ntu < 0) ntu = 0;
            if (ntu > 10) ntu = 10; 
        }
        else if (voltage < 0.7f) {
            ntu = 1000.0f + (0.7f - voltage) * 2000.0f;
            if (ntu > 3000) ntu = 3000; 
        }
        else {
            ntu = 1500.0f * (2.18f - voltage) / (2.18f - 0.65f);
            if (ntu < 0) ntu = 0;
        }
        
        return ntu;
    }
    
    float calibrateReading(float rawVoltage) {
        return voltageToNTU(rawVoltage);
    }
    
    void setCalibrationCoefficients(float a, float b, float c, float d) {
        calib_a = a;
        calib_b = b;
        calib_c = c;
        calib_d = d;
        Serial.printf(" Calibración turbidez actualizada: a=%.1f, b=%.1f, c=%.1f, d=%.1f\n", 
                     calib_a, calib_b, calib_c, calib_d);
    }
    
    void getCalibrationCoefficients(float& a, float& b, float& c, float& d) {
        a = calib_a;
        b = calib_b; 
        c = calib_c;
        d = calib_d;
    }
    
    void resetToDefaultCalibration() {
        calib_a = CALIB_COEFF_A;
        calib_b = CALIB_COEFF_B;
        calib_c = CALIB_COEFF_C;
        calib_d = CALIB_COEFF_D;
        Serial.printf(" Calibración turbidez restaurada a valores por defecto\n");
    }
    
    // ——— FUNCIONES DE ESTADO ———
    
    bool isInitialized() { 
        return initialized; 
    }
    
    bool isLastReadingValid() { 
        return last_reading.valid; 
    }
    
    float getLastTurbidity() { 
        return last_reading.turbidity_ntu; 
    }
    
    float getLastVoltage() {
        return last_reading.voltage;
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
            Serial.println(" No hay lecturas turbidez previas");
            return;
        }
        
        Serial.println(" --- ÚLTIMA LECTURA TURBIDEZ ---");
        Serial.printf("Lectura #%d\n", last_reading.reading_number);
        Serial.printf("Turbidez: %.1f NTU\n", last_reading.turbidity_ntu);
        Serial.printf("Voltaje: %.3fV\n", last_reading.voltage);
        Serial.printf("Timestamp: %u ms\n", last_reading.timestamp);
        Serial.printf("Estado: 0x%02X (%s)\n", 
                     last_reading.sensor_status,
                     last_reading.valid ? "VÁLIDA" : "INVÁLIDA");
        Serial.printf("Calidad: %s\n", getWaterQuality(last_reading.turbidity_ntu).c_str());
        Serial.printf("Categoría: %s\n", getTurbidityCategory(last_reading.turbidity_ntu).c_str());
        Serial.println("---------------------------");
    }
    
    bool isTurbidityInRange(float ntu) {
        return (ntu >= MIN_VALID_NTU && ntu <= MAX_VALID_NTU && !isnan(ntu));
    }
    
    bool isVoltageInRange(float voltage) {
        return (voltage >= MIN_VALID_VOLTAGE && voltage <= MAX_VALID_VOLTAGE && !isnan(voltage));
    }
    
    String getWaterQuality(float ntu) {
        if (ntu <= 1) return "Excelente";
        else if (ntu <= 4) return "Muy buena";
        else if (ntu <= 10) return "Buena";
        else if (ntu <= 25) return "Aceptable";
        else if (ntu <= 100) return "Pobre";
        else return "Muy pobre";
    }
    
    String getTurbidityCategory(float ntu) {
        if (ntu <= 1) return "Agua muy clara";
        else if (ntu <= 4) return "Agua clara";
        else if (ntu <= 10) return "Ligeramente turbia";
        else if (ntu <= 25) return "Moderadamente turbia";
        else if (ntu <= 100) return "Turbia";
        else if (ntu <= 400) return "Muy turbia";
        else return "Extremadamente turbia";
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
        Serial.println(" === INFORMACIÓN DE CALIBRACIÓN TURBIDEZ ===");
        Serial.printf("Estado: %s\n", initialized ? "Inicializado" : "No inicializado");
        Serial.printf("Pin ADC: %d\n", sensor_pin);
        Serial.printf("Ecuación: NTU = %.1f*V³ + %.1f*V² + %.1f*V + %.1f\n", 
                     calib_a, calib_b, calib_c, calib_d);
        Serial.printf("Rango válido: %.0f - %.0f NTU\n", MIN_VALID_NTU, MAX_VALID_NTU);
        Serial.printf("Voltaje válido: %.1f - %.1fV\n", MIN_VALID_VOLTAGE, MAX_VALID_VOLTAGE);
        
        if (last_reading.valid) {
            Serial.printf("Última lectura: %.1f NTU (%.3fV) - %s\n", 
                         last_reading.turbidity_ntu, last_reading.voltage,
                         getWaterQuality(last_reading.turbidity_ntu).c_str());
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
        
        Serial.println(" === TEST LECTURA TURBIDEZ ===");
        
        float voltage = readCalibratedVoltage();
        Serial.printf("Voltaje medido: %.6fV\n", voltage);
        
        if (isVoltageInRange(voltage)) {
            float ntu = voltageToNTU(voltage);
            
            Serial.printf("Turbidez calculada: %.1f NTU\n", ntu);
            Serial.printf("Calidad del agua: %s\n", getWaterQuality(ntu).c_str());
            Serial.printf("Categoría: %s\n", getTurbidityCategory(ntu).c_str());
        } else {
            Serial.printf(" Voltaje fuera de rango válido (%.1f-%.1fV)\n", 
                         MIN_VALID_VOLTAGE, MAX_VALID_VOLTAGE);
        }
        
        Serial.println("========================");
    }
    
    void debugVoltageReading() {
        if (!initialized) return;
        
        Serial.println("🔬 === DEBUG VOLTAJE TURBIDEZ ===");
        
        // Leer voltaje crudo
        long sum = 0;
        for (int i = 0; i < SAMPLES; i++) {
            sum += analogRead(sensor_pin);
            delayMicroseconds(1000);
        }
        
        float avgRaw = (float)sum / SAMPLES;
        uint32_t voltage_mv = esp_adc_cal_raw_to_voltage((uint32_t)avgRaw, &adc_chars);
        float voltage = voltage_mv / 1000.0f;
        
        Serial.printf("Valor ADC promedio: %.1f\n", avgRaw);
        Serial.printf("Voltaje calculado: %.6fV\n", voltage);
        Serial.printf("Turbidez estimada: %.1f NTU\n", voltageToNTU(voltage));
        
        Serial.println("==============================");
    }
    
    void printCalibrationCurve() {
        Serial.println(" === CURVA DE CALIBRACIÓN TURBIDEZ CORREGIDA ===");
        Serial.println("Voltaje (V) | Turbidez (NTU) | Calidad");
        Serial.println("------------|---------------|----------");
        
        for (float v = 0.6; v <= 2.2; v += 0.1) {
            float ntu = voltageToNTU(v);
            Serial.printf("   %.2fV    |    %.1f NTU    | %s\n", 
                         v, ntu, getWaterQuality(ntu).c_str());
        }
    }
    
} // namespace TurbiditySensor