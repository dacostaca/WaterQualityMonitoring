/**
 * @file Turbidez.cpp
 * @brief Implementación del sensor de turbidez para ESP32
 * @details Este archivo contiene la lógica completa para inicialización, lectura,
 *          calibración y validación del sensor analógico de turbidez. Implementa
 *          conversión de voltaje a NTU mediante ecuación polinómica cúbica calibrada
 *          experimentalmente y algoritmos de segmentación por rangos de voltaje.
 * @author Daniel Acosta - Santiago Erazo
 * @date 01/10/2025
 * @version 1.0
 */

 #include "Turbidez.h"

 /**
 * @namespace TurbiditySensor
 * @brief Espacio de nombres para todas las funcionalidades del sensor de turbidez
 * @details Contiene variables globales internas, funciones de lectura, calibración,
 *          conversión voltaje→NTU y utilidades para manejo completo del sensor
 *          analógico de turbidez conectado al ADC del ESP32.
 */
namespace TurbiditySensor {

    // ——— Variables internas del módulo ———
    
    // Variables del sensor

    /**
     * @brief Bandera de estado de inicialización del sensor de turbidez
     * @details Indica si el sensor ha sido correctamente inicializado y está listo
     *          para realizar lecturas. Evita operaciones sobre hardware no configurado.
     */
    bool initialized = false;

    /**
     * @brief Pin GPIO asignado al ADC para lectura del sensor de turbidez
     * @details Debe ser un pin compatible con ADC1 del ESP32 (GPIO32-39).
     *          Por defecto toma el valor de TURBIDITY_SENSOR_PIN definido en Turbidez.h
     */
    uint8_t sensor_pin = TURBIDITY_SENSOR_PIN;

    /**
     * @brief Timestamp de la última lectura válida realizada
     * @details Almacena millis() del momento en que se completó exitosamente una
     *          lectura. Útil para calcular intervalos entre mediciones.
     */
    uint32_t last_reading_time = 0;

    /**
     * @brief Última estructura de lectura capturada por el sensor
     * @details Contiene turbidez NTU, voltaje, timestamp, estado y número de lectura.
     *          Se actualiza en cada llamada a takeReadingWithTimeout().
     */
    TurbidityReading last_reading = {0};

    /**
     * @brief Características de calibración del ADC del ESP32
     * @details Estructura que almacena los parámetros de calibración específicos del
     *          chip para convertir valores crudos ADC a voltajes reales (mV).
     */
    esp_adc_cal_characteristics_t adc_chars;
    
    // Coeficientes de calibración
    
    /**
     * @brief Coeficiente cúbico (a) de la ecuación de calibración
     * @details Término de tercer grado en: NTU = a×V³ + b×V² + c×V + d
     *          Calibrado experimentalmente para el sensor específico usado.
     * @note Inicializado con CALIB_COEFF_A del header.
     */
    float calib_a = CALIB_COEFF_A;

    /**
     * @brief Coeficiente cuadrático (b) de la ecuación de calibración
     * @details Término de segundo grado en: NTU = a×V³ + b×V² + c×V + d
     *          Calibrado experimentalmente para el sensor específico usado.
     * @note Inicializado con CALIB_COEFF_B del header.
     */
    float calib_b = CALIB_COEFF_B;

    /**
     * @brief Coeficiente lineal (c) de la ecuación de calibración
     * @details Término de primer grado en: NTU = a×V³ + b×V² + c×V + d
     *          Calibrado experimentalmente para el sensor específico usado.
     * @note Inicializado con CALIB_COEFF_C del header.
     */
    float calib_c = CALIB_COEFF_C;

    /**
     * @brief Término independiente (d) de la ecuación de calibración
     * @details Término constante en: NTU = a×V³ + b×V² + c×V + d
     *          Calibrado experimentalmente para el sensor específico usado.
     * @note Inicializado con CALIB_COEFF_D del header.
     */
    float calib_d = CALIB_COEFF_D;
    
    // Configuración ADC

    /**
     * @brief Resolución en bits del ADC configurado
     * @details ESP32 soporta 12 bits de resolución (0-4095).
     *          Usado para configurar analogReadResolution().
     */
    const int ADC_BITS = 12;

