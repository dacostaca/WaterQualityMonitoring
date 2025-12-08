# Water Quality Monitoring System 🌊

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-compatible-blue.svg)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/ESP32-compatible-green.svg)](https://www.espressif.com/en/products/socs/esp32)

Un sistema autónomo de monitoreo de calidad del agua basado en ESP32 con cuatro sensores: pH, temperatura, TDS y turbidez. Incluye implementación de Deep Sleep y Watchdog Timer para operación autónoma y monitoreo de errores.

---

## 📋 Tabla de Contenidos

- [Descripción General](#-descripción-general)
- [Estructura del Repositorio](#-estructura-del-repositorio)
  - [Etapa I - Trabajo Previo](#etapa-i---trabajo-previo)
  - [Etapa II - Desarrollo Actual](#etapa-ii---desarrollo-actual)
  - [Código del Firmware (MonitorAgua_ESP32)](#código-del-firmware-monitoragua_esp32)
  - [Código del Servidor (monitor_agua_pagina)](#código-del-servidor-monitor_agua_pagina)
- [Documentación del Código](#-documentación-del-código)
- [Inicio Rápido](#-inicio-rápido)
- [Requisitos](#-requisitos)
- [Licencia](#-licencia)

---

## 🌟 Descripción General

Este proyecto implementa un sistema de monitoreo de calidad del agua diseñado para operar de forma autónoma. El sistema mide cuatro parámetros críticos del agua (pH, temperatura, TDS y turbidez) y transmite los datos a un servidor web para su visualización y análisis.

### ¿Qué mide el sistema?

- **pH**: Nivel de acidez/alcalinidad del agua (0-14)
- **Temperatura**: Temperatura del agua en °C
- **TDS (Total Dissolved Solids)**: Concentración de sólidos disueltos en ppm
- **Turbidez**: Claridad del agua en NTU

---

## 📁 Estructura del Repositorio

El repositorio está organizado por etapas de desarrollo y componentes del sistema:

```
WaterQualityMonitoring/
│
├── Etapa I/                        # Trabajo previo del proyecto
│   ├── diseños_pcb/               # Diseños de PCB originales
│   └── documentacion.pdf          # Documentación de la etapa inicial
│
├── Etapa II/                       # Desarrollo más reciente 
│   ├── Manual de Usuario.md       # Guía completa para el usuario
│   ├── Pruebas y Validación.pdf   # Documento de pruebas realizadas
│   └── registro sistema de monitoreo.rar  # Fotos y fuente LaTeX
│
├── MonitorAgua_ESP32/             # Código del firmware (ESP32)
│   ├── src/                       # Código fuente principal
│   ├── include/                   # Archivos de cabecera
│   ├── lib/                       # Librerías del proyecto
│   ├── platformio.ini            # Configuración de PlatformIO
│   ├── Doxyfile                  # Configuración para Doxygen
│   └── README.md                 # Documentación del firmware
│
├── monitor_agua_pagina/           # Código del servidor web
│   ├── src/                      # Código fuente del servidor
│   ├── public/                   # Archivos estáticos (HTML, CSS, JS)
│   ├── package.json             # Dependencias Node.js
│   └── README.md                # Documentación del servidor
│
├── LICENSE                       # Licencia del proyecto
└── README.md                     # Este archivo
```

---

## 📂 Descripción de Carpetas

### Etapa I - Trabajo Previo

La carpeta **`Etapa I/`** contiene el trabajo realizado por los desarrolladores anteriores del proyecto:

#### 📐 Diseños de PCB
- **Ubicación**: `Etapa I/diseños_pcb/`
- **Contenido**: Archivos de diseño de la placa de circuito impreso (PCB) utilizados en las versiones iniciales del proyecto
- **Formato**: Archivos de diseño electrónico (KiCad, Eagle, Gerber, etc.)
- **Propósito**: Referencia para entender el hardware original y base para mejoras futuras

#### 📄 Documentación Inicial
- **Ubicación**: `Etapa I/documentacion.pdf`
- **Contenido**: Documentación técnica del proyecto en su fase inicial
- **Incluye**: 
  - Especificaciones originales del sistema
  - Diagramas de conexión
  - Resultados de pruebas preliminares
  - Decisiones de diseño tomadas
- **Fecha**: Entregada por el equipo anterior

---

### Etapa II - Desarrollo Actual

La carpeta **`Etapa II/`** contiene el trabajo realizado durante el semestre actual:

#### 📖 Manual de Usuario
- **Archivo**: `Etapa II/Manual de Usuario.md`
- **Descripción**: Guía completa y detallada para usuarios finales del sistema
- **Contenido**:
  - Instalación del entorno de desarrollo
  - Configuración del hardware
  - Conexión de sensores
  - Uso del sistema
  - Solución de problemas comunes
  - Referencias y recursos adicionales
- **Audiencia**: Usuarios finales, técnicos, estudiantes

#### 🧪 Documento de Pruebas y Validación
- **Archivo**: `Etapa II/Pruebas y Validación.pdf`
- **Descripción**: Documentación de todas las pruebas realizadas al sistema
- **Contenido**:
  - Metodología de pruebas
  - Casos de prueba ejecutados
  - Resultados obtenidos
  - Análisis de desempeño
  - Validación de funcionalidades
  - Conclusiones y recomendaciones
- **Propósito**: Validar el correcto funcionamiento del sistema y documentar su rendimiento

#### 📸 Registro del Sistema (LaTeX)
- **Archivo**: `Etapa II/registro sistema de monitoreo.rar`
- **Contenido**:
  - **Fotografías**: Imágenes del sistema ensamblado, sensores, conexiones, pruebas en campo
  - **Fuente LaTeX**: Archivos `.tex` del documento de registro
  - **Recursos**: Figuras, tablas y archivos auxiliares para compilar el documento
- **Propósito**: Documentación visual del proyecto y fuente editable para reportes académicos
- **Uso**: Descomprimir el archivo RAR y compilar el documento LaTeX para visualizar el registro completo

---

### Código del Firmware (MonitorAgua_ESP32)

La carpeta **`MonitorAgua_ESP32/`** contiene todo el código que se ejecuta en la placa ESP32.

#### Estructura del Firmware

```
MonitorAgua_ESP32/
│
├── src/                           # Código fuente principal
│   ├── main.cpp                  # Punto de entrada del programa
│   ├── sensors.cpp               # Gestión de sensores (pH, temp, TDS, turbidez)
│   ├── wifi_manager.cpp          # Manejo de conectividad WiFi
│   ├── rtc_handler.cpp           # Control del reloj de tiempo real (RTC)
│   ├── storage.cpp               # Almacenamiento local de datos
│   ├── power_management.cpp      # Deep Sleep y gestión de energía
│   └── watchdog.cpp              # Implementación del Watchdog Timer
│
├── include/                       # Archivos de cabecera (.h)
│   ├── sensors.h                 # Definiciones de sensores
│   ├── wifi_manager.h            # Definiciones de WiFi
│   ├── config.h                  # Configuración general del sistema
│   └── constants.h               # Constantes del proyecto
│
├── lib/                          # Librerías personalizadas del proyecto
│   └── [librerías específicas]
│
├── test/                         # Pruebas unitarias (si existen)
│
├── platformio.ini                # Configuración de PlatformIO
│   # Define: placa, velocidad, librerías, flags de compilación
│
├── Doxyfile                      # Configuración de Doxygen
│   # Define cómo generar la documentación del código
│
└── README.md                     # Documentación específica del firmware
```

#### Funcionalidades Principales del Firmware

- **Adquisición de datos**: Lee valores de los cuatro sensores de forma periódica
- **Almacenamiento local**: Guarda mediciones en memoria cuando no hay conectividad
- **Conectividad WiFi**: Se conecta a red configurada para transmisión de datos
- **RTC**: Mantiene estampas de tiempo precisas con batería de respaldo
- **Deep Sleep**: Reduce consumo de energía entre mediciones
- **Watchdog Timer**: Reinicia automáticamente el sistema ante fallos
- **Protocolo OneWire**: Comunicación con sensor de temperatura DS18B20

#### Tecnologías Utilizadas

- **Framework**: Arduino para ESP32
- **Plataforma**: PlatformIO
- **Librerías principales**:
  - OneWire: Comunicación con DS18B20
  - DallasTemperature: Lectura de temperatura
  - RTClib: Manejo del RTC DS3231
  - WiFi.h: Conectividad WiFi
  - Preferences: Almacenamiento persistente

---

### Código del Servidor (monitor_agua_pagina)

La carpeta **`monitor_agua_pagina/`** contiene el servidor web que recibe y visualiza los datos del sistema.

#### Estructura del Servidor

```
monitor_agua_pagina/
│
├── src/                          # Código fuente del servidor
│   ├── app.js                   # Aplicación principal de Express
│   ├── routes/                  # Definición de rutas de la API
│   │   ├── data.js             # Endpoints para datos de sensores
│   │   └── device.js           # Endpoints para gestión de dispositivos
│   ├── controllers/             # Lógica de negocio
│   │   ├── dataController.js   # Procesamiento de datos
│   │   └── deviceController.js # Gestión de dispositivos
│   ├── middleware/              # Middleware de Express
│   │   ├── auth.js             # Autenticación (si aplica)
│   │   └── validation.js       # Validación de datos
│   └── utils/                   # Utilidades
│       └── logger.js            # Sistema de logs
│
├── public/                       # Archivos estáticos (interfaz web)
│   ├── index.html               # Página principal
│   ├── css/                     # Estilos CSS
│   │   └── styles.css
│   └── js/                      # JavaScript del cliente
│       ├── main.js              # Lógica principal de la interfaz
│       ├── charts.js            # Generación de gráficos
│       └── api.js               # Llamadas a la API
│
├── data/                         # Almacenamiento de datos (si aplica)
│   └── measurements.json        # Datos guardados
│
├── config/                       # Configuración del servidor
│   └── server.config.js         # Parámetros de configuración
│
├── package.json                  # Dependencias y scripts de Node.js
├── package-lock.json             # Versiones exactas de dependencias
├── .env.example                  # Ejemplo de variables de entorno
└── README.md                     # Documentación del servidor
```

#### Funcionalidades del Servidor

- **API RESTful**: Endpoints para recibir datos de la ESP32
- **Interfaz web**: Visualización de datos en tiempo real
- **Gráficos**: Representación visual de tendencias de mediciones
- **Almacenamiento**: Guarda historial de datos (opcional)
- **CORS**: Permite peticiones desde diferentes orígenes
- **Validación**: Verifica integridad de datos recibidos

#### Tecnologías Utilizadas

- **Runtime**: Node.js
- **Framework web**: Express.js
- **Gestión de CORS**: Librería `cors`
- **Procesamiento de JSON**: `body-parser`
- **Librerías principales**:
  - express: Servidor web
  - cors: Políticas de origen cruzado
  - body-parser: Parsing de datos JSON
  - dotenv: Variables de entorno (opcional)

#### Endpoints Principales (Ejemplo)

```
GET  /api/data          # Obtener últimos datos almacenados
POST /api/data          # Recibir nuevos datos de la ESP32
GET  /api/device/status # Consultar estado del dispositivo
GET  /                  # Servir interfaz web
```

---

## 📖 Documentación del Código

### Documentación Doxygen del Firmware

El código del firmware en la carpeta **`MonitorAgua_ESP32/`** está completamente documentado utilizando el estándar **Doxygen**. Esto permite generar documentación HTML profesional de forma automática.

#### ¿Qué es Doxygen?

Doxygen es una herramienta que extrae comentarios especialmente formateados del código fuente y genera documentación en varios formatos (HTML, PDF, LaTeX). Es ampliamente utilizado en proyectos de software para mantener la documentación sincronizada con el código.

#### 📚 Generación de la Documentación

##### Prerrequisitos

Antes de generar la documentación, asegúrese de tener Doxygen instalado:

**Windows:**
```bash
# Descargar desde: https://www.doxygen.nl/download.html
# Ejecutar el instalador
```

**macOS:**
```bash
brew install doxygen graphviz
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install doxygen graphviz
```

> **Nota**: Graphviz es opcional pero recomendado para generar diagramas de clases y dependencias.

##### Pasos para Generar la Documentación

1. **Abrir una terminal** en la carpeta raíz del firmware:
   ```bash
   cd MonitorAgua_ESP32/
   ```

2. **Ejecutar el comando de Doxygen**:
   ```bash
   doxygen Doxyfile
   ```

3. **Esperar** a que el proceso termine. Verá mensajes indicando el progreso de la generación.

4. **Acceder a la documentación generada**:
   - Una vez completado el proceso, se creará una carpeta llamada **`html/`** dentro del proyecto
   - Navegue a la carpeta: `MonitorAgua_ESP32/html/`
   - Abra el archivo **`index.html`** en su navegador web

   ```bash
   # Windows
   start html/index.html
   
   # macOS
   open html/index.html
   
   # Linux
   xdg-open html/index.html
   ```

##### Contenido de la Documentación

La documentación generada incluye:

- **Índice de archivos**: Listado de todos los archivos del proyecto con su descripción
- **Índice de clases**: Todas las clases definidas con sus métodos y atributos
- **Índice de funciones**: Listado alfabético de todas las funciones
- **Diagramas de clases**: Visualización de relaciones entre clases (requiere Graphviz)
- **Diagramas de dependencias**: Muestra qué archivos dependen de otros
- **Gráficos de llamadas**: Ilustra qué funciones llaman a otras funciones
- **Documentación detallada**: Descripción completa de cada función, parámetros, valores de retorno y ejemplos

##### Ejemplo de Navegación

1. Abra `index.html`
2. En el menú superior, haga clic en "Files" para ver todos los archivos del proyecto
3. Haga clic en "Classes" para ver las clases definidas
4. Haga clic en "Functions" para buscar funciones específicas
5. Use la barra de búsqueda para encontrar elementos específicos

#### 🔍 Formato de Comentarios Doxygen

El código fuente utiliza el siguiente formato de comentarios para que Doxygen pueda generar la documentación:

```cpp
/**
 * @file sensors.cpp
 * @brief Implementación del sistema de gestión de sensores
 * @author [Nombre del Autor]
 * @date 2024
 * @version 1.0
 */

/**
 * @brief Lee el valor de pH del sensor conectado
 * 
 * Esta función lee el valor analógico del pin especificado,
 * lo convierte a un valor de pH usando la fórmula de calibración
 * y aplica un filtro promedio móvil para reducir ruido.
 * 
 * @param pin Pin analógico donde está conectado el sensor de pH
 * @return float Valor de pH medido (rango 0-14)
 * 
 * @note El sensor debe estar calibrado previamente para obtener
 *       valores precisos. Ver calibration_guide.md
 * @warning No usar con voltajes superiores a 3.3V
 * 
 * @see calibratePHSensor()
 * @see applyMovingAverage()
 */
float readPH(int pin) {
    // Implementación de la función
    int rawValue = analogRead(pin);
    float voltage = rawValue * (3.3 / 4095.0);
    float pH = convertVoltageToPH(voltage);
    return applyMovingAverage(pH);
}
```

---

## 🚀 Inicio Rápido

### 1. Clonar el Repositorio

```bash
git clone https://github.com/dacostaca/WaterQualityMonitoring.git
cd WaterQualityMonitoring
```

### 2. Configurar el Firmware (ESP32)

```bash
cd MonitorAgua_ESP32
# Abrir con PlatformIO en Visual Studio Code
# Compilar y flashear a la ESP32
```

### 3. Configurar el Servidor

```bash
cd monitor_agua_pagina
npm install
npm start
```

### 4. Acceder a la Interfaz Web

Abrir en el navegador: `http://localhost:3000`

---

## 🔧 Requisitos

### Hardware

- ESP32 DevKit V1 o compatible
- Sensores DFRobot: pH, Temperatura (DS18B20), TDS, Turbidez
- Módulo RTC DS3231 con batería CR2032 (3.3V)
- Cable USB Tipo C
- Fuente de alimentación 5V

### Software

- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO IDE](https://platformio.org/)
- [Node.js](https://nodejs.org/) v14+
- [CP210x Drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- [Doxygen](https://www.doxygen.nl/download.html) (opcional, para documentación)

---

## 📄 Documentación Adicional

- **Manual de Usuario**: `Etapa II/Manual de Usuario.md` - Guía completa de uso del sistema
- **Pruebas y Validación**: `Etapa II/Pruebas y Validación.pdf` - Resultados de pruebas realizadas
- **Registro LaTeX**: `Etapa II/registro sistema de monitoreo.rar` - Documentación visual y fuente LaTeX
- **Documentación Etapa I**: `Etapa I/documentacion.pdf` - Trabajo previo del proyecto

---

## 🤝 Contribuir

Las contribuciones son bienvenidas. Por favor:

1. Fork el proyecto
2. Cree una rama para su función (`git checkout -b feature/NuevaFuncion`)
3. Documente el código con comentarios Doxygen
4. Commit sus cambios (`git commit -m 'Agregar nueva función'`)
5. Push a la rama (`git push origin feature/NuevaFuncion`)
6. Abra un Pull Request

---

## 📄 Licencia

Este proyecto está bajo la Licencia MIT. Ver el archivo [LICENSE](LICENSE) para más detalles.

---

## 👥 Autores

- **Equipo Etapa I**: Desarrollo inicial del hardware y firmware base
- **Equipo Etapa II**: Mejoras, documentación y validación del sistema
- **[dacostaca](https://github.com/dacostaca)**: Desarrollo actual

Ver la lista completa de [contribuyentes](https://github.com/dacostaca/WaterQualityMonitoring/contributors).

---

## 📞 Soporte

- **Issues**: [GitHub Issues](https://github.com/dacostaca/WaterQualityMonitoring/issues)
- **Manual de Usuario**: `Etapa II/Manual de Usuario.md`
- **Documentación del Código**: Generar con Doxygen (ver sección anterior)

---

**⭐ Si este proyecto te fue útil, considera darle una estrella en GitHub ⭐**

---

*Última actualización: Diciembre 2024*
