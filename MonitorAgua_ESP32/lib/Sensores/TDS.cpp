/**
 * @file TDS.cpp
 * @brief Implementación del sensor TDS (Total Dissolved Solids) para ESP32
 * @details Este archivo contiene la lógica completa para inicialización, lectura,
 *          calibración y validación del sensor TDS (Sólidos Disueltos Totales).
 *          Implementa compensación por temperatura, conversión EC→TDS y algoritmo
 *          polinómico cúbico basado en la librería GravityTDS original.
 * @author Daniel Acosta - Santiago Erazo
 * @date 01/10/2025
 * @version 1.0
 */

 #include "TDS.h"

 /**
 * @namespace TDSSensor
 * @brief Espacio de nombres para todas las funcionalidades del sensor TDS
 * @details Contiene variables globales internas, funciones de lectura, calibración,
 *          compensación por temperatura y utilidades para manejo completo del sensor
 *          analógico TDS conectado al ADC del ESP32.
 */

// ——— Variables internas del módulo ———
namespace TDSSensor {

    /**
     * @brief Bandera de estado de inicialización del sensor TDS
     * @details Indica si el sensor ha sido correctamente inicializado y está listo
     *          para realizar lecturas. Evita operaciones sobre hardware no configurado.
     */    
    
    // Variables del sensor
    bool initialized = false;

    /**
     * @brief Pin GPIO asignado al ADC para lectura del sensor TDS
     * @details Debe ser un pin compatible con ADC1 del ESP32 (GPIO32-39).
     *          Por defecto toma el valor de TDS_SENSOR_PIN definido en TDS.h
     */
    uint8_t sensor_pin = TDS_SENSOR_PIN;

    /**
     * @brief Timestamp de la última lectura válida realizada
     * @details Almacena millis() del momento en que se completó exitosamente una
     *          lectura. Útil para calcular intervalos entre mediciones.
     */
    uint32_t last_reading_time = 0;

    /**
     * @brief Última estructura de lectura capturada por el sensor
     * @details Contiene TDS, EC, temperatura, voltaje, timestamp, estado y número
     *          de lectura. Se actualiza en cada llamada a takeReadingWithTimeout().
     */
    TDSReading last_reading = {0};

    /**
     * @brief Características de calibración del ADC del ESP32
     * @details Estructura que almacena los parámetros de calibración específicos del
     *          chip para convertir valores crudos ADC a voltajes reales (mV).
     */
    esp_adc_cal_characteristics_t adc_chars;
    
    // Variables de calibración 

    /**
     * @brief Factor de calibración de la celda TDS (kValue)
     * @details Coeficiente multiplicador que ajusta la conductividad eléctrica (EC)
     *          calculada según las características específicas de la celda/electrodo.
     *          Inicializado con TDS_CALIBRATED_KVALUE del header.
     * @note Valor típico: 1.0 (sin ajuste) a 2.0 (celdas de alta sensibilidad).
     * @warning Debe calibrarse con solución estándar conocida (ej: 1413 µS/cm).
     */
    float kValue = TDS_CALIBRATED_KVALUE;       //factor de calibración de la celda 

    /**
     * @brief Offset de voltaje para compensar errores del sensor o ADC
     * @details Valor restado al voltaje medido para corregir desviaciones sistemáticas
     *          del ADC o offset del amplificador del sensor. Inicializado con
     *          TDS_CALIBRATED_VOFFSET del header.
     * @note Unidad: Voltios (V). Típicamente entre 0.0V y 0.2V.
     * @warning Si voltaje calibrado resulta negativo, el offset es demasiado alto.
     */
    float voltageOffset = TDS_CALIBRATED_VOFFSET; //ajuste de voltaje para compensar errores del sensor o ADC
    
    // Constantes del sensor 

    /**
     * @brief Factor de conversión de EC a TDS
     * @details Relación empírica: TDS (ppm) = EC (µS/cm) × TDS_FACTOR.
     *          Valor estándar 0.5 indica que TDS = EC / 2.
     * @note Este factor varía según composición del agua:
     *       - Agua con sales: 0.5 - 0.7
     *       - Agua pura: 0.5 (estándar)
     *       - Agua industrial: 0.4 - 0.5
     */
    const float TDS_FACTOR = 0.5f;         // TDS = EC / 2