    /**
     * @brief Valor máximo del ADC según resolución de 12 bits
     * @details 2^12 - 1 = 4095. Representa el valor digital máximo que puede
     *          retornar analogRead() con 12 bits de resolución.
     */
    const int ADC_MAX_VALUE = 4095;

    /**
     * @brief Voltaje de referencia interno del ADC en mV
     * @details Valor típico 1100 mV para ESP32. Usado en calibración con
     *          esp_adc_cal_characterize() para ajustar lecturas.
     */
    const int ADC_VREF = 1100;             // mV
    
    // Variables de integración con sistema principal

    /**
     * @brief Puntero al contador global de lecturas del sistema
     * @details Permite incrementar un contador externo cada vez que se realiza una
     *          lectura válida. nullptr si no se ha vinculado con sistema principal.
     */
    uint16_t* total_readings_counter = nullptr;

    /**
     * @brief Puntero a función de logging de errores del sistema
     * @details Callback para reportar errores (timeout, lectura inválida, etc.) al
     *          sistema principal. nullptr si no está configurado.
     */
    void (*error_logger)(int code, int severity, uint32_t context) = nullptr;
    
    // ——— FUNCIONES INTERNAS ———
    
    /**
     * @brief Lee voltaje calibrado del sensor de turbidez con promediado de muestras
     * @details Proceso:
     *          1. Toma SAMPLES (50) muestras del ADC con delay de 1ms entre c/u
     *          2. Descarta valores fuera de rango (0 - ADC_MAX_VALUE)
     *          3. Promedia muestras válidas
     *          4. Convierte valor crudo a voltaje (mV) usando calibración ESP32
     *          5. Convierte mV a voltios
     * @return Voltaje calibrado en voltios (V).
     * @note Mayor cantidad de muestras (50 vs 30 en TDS) mejora estabilidad en sensores
     *       de turbidez que tienden a tener más ruido por variaciones en el agua.
     * @warning Función bloqueante por ~50ms (SAMPLES × 1ms). No usar en ISR.
     */
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
    
    /**
     * @brief Inicializa el sensor de turbidez en el pin ADC especificado
     * @details Configura el ADC con:
     *          - Resolución: 12 bits (0-4095)
     *          - Atenuación: 11dB (rango 0-3.3V, apropiado para sensores de turbidez)
     *          - Calibración específica del chip ESP32
     *          Es seguro llamar múltiples veces (verifica estado de inicialización).
     * @param pin Pin GPIO compatible con ADC1 del ESP32 (por defecto TURBIDITY_SENSOR_PIN)
     * @return true si inicialización exitosa o ya estaba inicializado
     * @note Requiere llamarse una vez en setup() antes de usar otras funciones.
     * @warning Discrepancia: analogSetPinAttenuation usa ADC_11db pero
     *          esp_adc_cal_characterize usa ADC_ATTEN_DB_12 y ADC_WIDTH_BIT_13.
     *          PENDIENTE: Unificar niveles de atenuación (similar a sensor pH).
     */
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
    
    /**
     * @brief Limpia y deshabilita el sensor de turbidez
     * @details Marca el sensor como no inicializado, permitiendo reinicialización.
     *          No libera recursos de hardware, solo resetea estado lógico interno.
     * @note Útil para reset de sistema o cambio de configuración.
     */
    void cleanup() {
        initialized = false;
        //Serial.println(" Sensor turbidez limpiado");
    }
    
    /**
     * @brief Realiza una lectura completa de turbidez (wrapper de takeReadingWithTimeout)
     * @details Función de conveniencia que llama internamente a takeReadingWithTimeout().
     * @return Estructura TurbidityReading con resultado completo de la medición
     * @see takeReadingWithTimeout() para detalles completos del proceso de lectura
     */
    TurbidityReading takeReading() {
        return takeReadingWithTimeout();
    }
    
