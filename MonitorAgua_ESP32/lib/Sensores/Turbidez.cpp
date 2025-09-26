#include "Turbidez.h" // Incluye el archivo de cabecera asociado (Turbidez.h), 
                      // donde se definen las estructuras, constantes, prototipos de funciones 
                      // y macros que este archivo .cpp necesita para compilarse correctamente.

// ——— Variables internas del módulo ———
namespace TurbiditySensor { // Se abre un namespace (espacio de nombres) llamado TurbiditySensor
                              // Esto sirve para encapsular todas las variables y funciones del sensor
                              // evitando conflictos de nombres con otros módulos.
    
    // Variables del sensor
    bool initialized = false;  // Bandera que indica si el sensor ya fue inicializado (true) o no (false).
    uint8_t sensor_pin = TURBIDITY_SENSOR_PIN; // Pin de entrada analógica donde está conectado el sensor de turbidez.
    uint32_t last_reading_time = 0; // Almacena el tiempo (en ms) de la última lectura válida del sensor.
    TurbidityReading last_reading = {0}; // Estructura que guarda la última lectura realizada (se inicializa en 0).
    esp_adc_cal_characteristics_t adc_chars; // Estructura especial para manejar la calibración del ADC en el ESP32
    
    // Coeficientes de calibración 
    float calib_a = CALIB_COEFF_A; // Coeficiente "a" para la ecuación polinómica de calibración del sensor.
    float calib_b = CALIB_COEFF_B; // Coeficiente "b".
    float calib_c = CALIB_COEFF_C; // Coeficiente "c".
    float calib_d = CALIB_COEFF_D; // Coeficiente "d".
    
    // Configuración ADC
    const int ADC_BITS = 12; // Resolución del ADC en bits (12 bits → valores entre 0 y 4095).
    const int ADC_MAX_VALUE = 4095; // Valor máximo que puede devolver el ADC de 12 bits.
    const int ADC_VREF = 1100; // Voltaje de referencia del ADC (en milivoltios). Por defecto ~1100 mV en ESP32.
    
    // Variables de integración con sistema principal
    uint16_t* total_readings_counter = nullptr; // Puntero a un contador global de lecturas (se usa si se conecta al sistema).
    void (*error_logger)(int code, int severity, uint32_t context) = nullptr; 
    // Puntero a función para registrar errores en el sistema principal. Recibe: código, severidad y contexto.

    
    // ——— FUNCIONES INTERNAS ———
    
    float readCalibratedVoltage() { // Función interna para leer el voltaje promedio del sensor ya calibrado.
        long sum = 0; // Acumulador de todas las muestras del ADC.
        int validSamples = 0; // Contador de cuántas muestras fueron válidas.
        
        for (int i = 0; i < SAMPLES; i++) { // Bucle que toma varias muestras para mejorar estabilidad.
            int rawValue = analogRead(sensor_pin); // Lee el valor crudo (sin calibración) del ADC en el pin del sensor.
            if (rawValue >= 0 && rawValue <= ADC_MAX_VALUE) { // Verifica que la lectura esté dentro del rango válido.
                sum += rawValue; // Acumula el valor válido.
                validSamples++; // Aumenta el número de muestras válidas.
            }
            delayMicroseconds(1000); // Espera 1 ms entre muestras para evitar lecturas demasiado seguidas.
        }
        
        if (validSamples == 0) return 0.0f; // Si no hubo ninguna muestra válida, retorna 0.0 (error en lectura).
        
        float avgRaw = (float)sum / validSamples; // Calcula el valor promedio de las lecturas válidas.
        uint32_t voltage_mv = esp_adc_cal_raw_to_voltage((uint32_t)avgRaw, &adc_chars);
        // Convierte el valor promedio del ADC a milivoltios usando la calibración propia del ESP32.
        
        // Convertir a voltios
        float voltage_v = voltage_mv / 1000.0f; // Convierte de mV a V
        
        return voltage_v; // Retorna el voltaje promedio ya calibrado en voltios.
    }
    
    // ——— IMPLEMENTACIÓN DE FUNCIONES PÚBLICAS ———
    
