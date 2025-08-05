#include "pH.h"

// ——— Variables internas del módulo ———
namespace pHSensor {
    
    // Variables del sensor
    bool initialized = false;
    uint8_t sensor_pin = PH_SENSOR_PIN;
    uint32_t last_reading_time = 0;
    pHReading last_reading = {0};
    esp_adc_cal_characteristics_t adc_chars;
    
    // Variables de calibración
    float phOffset = PH_CALIBRATED_OFFSET;
    float phSlope = PH_CALIBRATED_SLOPE;
    
    // Buffer de muestras para promediado
    int phArray[PH_ARRAY_LENGTH];
    int phArrayIndex = 0;
    
    // Configuración ADC
    const int ADC_BITS = 12;
    const int ADC_MAX_VALUE = 4095;
    const int ADC_VREF = 1100;  // mV
    
    // Variables de integración con sistema principal
    uint16_t* total_readings_counter = nullptr;
    void (*error_logger)(int code, int severity, uint32_t context) = nullptr;
    
    // ——— FUNCIONES INTERNAS ———
    
    double averageArray(int* arr, int number) {
        if (number <= 0) return 0;
        
        long sum = 0;
        
        if (number < 5) {
            // Si hay pocas muestras, promediar todas
            for (int i = 0; i < number; i++) {
                sum += arr[i];
            }
            return (double)sum / number;
        } else {
            // Descartar máximo y mínimo
            int minv = arr[0];
            int maxv = arr[0];
            
            // Encontrar máximo y mínimo
            for (int i = 1; i < number; i++) {
                if (arr[i] < minv) minv = arr[i];
                if (arr[i] > maxv) maxv = arr[i];
            }
            
            // Sumar todos excepto máximo y mínimo
            int count = 0;
            for (int i = 0; i < number; i++) {
                if (arr[i] != minv && arr[i] != maxv) {
                    sum += arr[i];
                    count++;
                }
            }
            
            return count > 0 ? (double)sum / count : 0;
        }
    }
    
    float readAveragedVoltage() {
        // Tomar múltiples muestras con el intervalo configurado
        unsigned long startTime = millis();
        int sampleCount = 0;
        
        // Llenar el array de muestras
        while (sampleCount < PH_ARRAY_LENGTH && (millis() - startTime) < 1000) {
            phArray[sampleCount] = analogRead(sensor_pin);
            sampleCount++;
            delay(PH_SAMPLING_INTERVAL);
        }
        
        // Calcular promedio descartando extremos
        double avgRaw = averageArray(phArray, sampleCount);
        
        // Convertir a voltaje usando calibración ESP32
        uint32_t voltage_mv = esp_adc_cal_raw_to_voltage((uint32_t)avgRaw, &adc_chars);
        float voltage_v = voltage_mv / 1000.0f;
        
        return voltage_v;
    }
    
    // ——— IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS ———
    
    bool initialize(uint8_t pin) {
        if (initialized) {
            //Serial.println(" Sensor pH ya inicializado");
            return true;
        }
        
        //Serial.printf(" Inicializando sensor pH (pin %d)...\n", pin);
        
        sensor_pin = pin;
        
        // Configurar ADC con calibración ESP32
        analogReadResolution(ADC_BITS);
        analogSetPinAttenuation(sensor_pin, ADC_11db); // Para voltajes hasta 3.3V
        
        // Calibrar ADC específico para ESP32
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
            ADC_UNIT_1,
            ADC_ATTEN_DB_12,
            ADC_WIDTH_BIT_13,
            ADC_VREF,
            &adc_chars
        );
        
        // Limpiar array de muestras
        memset(phArray, 0, sizeof(phArray));
        phArrayIndex = 0;
        
        initialized = true;
        last_reading_time = millis();
        
        //Serial.println(" Sensor pH inicializado correctamente");
        //Serial.printf("   Offset calibrado: %.2f\n", phOffset);
        //Serial.printf("   Pendiente calibrada: %.2f\n", phSlope);