    /**
     * @brief Realiza lectura completa de turbidez con control de timeout y validación exhaustiva
     * @details Proceso completo:
     *          1. Verifica inicialización del sensor
     *          2. Incrementa contador global de lecturas
     *          3. Lee voltaje calibrado (50 muestras promediadas)
     *          4. Verifica timeout de operación (< TURBIDITY_OPERATION_TIMEOUT)
     *          5. Valida rango de voltaje (MIN_VALID_VOLTAGE - MAX_VALID_VOLTAGE)
     *          6. Convierte voltaje a NTU usando voltageToNTU() con algoritmo segmentado
     *          7. Valida rango de turbidez (0 - MAX_VALID_NTU)
     *          8. Actualiza last_reading y registra errores si corresponde
     * @return Estructura TurbidityReading con campos:
     *         - turbidity_ntu: Turbidez en NTU (0.0 si inválida)
     *         - voltage: Voltaje medido en voltios
     *         - timestamp: millis() al momento de la lectura
     *         - reading_number: Número secuencial de lectura
     *         - valid: true si lectura válida y dentro de todos los rangos
     *         - sensor_status: Código bit-field de estado (ver TURBIDITY_STATUS_*)
     * @note Si hay timeout o valores fuera de rango, decrementa el contador global.
     * @warning Función bloqueante por ~50ms (tiempo de muestreo del ADC).
     */
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

    /**
     * @brief Convierte voltaje medido a turbidez en NTU usando algoritmo segmentado
     * @details Implementa conversión por segmentos de voltaje para mejor precisión:
     *          - V > 2.15V: Rango de agua muy clara (0-10 NTU)
     *          - V < 0.7V: Rango de agua muy turbia (>1000 NTU)
     *          - 0.7V ≤ V ≤ 2.15V: Rango medio (10-1500 NTU)
     *          Cada segmento usa interpolación lineal ajustada experimentalmente.
     * @param voltage Voltaje medido del sensor en voltios
     * @return Turbidez en NTU (Nephelometric Turbidity Units). Mínimo 0 NTU.
     * @note El algoritmo segmentado mejora linealidad en rangos extremos donde
     *       el polinomio cúbico pierde precisión.
     * @warning Los coeficientes del polinomio cúbico (calib_a/b/c/d) actualmente NO
     *          se usan en esta implementación. Se conservan para compatibilidad futura.
     */
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
        
        // Segmento 1: Agua muy clara (V > 2.15V → 0-10 NTU)
        if (voltage > 2.15f) {
            ntu = 3000.0f * (2.2f - voltage) / (2.2f - 0.65f);
            if (ntu < 0) ntu = 0;
            if (ntu > 10) ntu = 10; 
        }
        // Segmento 2: Agua muy turbia (V < 0.7V → >1000 NTU)
        else if (voltage < 0.7f) {
            ntu = 1000.0f + (0.7f - voltage) * 2000.0f;
            if (ntu > 3000) ntu = 3000; 
        }
        // Segmento 3: Rango medio (0.7V ≤ V ≤ 2.15V → 10-1500 NTU)
        else {
            ntu = 1500.0f * (2.18f - voltage) / (2.18f - 0.65f);
            if (ntu < 0) ntu = 0;
        }
        