    /**
     * @brief Coeficiente de temperatura para compensación (2% por °C)
     * @details Define el cambio porcentual de conductividad por cada grado Celsius
     *          alejado de 25°C (temperatura de referencia estándar).
     * @note Compensación: factor = 1 + 0.02 × (T - 25°C)
     */
    const float TEMP_COEFFICIENT = 0.02f;  // 2% por °C

    /**
     * @brief Coeficiente cúbico (A3) del polinomio de calibración GravityTDS
     * @details Término de tercer grado en: EC_raw = A3×V³ + A2×V² + A1×V
     *          Calibrado empíricamente por el fabricante DFRobot para sensor Gravity TDS.
     */
    const float COEFF_A3 = 133.42f;        // Coeficiente cúbico

    /**
     * @brief Coeficiente cuadrático (A2) del polinomio de calibración GravityTDS
     * @details Término de segundo grado en: EC_raw = A3×V³ + A2×V² + A1×V
     *          Calibrado empíricamente por el fabricante DFRobot para sensor Gravity TDS.
     */
    const float COEFF_A2 = -255.86f;       // Coeficiente cuadrático

    /**
     * @brief Coeficiente lineal (A1) del polinomio de calibración GravityTDS
     * @details Término de primer grado en: EC_raw = A3×V³ + A2×V² + A1×V
     *          Calibrado empíricamente por el fabricante DFRobot para sensor Gravity TDS.
     */
    const float COEFF_A1 = 857.39f;        // Coeficiente lineal
    
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
     * @brief Lee voltaje calibrado del sensor TDS con promediado de muestras
     * @details Proceso:
     *          1. Toma SAMPLES (30) muestras del ADC con delay de 1ms entre c/u
     *          2. Descarta valores fuera de rango (0 - ADC_MAX_VALUE)
     *          3. Promedia muestras válidas
     *          4. Convierte valor crudo a voltaje (mV) usando calibración ESP32
     *          5. Convierte mV a voltios y resta voltageOffset
     * @return Voltaje calibrado en voltios (V). Puede ser negativo si offset muy alto.
     * @note Si voltaje resultante < 0, indica que voltageOffset está mal calibrado.
     * @warning Función bloqueante por ~30ms (SAMPLES × 1ms). No usar en ISR.
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
        
        // Aplicar offset calibrado directamente
        float voltage_v = (voltage_mv / 1000.0f) - voltageOffset;
        