    bool initialize(uint8_t pin) { // Función para inicializar el sensor en el pin especificado.
        if (initialized) {// Si el sensor ya estaba inicializado previamente...
        //Serial.println(" Sensor turbidez ya inicializado"); // Mensaje opcional de depuración.
            //Serial.println(" Sensor turbidez ya inicializado");
            return true;
        }
        
        //Serial.printf(" Inicializando sensor turbidez (pin %d)...\n", pin);
        
        sensor_pin = pin; // Se asigna el pin recibido como pin del sensor.

        
        // Configurar ADC con calibración ESP32
        analogReadResolution(ADC_BITS); // Se establece la resolución de lectura del ADC (12 bits en este caso).
        analogSetPinAttenuation(sensor_pin, ADC_11db); 
        // Se configura la atenuación del pin analógico (ADC_11db → rango aprox. 0 - 3.6V en ESP32).
  
        // Calibrar ADC específico para ESP32
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
            ADC_UNIT_1, // Se usa la unidad ADC #1 del ESP32.
            ADC_ATTEN_DB_12, // Se establece la atenuación en 12 dB (permite hasta ~3.6V).
            ADC_WIDTH_BIT_13, // Se fija el ancho en 13 bits para caracterización (aunque resol. es 12 bits).
            ADC_VREF, // Se usa el valor de referencia definido (1100 mV).
            &adc_chars // Se pasa la estructura para guardar los parámetros calibrados.
        );
        
        initialized = true; // Marca el sensor como inicializado.
        last_reading_time = millis(); // Registra el tiempo de inicialización como último tiempo de lectura.
        