        return ntu;
    }
    
    /**
     * @brief Alias de voltageToNTU() para compatibilidad con API antigua
     * @param rawVoltage Voltaje crudo del sensor en voltios
     * @return Turbidez calibrada en NTU
     * @see voltageToNTU() para detalles del algoritmo
     */
    float calibrateReading(float rawVoltage) {
        return voltageToNTU(rawVoltage);
    }
    
    /**
     * @brief Establece nuevos coeficientes del polinomio de calibración
     * @details Actualiza coeficientes de la ecuación: NTU = a×V³ + b×V² + c×V + d
     * @param a Coeficiente cúbico
     * @param b Coeficiente cuadrático
     * @param c Coeficiente lineal
     * @param d Término independiente
     * @note Imprime confirmación de cambios en Serial.
     * @warning NOTA IMPORTANTE: Los coeficientes actualizados NO se usan en voltageToNTU()
     *          que implementa algoritmo segmentado. Conservados para compatibilidad futura.
     */
    void setCalibrationCoefficients(float a, float b, float c, float d) {
        calib_a = a;
        calib_b = b;
        calib_c = c;
        calib_d = d;
        Serial.printf(" Calibración turbidez actualizada: a=%.1f, b=%.1f, c=%.1f, d=%.1f\n", 
                     calib_a, calib_b, calib_c, calib_d);
    }
    
    /**
     * @brief Obtiene los coeficientes de calibración actuales por referencia
     * @param[out] a Referencia donde se almacenará el coeficiente cúbico
     * @param[out] b Referencia donde se almacenará el coeficiente cuadrático
     * @param[out] c Referencia donde se almacenará el coeficiente lineal
     * @param[out] d Referencia donde se almacenará el término independiente
     */
    void getCalibrationCoefficients(float& a, float& b, float& c, float& d) {
        a = calib_a;
        b = calib_b; 
        c = calib_c;
        d = calib_d;
    }
    
    /**
     * @brief Restablece los coeficientes de calibración a valores por defecto del header
     * @details Restaura calib_a/b/c/d a CALIB_COEFF_A/B/C/D.
     * @note Imprime confirmación en Serial.
     */
    void resetToDefaultCalibration() {
        calib_a = CALIB_COEFF_A;
        calib_b = CALIB_COEFF_B;
        calib_c = CALIB_COEFF_C;
        calib_d = CALIB_COEFF_D;
        Serial.printf(" Calibración turbidez restaurada a valores por defecto\n");
    }
    
    // ——— FUNCIONES DE ESTADO ———
    
    /**
     * @brief Consulta si el sensor está inicializado
     * @return true si initialize() fue llamado exitosamente
     */
    bool isInitialized() { 
        return initialized; 
    }
    
    /**
     * @brief Consulta validez de la última lectura almacenada
     * @return true si last_reading.valid es true
     */
    bool isLastReadingValid() { 
        return last_reading.valid; 
    }
    
    /**
     * @brief Obtiene el valor de turbidez de la última lectura
     * @return Turbidez en NTU (0.0 si última lectura fue inválida)
     */
    float getLastTurbidity() { 
        return last_reading.turbidity_ntu; 
    }
    
    /**
     * @brief Obtiene el voltaje de la última lectura
     * @return Voltaje en voltios
     */
    float getLastVoltage() {
        return last_reading.voltage;
    }
    
    /**
     * @brief Obtiene timestamp de la última lectura válida
     * @return millis() del momento de última lectura exitosa
     */
    uint32_t getLastReadingTime() { 
        return last_reading_time; 
    }
    
    /**
     * @brief Obtiene el total de lecturas realizadas desde el contador global
     * @return Número total de lecturas o 0 si contador no está vinculado
     */
    uint16_t getTotalReadings() {
        return total_readings_counter ? *total_readings_counter : 0;
    }
    
    // ——— FUNCIONES DE UTILIDAD ———
    
    /**
     * @brief Imprime por Serial la última lectura almacenada en formato estructurado
     * @details Muestra: número de lectura, turbidez NTU, voltaje, timestamp, estado,
     *          calidad del agua y categoría de turbidez.
     * @note Si no hay lecturas previas (reading_number == 0), informa al usuario.
     */
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
    
    /**
     * @brief Valida si un valor de turbidez está dentro del rango aceptable
     * @param ntu Valor de turbidez a validar en NTU
     * @return true si ntu está entre MIN_VALID_NTU (0.0) y MAX_VALID_NTU (3000.0) y no es NaN
     */
    bool isTurbidityInRange(float ntu) {
        return (ntu >= MIN_VALID_NTU && ntu <= MAX_VALID_NTU && !isnan(ntu));
    }
    
    /**
     * @brief Valida si un voltaje está dentro del rango aceptable del sensor
     * @param voltage Voltaje en voltios a validar
     * @return true si voltaje está entre MIN_VALID_VOLTAGE (0.1V) y MAX_VALID_VOLTAGE (2.5V) y no es NaN
     */
    bool isVoltageInRange(float voltage) {
        return (voltage >= MIN_VALID_VOLTAGE && voltage <= MAX_VALID_VOLTAGE && !isnan(voltage));
    }
    
    /**
     * @brief Clasifica la calidad del agua según su turbidez
     * @param ntu Valor de turbidez a clasificar en NTU
     * @return String descriptivo de la calidad del agua:
     *         - "Excelente" (NTU ≤ 1) - Agua cristalina
     *         - "Muy buena" (1 < NTU ≤ 4) - Agua muy clara
     *         - "Buena" (4 < NTU ≤ 10) - Agua clara
     *         - "Aceptable" (10 < NTU ≤ 25) - Agua ligeramente turbia
     *         - "Pobre" (25 < NTU ≤ 100) - Agua turbia
     *         - "Muy pobre" (NTU > 100) - Agua muy turbia/no potable
     * @note Clasificación según estándares EPA y OMS para agua potable (límite 5 NTU).
     */
    String getWaterQuality(float ntu) {
        if (ntu <= 1) return "Excelente";
        else if (ntu <= 4) return "Muy buena";
        else if (ntu <= 10) return "Buena";
        else if (ntu <= 25) return "Aceptable";
        else if (ntu <= 100) return "Pobre";
        else return "Muy pobre";
    }
    
    /**
     * @brief Clasifica la categoría visual de turbidez del agua
     * @param ntu Valor de turbidez a clasificar en NTU
     * @return String descriptivo de la apariencia visual:
     *         - "Agua muy clara" (NTU ≤ 1)
     *         - "Agua clara" (1 < NTU ≤ 4)
     *         - "Ligeramente turbia" (4 < NTU ≤ 10)
     *         - "Moderadamente turbia" (10 < NTU ≤ 25)
     *         - "Turbia" (25 < NTU ≤ 100)
     *         - "Muy turbia" (100 < NTU ≤ 400)
     *         - "Extremadamente turbia" (NTU > 400)
     * @note Útil para interpretación rápida de resultados y logs legibles.
     */
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
    
    /**
     * @brief Vincula el sensor con un contador global de lecturas del sistema
     * @param total_readings_ptr Puntero a uint16_t que será incrementado en cada lectura válida
     * @note El puntero debe apuntar a memoria válida durante toda la vida útil del sensor.
     * @warning No pasar punteros a variables locales que puedan salir de scope.
     */
    void setReadingCounter(uint16_t* total_readings_ptr) {
        total_readings_counter = total_readings_ptr;
    }
    
    /**
     * @brief Vincula el sensor con un sistema de logging de errores externo
     * @param log_error_func Puntero a función con firma: void(int code, int severity, uint32_t context)
     *        - code: Código de error (1=timeout, 2=lectura inválida, etc.)
     *        - severity: Nivel de severidad (1=warning, 2=error, 3=crítico, etc.)
     *        - context: Información contextual (tiempo transcurrido, voltaje*1000, NTU, etc.)
     * @note La función debe ser thread-safe si se usa en entorno multitarea (FreeRTOS).
     * @warning No pasar punteros a funciones lambda sin captura estática.
     */
    void setErrorLogger(void (*log_error_func)(int, int, uint32_t)) {
        error_logger = log_error_func;
    }
    
    // ——— FUNCIONES ADICIONALES ———
    
    /**
     * @brief Muestra información completa de calibración y estado del sensor por Serial
     * @details Imprime:
     *          - Estado de inicialización (inicializado / no inicializado)
     *          - Pin ADC configurado
     *          - Ecuación de calibración polinómica cúbica
     *          - Rango válido de turbidez (0 - 3000 NTU)
     *          - Rango válido de voltaje (0.1V - 2.5V)
     *          - Información de última lectura válida si existe
     * @note Útil para verificación rápida de configuración y diagnóstico de problemas.
     */
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
    
    /**
     * @brief Realiza una lectura de prueba y muestra resultados detallados paso a paso
     * @details Lee voltaje calibrado, calcula turbidez usando voltageToNTU(), y muestra
     *          cada etapa del proceso. No actualiza last_reading ni contadores globales.
     *          Ideal para verificación rápida sin afectar estadísticas del sistema.
     * @note Requiere sensor inicializado. Función bloqueante por ~50ms.
     */
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
    
    /**
     * @brief Función de debug para verificar voltaje crudo y conversión a NTU
     * @details Toma muestras del ADC, muestra:
     *          - Valor ADC promedio crudo
     *          - Voltaje calculado con calibración ESP32
     *          - Turbidez estimada usando voltageToNTU()
     * @note Útil para diagnosticar problemas de calibración o ADC.
     * @note No actualiza last_reading ni contadores. Solo para depuración.
     * @warning Requiere sensor inicializado. Función bloqueante por ~50ms.
     */
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
    
    /**
     * @brief Imprime curva completa de calibración voltaje vs NTU
     * @details Muestra tabla con conversión V→NTU en incrementos de 0.1V desde 0.6V hasta 2.2V.
     *          Útil para verificar comportamiento del algoritmo de conversión en todo el rango.
     * @note Permite visualizar linealidad y detectar anomalías en la calibración.
     * @note Usa el algoritmo segmentado actual implementado en voltageToNTU().
     */
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