        return true;
    }
    
    void cleanup() {
        initialized = false;
        //Serial.println(" Sensor pH limpiado");
    }
    
    pHReading takeReading (float temperature) {
        return takeReadingWithTimeout (temperature);
    }
    
    pHReading takeReadingWithTimeout(float temperature) {
        pHReading reading = {0};
        
        if (!initialized) {
            //Serial.println(" Sensor pH no inicializado");
            reading.valid = false;
            reading.sensor_status = PH_STATUS_INVALID_READING;
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
        
        // Leer voltaje promediado
        float voltage = readAveragedVoltage();
        
        // Verificar timeout
        if (millis() - start_time > PH_OPERATION_TIMEOUT) {
            Serial.println(" Timeout en lectura de sensor pH");
            
            if (error_logger) {
                error_logger(1, 1, millis() - start_time); // ERROR_SENSOR_TIMEOUT
            }
            
            reading.valid = false;
            reading.sensor_status = PH_STATUS_TIMEOUT;
            
            if (total_readings_counter) {
                (*total_readings_counter)--;
            }
            
            last_reading = reading;
            return reading;
        }
        
        // Validar voltaje
        if (!isVoltageInRange(voltage)) {
            if (voltage < MIN_VALID_VOLTAGE) {
                reading.sensor_status = PH_STATUS_VOLTAGE_LOW;
                //Serial.printf(" Voltaje pH muy bajo: %.3fV\n", voltage);
            } else {
                reading.sensor_status = PH_STATUS_VOLTAGE_HIGH;
                //Serial.printf(" Voltaje pH muy alto: %.3fV\n", voltage);
            }
            
            reading.valid = false;
            reading.ph_value = 0.0;
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
        
        // Calcular pH usando calibración
        float ph = phSlope * voltage + phOffset;
        
        // Validar resultado
        if (isPHInRange(ph)) {
            reading.ph_value = ph;
            reading.voltage = voltage;
            reading.valid = true;
            reading.sensor_status = PH_STATUS_OK;
            
            last_reading_time = millis();
            
            Serial.printf(" pH: %.2f | V: %.3fV | %s (%.0f ms)\n", 
                         ph, voltage, getWaterType(ph).c_str(), millis() - start_time);
        } else {
            reading.ph_value = 0.0;
            reading.voltage = voltage;
            reading.valid = false;
            reading.sensor_status = PH_STATUS_OUT_OF_RANGE;
            
            if (error_logger) {
                error_logger(2, 1, (uint32_t)(ph * 100)); // ERROR_SENSOR_INVALID_READING
            }
            
            if (total_readings_counter) {
                (*total_readings_counter)--;
            }
            
            Serial.printf(" pH fuera de rango: %.2f\n", ph);
        }
        
        last_reading = reading;
        return reading;
    }
    
    // ——— FUNCIONES DE CALIBRACIÓN ———
    
    void setCalibration(float offset, float slope) {
        phOffset = offset;
        phSlope = slope;
        Serial.printf(" Calibración pH actualizada: offset=%.2f, pendiente=%.2f\n", 
                     phOffset, phSlope);
    }
    
    void getCalibration(float& offset, float& slope) {
        offset = phOffset;
        slope = phSlope;
    }
    
    void resetToDefaultCalibration() {
        phOffset = PH_CALIBRATED_OFFSET;
        phSlope = PH_CALIBRATED_SLOPE;
        Serial.printf(" Calibración pH restaurada a valores por defecto\n");
    }
    
    bool calibrateWithBuffer(float bufferPH, float measuredVoltage) {
        // Para calibración simple con un punto (asumiendo pendiente fija)
        // pH = slope * V + offset
        // offset = pH - slope * V
        
        float newOffset = bufferPH - phSlope * measuredVoltage;
        
        Serial.printf(" Calibración con buffer pH %.2f:\n", bufferPH);
        Serial.printf("   Voltaje medido: %.3fV\n", measuredVoltage);
        Serial.printf("   Nuevo offset: %.2f (anterior: %.2f)\n", newOffset, phOffset);
        
        phOffset = newOffset;
        
        return true;
    }
    
    // ——— FUNCIONES DE ESTADO ———
    
    bool isInitialized() { 
        return initialized; 
    }
    
    bool isLastReadingValid() { 
        return last_reading.valid; 
    }
    
    float getLastPH() { 
        return last_reading.ph_value; 
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
            Serial.println(" No hay lecturas pH previas");
            return;
        }
        
        Serial.println(" --- ÚLTIMA LECTURA pH ---");
        Serial.printf("Lectura #%d\n", last_reading.reading_number);
        Serial.printf("pH: %.2f\n", last_reading.ph_value);
        Serial.printf("Voltaje: %.3fV\n", last_reading.voltage);
        Serial.printf("Timestamp: %u ms\n", last_reading.timestamp);
        Serial.printf("Estado: 0x%02X (%s)\n", 
                     last_reading.sensor_status,
                     last_reading.valid ? "VÁLIDA" : "INVÁLIDA");
        Serial.println("---------------------------");
    }
    
    bool isPHInRange(float ph) {
        return (ph >= MIN_VALID_PH && ph <= MAX_VALID_PH && !isnan(ph));
    }
    
    bool isVoltageInRange(float voltage) {
        return (voltage >= MIN_VALID_VOLTAGE && voltage <= MAX_VALID_VOLTAGE && !isnan(voltage));
    }
    
    String getWaterType(float ph) {
        if (ph < 6.0) return "Muy ácida";
        else if (ph < 6.5) return "Ácida";
        else if (ph < 7.0) return "Ligeramente ácida";
        else if (ph == 7.0) return "Neutra";
        else if (ph < 7.5) return "Ligeramente alcalina";
        else if (ph < 8.5) return "Alcalina";
        else return "Muy alcalina";
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
        Serial.println(" === INFORMACIÓN DE CALIBRACIÓN pH ===");
        Serial.printf("Estado: %s\n", initialized ? "Inicializado" : "No inicializado");
        Serial.printf("Pin ADC: %d\n", sensor_pin);
        Serial.printf("Ecuación: pH = %.2f * V + %.2f\n", phSlope, phOffset);
        Serial.printf("Rango válido pH: %.1f - %.1f\n", MIN_VALID_PH, MAX_VALID_PH);
        Serial.printf("Voltaje válido: %.1f - %.1fV\n", MIN_VALID_VOLTAGE, MAX_VALID_VOLTAGE);
        
        if (last_reading.valid) {
            Serial.printf("Última lectura: pH %.2f (%.3fV) - %s\n", 
                         last_reading.ph_value, last_reading.voltage,
                         getWaterType(last_reading.ph_value).c_str());
        } else {
            Serial.println("Sin lecturas válidas recientes");
        }
        Serial.println("=======================================");
    }
    
    void testReading() {
        if (!initialized) {
            Serial.println(" Sensor no inicializado");
            return;
        }
        
        Serial.println(" === TEST LECTURA pH ===");
        
        float voltage = readAveragedVoltage();
        Serial.printf("Voltaje medido: %.6fV\n", voltage);
        
        if (isVoltageInRange(voltage)) {
            float ph = phSlope * voltage + phOffset;
            
            Serial.printf("pH calculado: %.2f\n", ph);
            Serial.printf("Estado: %s\n", isPHInRange(ph) ? "VÁLIDO" : "FUERA DE RANGO");
        } else {
            Serial.printf(" Voltaje fuera de rango válido (%.1f-%.1fV)\n", 
                         MIN_VALID_VOLTAGE, MAX_VALID_VOLTAGE);
        }
        
        Serial.println("========================");
    }
    
    void performCalibrationRoutine() {
        if (!initialized) {
            Serial.println(" Sensor no inicializado");
            return;
        }
        
        Serial.println("\n === RUTINA DE CALIBRACIÓN pH ===");
        Serial.println("Necesitarás soluciones buffer de pH conocido");
        Serial.println("Recomendado: pH 4.0, 7.0 y 10.0");
        Serial.println("\n1. Sumerge el sensor en buffer pH 7.0");
        Serial.println("2. Espera 30 segundos para estabilizar");
        Serial.println("3. Presiona cualquier tecla para continuar...");
        
        // Esperar input del usuario
        while (!Serial.available()) {
            delay(100);
        }
        Serial.read(); // Limpiar buffer
        
        Serial.println("\nLeyendo voltaje en pH 7.0...");
        delay(2000);
        
        float voltage7 = readAveragedVoltage();
        Serial.printf("Voltaje en pH 7.0: %.3fV\n", voltage7);
        
        // Calcular nuevo offset asumiendo la pendiente actual
        float newOffset = 7.0 - phSlope * voltage7;
        
        Serial.printf("\nCalibración completada:");
        Serial.printf("  Offset anterior: %.2f\n", phOffset);
        Serial.printf("  Nuevo offset: %.2f\n", newOffset);
        Serial.printf("  Pendiente: %.2f (sin cambios)\n", phSlope);
        
        phOffset = newOffset;
        
        Serial.println("\n Calibración actualizada");
        Serial.println("=====================================");
    }
    
} // namespace pHSensor