        //Serial.println(" Sensor turbidez inicializado correctamente");
        return true; // Retorna true indicando que la inicialización fue exitosa.
    }
    
    void cleanup() { // Función para "limpiar" el estado del sensor
        initialized = false; // Marca el sensor como no inicializado.
        //Serial.println(" Sensor turbidez limpiado");
    }
    
    TurbidityReading takeReading() { // Función para tomar una lectura simple del sensor.   
        return takeReadingWithTimeout(); // En realidad llama a la función más completa que incluye timeout.
    }
    
    TurbidityReading takeReadingWithTimeout() { // Función principal que realiza la lectura del sensor con validaciones. 
        TurbidityReading reading = {0}; // Se crea una nueva estructura de lectura y se inicializa en cero.
        
        if (!initialized) { // Si el sensor no está inicializado...     
            Serial.println(" Sensor turbidez no inicializado");
            reading.valid = false; // Marca la lectura como inválida.
            reading.sensor_status = TURBIDITY_STATUS_INVALID_READING; // Estado de lectura inválida
            return reading; // Retorna de inmediato la lectura inválida
        }
        
        // Incrementar contador
        if (total_readings_counter) { // Si existe un puntero válido al contador global de lecturas...
            (*total_readings_counter)++; // Incrementa el contador total de lecturas.
            reading.reading_number = *total_readings_counter; // Guarda el número de lectura en la estructura.  
        }
        
        reading.timestamp = millis(); // Se registra el tiempo en ms de esta lectura
        
        // Timeout para operación del sensor
        uint32_t start_time = millis(); // Marca el inicio del tiempo de lectura.
        
        // Leer voltaje calibrado
        float voltage = readCalibratedVoltage();  // Llama a la función interna que obtiene el voltaje promedio del sensor.
        
        // Verificar timeout
        if (millis() - start_time > TURBIDITY_OPERATION_TIMEOUT) { // Si el tiempo de lectura supera el límite permitido...
            Serial.println(" Timeout en lectura de sensor turbidez");
            
            if (error_logger) { // Si existe un logger de errores definido...
                error_logger(1, 1, millis() - start_time); // ERROR_SENSOR_TIMEOUT, SEVERITY_WARNING. (Se reporta un error (código 1, severidad 1, con el tiempo excedido como contexto).)
            }
            
            reading.valid = false; // La lectura se marca como inválida.
            reading.sensor_status = TURBIDITY_STATUS_TIMEOUT; // Estado: error por timeout.
            
            if (total_readings_counter) {
                (*total_readings_counter)--; // Se revierte el incremento del contador de lecturas.
            }
            
            last_reading = reading; // Se guarda esta lectura (inválida) como la última.
            return reading; // Se retorna la lectura con error.
        }
        
        // Validar voltaje
        if (!isVoltageInRange(voltage)) { // Si el voltaje medido no está dentro del rango válido...
            if (voltage < MIN_VALID_VOLTAGE) { // Si es demasiado bajo...
                reading.sensor_status = TURBIDITY_STATUS_VOLTAGE_LOW; // Estado: voltaje bajo.
                Serial.printf(" Voltaje turbidez muy bajo: %.3fV\n", voltage);
            } else { // Si es demasiado alto...
                reading.sensor_status = TURBIDITY_STATUS_VOLTAGE_HIGH; // Estado: voltaje alto.
                Serial.printf(" Voltaje turbidez muy alto: %.3fV\n", voltage);
            }
            
            reading.valid = false; // Lectura inválida.
            reading.turbidity_ntu = 0.0; // Se asigna 0.0 NTU (no confiable).
            reading.voltage = voltage; // Se guarda el voltaje medido igualmente.
            
            if (error_logger) { // Si existe logger de errores...
                error_logger(2, 1, (uint32_t)(voltage * 1000)); // ERROR_SENSOR_INVALID_READING
                // Reporta error: código 2, severidad 1, con el voltaje en mV como contexto.
            }
            
            if (total_readings_counter) {
                (*total_readings_counter)--; // Reversa el incremento del contador.
            }
            
            last_reading = reading; // Se guarda como última lectura.
            return reading; // Retorna esta lectura inválida.
        }
        
        // Calcular turbidez usando calibración
        float ntu = voltageToNTU(voltage); // Llama a la función que transforma el voltaje medido (V) a turbidez (NTU)
                                            // usando la curva/algoritmo de calibración definido en este módulo.    
        
        // Validar resultados
        if (isTurbidityInRange(ntu)) { // Comprueba si el valor calculado de NTU está dentro de los límites aceptables
            reading.turbidity_ntu = ntu; // Guarda el valor de turbidez calculado en la estructura de lectura
            reading.voltage = voltage; // Guarda el voltaje crudo asociado (útil para depuración y trazabilidad)
            reading.valid = true; // Marca la lectura como válida (pasa las comprobaciones)
            reading.sensor_status = TURBIDITY_STATUS_OK; // Código de estado indicando lectura correcta
            
            last_reading_time = millis(); // Actualiza la variable con el tiempo (ms) en que se tomó la lectura
            
            Serial.printf(" Turbidez: %.1f NTU | V: %.3fV | %s (%.0f ms)\n", 
                         ntu, voltage, getWaterQuality(ntu).c_str(), millis() - start_time);
            // Imprime en consola: el valor de NTU, el voltaje, la etiqueta de calidad y el tiempo que tomó la operación.
        } else { // Rama ejecutada si la NTU calculada NO está dentro del rango válido
            reading.turbidity_ntu = 0.0; // Para seguridad, asigna 0.0 al campo NTU (no confiar en el valor)
            reading.voltage = voltage; // Sigue guardando el voltaje leído para diagnósticos posteriores
            reading.valid = false; // Marca la lectura como inválida (no la consideraremos fiable)
            
            if (ntu > MAX_VALID_NTU) { // Si el valor calculado excede el límite máximo definido...
                reading.sensor_status = TURBIDITY_STATUS_OVERFLOW; // Marca como desbordamiento/overflow
                Serial.printf(" Turbidez fuera de rango: %.1f NTU (máximo: %.0f)\n", ntu, MAX_VALID_NTU);
                // Imprime aviso indicando que la turbidez excede el máximo esperado.
            } else {
                reading.sensor_status = TURBIDITY_STATUS_INVALID_READING; // Otro tipo de lectura inválida
                Serial.printf(" Lectura turbidez inválida: %.1f NTU\n", ntu); // Imprime que la lectura es inválida 
            }
            
            if (error_logger) {
                error_logger(2, 1, (uint32_t)ntu); // ERROR_SENSOR_INVALID_READING
                // Si se ha provisto una función de registro de errores, se la invoca con:
                //    code = 2 (lectura inválida), severity = 1 (advertencia), context = ntu (convertido a entero)
            }
            
            if (total_readings_counter) {
                (*total_readings_counter)--; // Revertir el incremento del contador si la lectura fue inválida
                // Esto garantiza que el contador represente solo lecturas exitosas/contabilizadas.
            }
        }
        
        last_reading = reading; // Guarda la estructura `reading` en `last_reading` para consultas futuras
        return reading; // Retorna la lectura completa (válida o no) al llamador
    }
    
    // ——— FUNCIONES DE CALIBRACIÓN ———
    
    float voltageToNTU(float voltage) { // Función central que mapea voltaje (V) → turbidez (NTU)
        
        // Aplicar ecuación polinómica cúbica calibrada
        // NTU = a*V³ + b*V² + c*V + d
        float v = voltage; // Copia local: valor del voltaje para facilitar lectura
        float ntu = calib_a * v * v * v +  // Evalúa el polinomio de grado 3 con coeficientes calibrados
                   calib_b * v * v + 
                   calib_c * v + 
                   calib_d;
        // Resultado inicial de la curva cúbica (puede ser corregido por secciones siguientes).           
        
        // Asegurar que NTU no sea negativo
        if (ntu < 0) ntu = 0; // Si por la curva polinómica sale negativo (no tiene sentido físico), lo corrige a 0
        
        if (voltage > 2.15f) { // Caso: voltaje alto (extremo superior) — regla empírica para saturación
            ntu = 3000.0f * (2.2f - voltage) / (2.2f - 0.65f);
            // Mapeo alternativo en tramo superior: evita resultados no físicos y aproxima comportamiento en saturación
            if (ntu < 0) ntu = 0; // Protege de valores negativos tras la transformación    
            if (ntu > 10) ntu = 10; // Limita el tramo superior a 10 NTU (ajuste empírico usado por el autor)
        }
        else if (voltage < 0.7f) { // Caso: voltaje muy bajo (extremo inferior)
            ntu = 1000.0f + (0.7f - voltage) * 2000.0f; 
            // Si el voltaje cae por debajo de 0.7V, la función fuerza un incremento fuerte (valores grandes de NTU)
            if (ntu > 3000) ntu = 3000;  // Límite superior absoluto en este tramo (corresponde al rango máximo del sensor)
        }
        else { // Caso intermedio (0.7V .. ~2.15V) — tramo "principal"
            ntu = 1500.0f * (2.18f - voltage) / (2.18f - 0.65f);
            // Mapeo intermedio lineal/afín ajustado empíricamente para este rango
            if (ntu < 0) ntu = 0; // Protección: nunca devolver NTU negativo
        }
        
        return ntu; // Devuelve el valor final de turbidez calculado
    }
    
    float calibrateReading(float rawVoltage) {
        return voltageToNTU(rawVoltage); // Función auxiliar / alias: mantiene compatibilidad de nombre
                                           // con otras partes del código que esperen `calibrateReading()
    }
    
    void setCalibrationCoefficients(float a, float b, float c, float d) {
        calib_a = a; // Asigna coeficiente cúbico 'a' desde parámetros externos
        calib_b = b; // Asigna 'b'
        calib_c = c; // Asigna 'c'
        calib_d = d; // Asigna 'd'
        Serial.printf(" Calibración turbidez actualizada: a=%.1f, b=%.1f, c=%.1f, d=%.1f\n", 
                     calib_a, calib_b, calib_c, calib_d);
        // Imprime los nuevos coeficientes para verificación. Útil tras cargar calibración desde PC/usuario.
    }
    
    void getCalibrationCoefficients(float& a, float& b, float& c, float& d) {
        a = calib_a; // Devuelve por referencia el valor actual del coeficiente 'a'
        b = calib_b; // Devuelve 'b'
        c = calib_c; // Devuelve 'c'
        d = calib_d; // Devuelve 'd'
        // Esta función permite que otras partes del sistema (o interfaz) lean la calibración actual.
    }
    
    void resetToDefaultCalibration() {
        calib_a = CALIB_COEFF_A; // Restaura 'a' al valor por defecto definido en el header
        calib_b = CALIB_COEFF_B; // Restaura 'b'
        calib_c = CALIB_COEFF_C; // Restaura 'c'
        calib_d = CALIB_COEFF_D; // Restaura 'd'
        Serial.printf(" Calibración turbidez restaurada a valores por defecto\n"); 
        // Mensaje para confirmar que se ha revertido la calibración a los parámetros de fábrica/proyecto.
    }
    
    // ——— FUNCIONES DE ESTADO ———
    
    bool isInitialized() { 
        return initialized; // Devuelve true si el módulo ya fue inicializado correctamente
    }
    
    bool isLastReadingValid() { 
        return last_reading.valid; // Indica si la última lectura registrada fue considerada válida 
    }
    
    float getLastTurbidity() { 
        return last_reading.turbidity_ntu; // Devuelve el último valor de turbidez (NTU) conocido 
    }
    
    float getLastVoltage() {
        return last_reading.voltage; // Devuelve el último voltaje medido (V)
    }
    
    uint32_t getLastReadingTime() { 
        return last_reading_time; // Devuelve el tiempo (ms) en que se registró la última lectura válida
    }
    
    uint16_t getTotalReadings() {
        return total_readings_counter ? *total_readings_counter : 0;
        // Si existe un contador global, devuelve su valor; si no, devuelve 0
    }
    
    // ——— FUNCIONES DE UTILIDAD ———
    
    void printLastReading() {
        if (last_reading.reading_number == 0) {
            Serial.println(" No hay lecturas turbidez previas"); // Mensaje si nunca se registró lectura
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
        // Muestra estado en hexadecimal y su interpretación (VÁLIDA/INVÁLIDA)
        Serial.printf("Calidad: %s\n", getWaterQuality(last_reading.turbidity_ntu).c_str());
        // Muestra una etiqueta cualitativa de calidad basada en NTU 
        Serial.printf("Categoría: %s\n", getTurbidityCategory(last_reading.turbidity_ntu).c_str());
        // Muestra una categoría más descriptiva
        Serial.println("---------------------------");
    }
    
    bool isTurbidityInRange(float ntu) {
        return (ntu >= MIN_VALID_NTU && ntu <= MAX_VALID_NTU && !isnan(ntu));
        // Devuelve true si ntu está dentro de los límites configurados y no es NaN
    }
    
    bool isVoltageInRange(float voltage) {
        return (voltage >= MIN_VALID_VOLTAGE && voltage <= MAX_VALID_VOLTAGE && !isnan(voltage));
         // Verifica que el voltaje leido esté dentro del rango seguro y esperable para el sensor
    }
    
    String getWaterQuality(float ntu) { // Etiquetado: NTU
        if (ntu <= 1) return "Excelente";
        else if (ntu <= 4) return "Muy buena";
        else if (ntu <= 10) return "Buena";
        else if (ntu <= 25) return "Aceptable";
        else if (ntu <= 100) return "Pobre";
        else return "Muy pobre";
    }
    
    String getTurbidityCategory(float ntu) { // Categoría descriptiva más detallada
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
        total_readings_counter = total_readings_ptr; // Asocia el puntero del contador global al módulo
    }
    
    void setErrorLogger(void (*log_error_func)(int, int, uint32_t)) {
        error_logger = log_error_func; // Registra la función externa que manejará errores
    }
    
    // ——— FUNCIONES ADICIONALES ———
    
    void showCalibrationInfo() {
        Serial.println(" === INFORMACIÓN DE CALIBRACIÓN TURBIDEZ ===");
        Serial.printf("Estado: %s\n", initialized ? "Inicializado" : "No inicializado");
        Serial.printf("Pin ADC: %d\n", sensor_pin);
        Serial.printf("Ecuación: NTU = %.1f*V³ + %.1f*V² + %.1f*V + %.1f\n", 
                     calib_a, calib_b, calib_c, calib_d);
        // Imprime la ecuación polinómica actual con los coeficientes usados para calibración
        Serial.printf("Rango válido: %.0f - %.0f NTU\n", MIN_VALID_NTU, MAX_VALID_NTU); // Muestra el rango válido configurado en macros
        Serial.printf("Voltaje válido: %.1f - %.1fV\n", MIN_VALID_VOLTAGE, MAX_VALID_VOLTAGE); // Muestra el rango de voltaje aceptado para la sonda/ADC
        
        if (last_reading.valid) {
            Serial.printf("Última lectura: %.1f NTU (%.3fV) - %s\n", 
                         last_reading.turbidity_ntu, last_reading.voltage,
                         getWaterQuality(last_reading.turbidity_ntu).c_str());
                         // Si existe una lectura válida, imprímela con una etiqueta de calidad
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
        
        float voltage = readCalibratedVoltage(); // Toma una medición de voltaje promedio/calibrado
        Serial.printf("Voltaje medido: %.6fV\n", voltage); // Imprime el voltaje con alta resolución
        
        if (isVoltageInRange(voltage)) { // Si el voltaje está dentro del rango aceptable...
            float ntu = voltageToNTU(voltage); // Calcula la turbidez
            
            Serial.printf("Turbidez calculada: %.1f NTU\n", ntu); // Imprime NTU
            Serial.printf("Calidad del agua: %s\n", getWaterQuality(ntu).c_str()); // Imprime calidad
            Serial.printf("Categoría: %s\n", getTurbidityCategory(ntu).c_str()); // Imprime categoría
        } else {
            Serial.printf(" Voltaje fuera de rango válido (%.1f-%.1fV)\n", 
                         MIN_VALID_VOLTAGE, MAX_VALID_VOLTAGE);
            // Si está fuera de rango, informa cuál es el rango válido configurado
        }
        
        Serial.println("========================");
    }
    
    void debugVoltageReading() {
        if (!initialized) return; // Si no está inicializado, no hacer nada
        
        Serial.println("🔬 === DEBUG VOLTAJE TURBIDEZ ===");
        
        // Leer voltaje crudo
        long sum = 0;
        for (int i = 0; i < SAMPLES; i++) {
            sum += analogRead(sensor_pin); // Acumula lecturas crudas del ADC
            delayMicroseconds(1000); // Espacio entre lecturas para mitigar ruido/conmutación   
        }
        
        float avgRaw = (float)sum / SAMPLES; // Promedio de valores crudos del ADC
        uint32_t voltage_mv = esp_adc_cal_raw_to_voltage((uint32_t)avgRaw, &adc_chars); // Convierte el promedio crudo en milivoltios considerando la calibración ADC del ESP32
        float voltage = voltage_mv / 1000.0f; // Convierte de mV a V para presentarlo   
        
        Serial.printf("Valor ADC promedio: %.1f\n", avgRaw); // Imprime la media del ADC (un número en 0..4095)
        Serial.printf("Voltaje calculado: %.6fV\n", voltage); // Imprime voltaje con precisión
        Serial.printf("Turbidez estimada: %.1f NTU\n", voltageToNTU(voltage)); // Muestra la NTU estimada
        
        Serial.println("==============================");
    }
    
    void printCalibrationCurve() {
        Serial.println(" === CURVA DE CALIBRACIÓN TURBIDEZ CORREGIDA ===");
        Serial.println("Voltaje (V) | Turbidez (NTU) | Calidad");
        Serial.println("------------|---------------|----------");
        
        for (float v = 0.6; v <= 2.2; v += 0.1) { // Recorre voltajes típicos del sensor en pasos de 0.1V
            float ntu = voltageToNTU(v); // Calcula NTU para cada voltaje de la serie
            Serial.printf("   %.2fV    |    %.1f NTU    | %s\n", 
                         v, ntu, getWaterQuality(ntu).c_str());
            // Imprime la fila con voltaje, NTU y etiqueta de calidad para facilitar revisión visual
        }
    }
    
} // namespace TurbiditySensor