        return voltage_v;
        //toma n muestras, descarta valores fuera de rango y promedia los datos obtenidos de datos crudos 
        //convierte a voltaje en mv y luego a voltaje en V y resta el offset
    }

    /**
     * @brief Compensa el voltaje medido según la temperatura del agua
     * @details Ajusta el voltaje para normalizar a 25°C (temperatura de referencia).
     *          La conductividad aumenta ~2% por cada °C sobre 25°C.
     *          Compensación: V_compensado = V_medido / (1 + 0.02 × (T - 25))
     * @param voltage Voltaje medido sin compensar (V)
     * @param temperature Temperatura actual del agua (°C)
     * @return Voltaje compensado normalizado a 25°C
     * @note Si T < 25°C, aumenta el voltaje (conductividad ajustada hacia arriba)
     *       Si T > 25°C, disminuye el voltaje (conductividad ajustada hacia abajo)
     */
    
    float compensateTemperature(float voltage, float temperature) {
        // Compensación de temperatura 
        float compensationFactor = 1.0f + TEMP_COEFFICIENT * (temperature - 25.0f);
        return voltage / compensationFactor;
        //ajusta el voltaje respecto a la temperatura 
    }
    
    /**
     * @brief Calcula conductividad eléctrica cruda (EC) usando polinomio cúbico
     * @details Implementa la ecuación polinómica de la librería GravityTDS original:
     *          EC_raw = 133.42×V³ - 255.86×V² + 857.39×V
     *          Donde V es el voltaje compensado por temperatura.
     * @param compensatedVoltage Voltaje ya compensado por temperatura (V)
     * @return Conductividad eléctrica cruda en µS/cm (sin factor kValue aplicado)
     * @note Esta EC debe multiplicarse por kValue para obtener EC final calibrada.
     * @warning Coeficientes válidos solo para sensor Gravity TDS de DFRobot.
     */

    float calculateECRaw(float compensatedVoltage) {
        // Ecuación polinómica de la librería GravityTDS
        return COEFF_A3 * compensatedVoltage * compensatedVoltage * compensatedVoltage +
               COEFF_A2 * compensatedVoltage * compensatedVoltage +
               COEFF_A1 * compensatedVoltage;
               //convierte el voltaje compensado a una CONDUCTIVIDAD ELÉCTRICA
               //polinomio cúbico de calibración de la librería original Gravity TDS
    }
    
    /**
     * @brief Calcula conductividad eléctrica final (EC) aplicando factor de calibración
     * @details Multiplica EC cruda por kValue para ajustar según características
     *          específicas de la celda/electrodo usado.
     * @param compensatedVoltage Voltaje compensado por temperatura (V)
     * @return Conductividad eléctrica calibrada en µS/cm
     * @note EC = EC_raw × kValue, donde kValue típicamente está entre 1.0 y 2.0
     */
    float calculateEC(float compensatedVoltage) {
        // multiplica por el factor de calibración del electrodo kValue
        return calculateECRaw(compensatedVoltage) * kValue;
    }
    
    /**
     * @brief Convierte conductividad eléctrica (EC) a TDS (Total Dissolved Solids)
     * @details Usa factor de conversión estándar: TDS = EC × 0.5
     *          Esto significa que TDS (ppm) ≈ EC (µS/cm) / 2
     * @param ec Conductividad eléctrica en µS/cm
     * @return TDS (Sólidos Disueltos Totales) en ppm (partes por millón)
     * @note Factor 0.5 es estándar para agua potable. Puede variar según composición.
     */
    float calculateTDS(float ec) {
        // convierte conductividad a TDS usando factor TDS (definido en 0.5)
        return ec * TDS_FACTOR;
    }
    
    // ——— IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS ———

    /**
     * @brief Inicializa el sensor TDS en el pin ADC especificado
     * @details Configura el ADC con:
     *          - Resolución: 12 bits (0-4095)
     *          - Atenuación: 6dB (rango 0-2.2V, apropiado para sensor TDS que entrega hasta 2.0V)
     *          - Calibración específica del chip ESP32
     *          Es seguro llamar múltiples veces (verifica si ya está inicializado).
     * @param pin Pin GPIO compatible con ADC1 del ESP32 (por defecto TDS_SENSOR_PIN)
     * @return true si inicialización exitosa o ya estaba inicializado
     * @note Requiere llamarse una vez en setup() antes de usar otras funciones.
     * @warning Discrepancia: analogSetPinAttenuation usa ADC_6db pero
     *          esp_adc_cal_characterize usa ADC_ATTEN_DB_6 y ADC_WIDTH_BIT_13.
     *          PENDIENTE: Verificar consistencia (similar a sensor pH).
     */
    
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
    
    /**
     * @brief Realiza una lectura completa de TDS (wrapper de takeReadingWithTimeout)
     * @details Función de conveniencia que llama internamente a takeReadingWithTimeout().
     * @param temperature Temperatura actual del agua en °C (usada para compensación)
     * @return Estructura TDSReading con resultado completo de la medición
     * @see takeReadingWithTimeout() para detalles completos del proceso de lectura
     */
    TDSReading takeReading(float temperature) {
        return takeReadingWithTimeout(temperature);
    }
    
    /**
     * @brief Realiza lectura completa de TDS con control de timeout y validación exhaustiva
     * @details Función principal para toma de datos. Proceso completo:
     *          1. Verifica inicialización del sensor
     *          2. Incrementa contador global de lecturas
     *          3. Lee voltaje calibrado (30 muestras promediadas con offset aplicado)
     *          4. Verifica timeout de operación (< TDS_OPERATION_TIMEOUT)
     *          5. Valida rango de voltaje (MIN_VALID_VOLTAGE - MAX_VALID_VOLTAGE)
     *          6. Compensa voltaje por temperatura usando coeficiente 2%/°C
     *          7. Calcula EC usando polinomio cúbico GravityTDS × kValue
     *          8. Convierte EC a TDS usando factor 0.5
     *          9. Valida rangos de TDS y EC
     *          10. Actualiza last_reading y registra errores si corresponde
     * @param temperature Temperatura del agua en °C (requerida para compensación precisa)
     * @return Estructura TDSReading con campos:
     *         - tds_value: TDS en ppm (0.0 si inválido)
     *         - ec_value: EC en µS/cm (0.0 si inválido)
     *         - temperature: Temperatura usada en la compensación
     *         - timestamp: millis() al momento de la lectura
     *         - reading_number: Número secuencial de lectura
     *         - valid: true si lectura válida y dentro de todos los rangos
     *         - sensor_status: Código bit-field de estado (ver TDS_STATUS_*)
     * @note Si hay timeout o valores fuera de rango, decrementa el contador global.
     * @warning Función bloqueante por ~30ms (tiempo de muestreo del ADC).
     */
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
    
    /**
     * @brief Función de debug para verificar voltaje crudo y offset aplicado
     * @details Toma muestras del ADC sin aplicar offset y muestra:
     *          - Voltaje crudo (antes de restar offset)
     *          - Offset actual configurado
     *          - Voltaje final (crudo - offset)
     *          - Sugerencia de nuevo offset si el voltaje final es negativo
     * @note Útil para diagnosticar problemas de calibración donde voltaje < 0.
     * @note No actualiza last_reading ni contadores. Solo para depuración.
     * @warning Requiere sensor inicializado. Función bloqueante por ~30ms.
     */
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
    
    /**
     * @brief Establece nuevos parámetros de calibración manualmente
     * @details Actualiza kValue (factor de celda) y voltageOffset (offset ADC).
     *          Útil después de calibración externa con solución estándar conocida.
     * @param kVal Nuevo factor de calibración de celda (típicamente 1.0 - 2.0)
     * @param vOffset Nuevo offset de voltaje en voltios (típicamente 0.0 - 0.2V)
     * @note Imprime confirmación de cambios en Serial para verificación.
     */
    void setCalibration(float kVal, float vOffset) {
        kValue = kVal;
        voltageOffset = vOffset;
        Serial.printf(" Calibración TDS actualizada: k=%.6f, offset=%.6fV\n", kValue, voltageOffset);
    }
    
    /**
     * @brief Obtiene los parámetros de calibración actuales por referencia
     * @details Permite al sistema principal consultar calibración sin modificarla.
     *          Útil para guardar configuración en memoria persistente (EEPROM, RTC Memory).
     * @param[out] kVal Referencia donde se almacenará el kValue actual
     * @param[out] vOffset Referencia donde se almacenará el voltageOffset actual
     */
    void getCalibration(float& kVal, float& vOffset) {
        kVal = kValue;
        vOffset = voltageOffset;

        //Serial.printf(" getCalibration() - k=%.6f, offset=%.6fV\n", kVal, vOffset);
    }
    
    /**
     * @brief Restablece la calibración a valores por defecto del header
     * @details Restaura kValue y voltageOffset a TDS_CALIBRATED_KVALUE y
     *          TDS_CALIBRATED_VOFFSET. Útil para resetear calibraciones incorrectas
     *          o volver a estado de fábrica.
     * @note Imprime confirmación en Serial.
     */
    void resetToDefaultCalibration() {
        kValue = TDS_CALIBRATED_KVALUE;
        voltageOffset = TDS_CALIBRATED_VOFFSET;
        Serial.printf(" Calibración restaurada a valores por defecto: k=%.6f, offset=%.6fV\n", 
                     kValue, voltageOffset);
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
     * @brief Obtiene el valor TDS de la última lectura
     * @return TDS en ppm (0.0 si última lectura fue inválida)
     */
    float getLastTDS() { 
        return last_reading.tds_value; 
    }
    
    /**
     * @brief Obtiene la conductividad eléctrica de la última lectura
     * @return EC en µS/cm (0.0 si última lectura fue inválida)
     */
    float getLastEC() { 
        return last_reading.ec_value; 
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
     * @details Muestra: número de lectura, TDS, EC, temperatura, timestamp, estado.
     *          Útil para depuración y monitoreo en tiempo real.
     * @note Si no hay lecturas previas (reading_number == 0), informa al usuario.
     */
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
    
    /**
     * @brief Valida si un valor TDS está dentro del rango aceptable
     * @param tds Valor TDS a validar en ppm
     * @return true si TDS está entre MIN_VALID_TDS (0.0) y MAX_VALID_TDS (2000.0) y no es NaN
     */
    bool isTDSInRange(float tds) {
        return (tds >= MIN_VALID_TDS && tds <= MAX_VALID_TDS && !isnan(tds));
    }
    
    /**
     * @brief Valida si un valor EC está dentro del rango aceptable
     * @param ec Valor EC a validar en µS/cm
     * @return true si EC está entre MIN_VALID_EC (0.0) y MAX_VALID_EC (4000.0) y no es NaN
     */
    bool isECInRange(float ec) {
        return (ec >= MIN_VALID_EC && ec <= MAX_VALID_EC && !isnan(ec));
    }
    
    /**
     * @brief Valida si un voltaje está dentro del rango aceptable del sensor
     * @param voltage Voltaje en voltios a validar
     * @return true si voltaje está entre MIN_VALID_VOLTAGE (0.001V) y MAX_VALID_VOLTAGE (2.2V) y no es NaN
     */
    bool isVoltageInRange(float voltage) {
        return (voltage >= MIN_VALID_VOLTAGE && voltage <= MAX_VALID_VOLTAGE && !isnan(voltage));
    }
    
    /**
     * @brief Clasifica la calidad del agua según su TDS
     * @param tds Valor TDS a clasificar en ppm
     * @return String descriptivo de la calidad del agua:
     *         - "Muy pura" (TDS < 50 ppm) - Agua destilada/osmosis inversa
     *         - "Excelente" (50 ≤ TDS < 150 ppm) - Agua embotellada premium
     *         - "Buena" (150 ≤ TDS < 300 ppm) - Agua potable de calidad
     *         - "Aceptable" (300 ≤ TDS < 500 ppm) - Agua potable estándar
     *         - "Pobre" (500 ≤ TDS < 900 ppm) - Calidad marginal
     *         - "Muy pobre" (TDS ≥ 900 ppm) - No recomendada para consumo
     * @note Clasificación según estándares EPA y OMS para agua potable.
     */
    String getWaterQuality(float tds) {
        if (tds < 50) return "Muy pura";
        else if (tds < 150) return "Excelente";
        else if (tds < 300) return "Buena";
        else if (tds < 500) return "Aceptable";
        else if (tds < 900) return "Pobre";
        else return "Muy pobre";
    }
    
    // ——— FUNCIONES DE INTEGRACIÓN ———
    
    /**
     * @brief Vincula el sensor con un contador global de lecturas del sistema
     * @details Permite que el módulo TDS incremente automáticamente un contador externo
     *          en cada lectura válida. Útil para estadísticas globales del sistema.
     * @param total_readings_ptr Puntero a uint16_t que será incrementado en cada lectura válida
     * @note El puntero debe apuntar a memoria válida durante toda la vida útil del sensor.
     * @warning No pasar punteros a variables locales que puedan salir de scope.
     */
    void setReadingCounter(uint16_t* total_readings_ptr) {
        total_readings_counter = total_readings_ptr;
    }
    
    /**
     * @brief Vincula el sensor con un sistema de logging de errores externo
     * @details Permite que el módulo TDS reporte errores (timeout, lectura inválida, etc.)
     *          a un sistema centralizado de gestión de errores o logger.
     * @param log_error_func Puntero a función con firma: void(int code, int severity, uint32_t context)
     *        - code: Código de error (1=timeout, 2=lectura inválida, etc.)
     *        - severity: Nivel de severidad (1=warning, 2=error, 3=crítico, etc.)
     *        - context: Información contextual (tiempo transcurrido, voltaje*1000, TDS, etc.)
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
     *          - kValue (factor de calibración de celda)
     *          - voltageOffset (offset de voltaje ADC)
     *          - TDS_FACTOR (relación EC→TDS)
     *          - Coeficientes del polinomio cúbico (A3, A2, A1)
     *          - Información de última lectura válida si existe
     * @note Útil para verificación rápida de configuración y diagnóstico de problemas.
     */
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
    
    /**
     * @brief Realiza una lectura de prueba y muestra resultados detallados paso a paso
     * @details Lee voltaje calibrado, compensa por temperatura (asumiendo 25°C), calcula
     *          EC y TDS, y muestra cada etapa del proceso. No actualiza last_reading ni
     *          contadores globales. Ideal para verificación rápida sin afectar estadísticas.
     * @note Requiere sensor inicializado. Función bloqueante por ~30ms.
     * @note Usa temperatura fija de 25°C (sin compensación) para simplificar debug.
     */
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