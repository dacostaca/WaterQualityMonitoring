# Water Quality Monitoring System 🌊

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-compatible-blue.svg)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/ESP32-compatible-green.svg)](https://www.espressif.com/en/products/socs/esp32)

Un sistema autónomo de monitoreo de calidad del agua basado en ESP32 con cuatro sensores: pH, temperatura, TDS y turbidez. Incluye implementación de Deep Sleep y Watchdog Timer para operación autónoma y monitoreo de errores.

---

## 📋 Tabla de Contenidos

- [Descripción General](#-descripción-general)
- [Estado del Proyecto](#-estado-del-proyecto)
- [Estructura del Repositorio](#-estructura-del-repositorio)
  - [Etapa I - Trabajo Previo](#etapa-i---trabajo-previo)
  - [Etapa II - Desarrollo Actual](#etapa-ii---desarrollo-actual)
  - [Código del Firmware (MonitorAgua_ESP32)](#código-del-firmware-monitoragua_esp32)
  - [Código del Servidor (monitor_agua_pagina)](#código-del-servidor-monitor_agua_pagina)
- [Documentación del Código](#-documentación-del-código)
- [Inicio Rápido](#-inicio-rápido)
- [Requisitos](#-requisitos)
- [Colaboradores](#-colaboradores)
- [Licencia](#-licencia)

---

## 🌟 Descripción General

Este proyecto implementa un sistema de monitoreo de calidad del agua diseñado para operar de forma autónoma. El sistema mide cuatro parámetros críticos del agua (pH, temperatura, TDS y turbidez) y transmite los datos a un servidor web para su visualización y análisis.

### ¿Qué mide el sistema?

- **pH**: Nivel de acidez/alcalinidad del agua (0-14)
- **Temperatura**: Temperatura del agua en °C usando sensor DS18B20
- **TDS (Total Dissolved Solids)**: Concentración de sólidos disueltos en ppm
- **Turbidez**: Claridad del agua en NTU

### Aplicaciones

- Monitoreo ambiental de cuerpos de agua naturales
- Control de calidad en plantas de tratamiento
- Investigación científica y educativa
- Acuicultura y piscicultura
- Sistemas de agua potable

---

## 📌 Estado del Proyecto

**Estado actual**: El desarrollo del proyecto ha sido completado en su Etapa II. El sistema se encuentra funcional y documentado, listo para su uso o para ser retomado por futuros desarrolladores que deseen realizar mejoras o extensiones.

**Última actualización**: 09 de diciembre de 2025 (Etapa II)

---

## 📁 Estructura del Repositorio

```
WaterQualityMonitoring/
│
├── Etapa I/                        # Trabajo previo del proyecto (01-08-2025)
│   ├── diseños_pcb/               # Diseños de PCB originales
│   └── documentacion.pdf          # Documentación de la etapa inicial
│
├── Etapa II/                       # Desarrollo actual (09-12-2025)
│   ├── Manual de Usuario.md       # Guía completa para el usuario
│   ├── Pruebas y Validación.pdf   # Documento de pruebas realizadas
│   └── registro sistema de monitoreo.rar  # Fotos y fuente LaTeX
│
├── MonitorAgua_ESP32/             # Código del firmware (ESP32)
│   ├── .vscode/                   # Configuración de Visual Studio Code
│   ├── include/                   # Archivos de cabecera (placeholder)
│   ├── lib/                       # Librerías del proyecto
│   ├── src/                       # Código fuente principal
│   ├── test/                      # Directorio de tests (placeholder)
│   ├── .gitignore                # Archivos ignorados por Git
│   ├── Doxyfile                  # Configuración para Doxygen
│   └── platformio.ini            # Configuración de PlatformIO
│
├── monitor_agua_pagina/           # Código del servidor web
│   ├── web_interface/            # Interfaz web del sistema
│   └── servidor.py               # Servidor Python
│
└── README.md                     # Este archivo
```

---

## 📂 Descripción de Carpetas

### Etapa I - Trabajo Previo

**Fecha de entrega**: 01 de agosto de 2025

La carpeta **`Etapa I/`** contiene el trabajo realizado por los desarrolladores iniciales del proyecto:

#### 📐 Diseños de PCB
- **Ubicación**: `Etapa I/diseños_pcb/`
- **Contenido**: Archivos de diseño de la placa de circuito impreso (PCB) utilizados en las versiones iniciales del proyecto
- **Propósito**: Referencia para entender el hardware original y base para mejoras futuras

#### 📄 Documentación Inicial
- **Ubicación**: `Etapa I/documentacion.pdf`
- **Contenido**: Documentación técnica del proyecto en su fase inicial
- **Incluye**: 
  - Especificaciones originales del sistema
  - Diagramas de conexión
  - Resultados de pruebas preliminares
  - Decisiones de diseño tomadas

**Colaboradores Etapa I:**
- Maria Alejandra González Duque
- Juan Carlos Delgado Figueroa

---

### Etapa II - Desarrollo Actual

**Fecha de entrega**: 09 de diciembre de 2025

La carpeta **`Etapa II/`** contiene el trabajo realizado durante el segundo semestre de 2025:

#### 📖 Manual de Usuario
- **Archivo**: `Etapa II/Manual de Usuario.md`
- **Descripción**: Guía completa y detallada para usuarios finales del sistema
- **Contenido**:
  - Instalación del entorno de desarrollo (Visual Studio Code y PlatformIO)
  - Configuración del hardware y drivers
  - Conexión física de sensores a los pines GPIO
  - Flasheo del programa a la ESP32
  - Configuración de red WiFi
  - Uso del sistema y visualización de datos
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
  - Análisis de desempeño del sistema
  - Validación de funcionalidades
  - Conclusiones y recomendaciones
- **Propósito**: Validar el correcto funcionamiento del sistema y documentar su rendimiento

#### 📸 Registro del Sistema (LaTeX)
- **Archivo**: `Etapa II/registro sistema de monitoreo.rar`
- **Contenido**:
  - **Fotografías**: Imágenes del sistema ensamblado, sensores conectados, pruebas en campo
  - **Fuente LaTeX**: Archivos `.tex` del documento de registro
  - **Recursos**: Figuras, tablas y archivos auxiliares para compilar el documento
- **Propósito**: Documentación visual del proyecto y fuente editable para reportes académicos
- **Uso**: Descomprimir el archivo RAR y compilar el documento LaTeX para visualizar el registro completo del sistema

**Colaboradores Etapa II:**
- Daniel Felipe Acosta Castro
- Oscar Santiago Erazo Mora

*Estudiantes de Ingeniería Electrónica, Universidad Nacional de Colombia*

---

### Código del Firmware (MonitorAgua_ESP32)

La carpeta **`MonitorAgua_ESP32/`** contiene todo el código que se ejecuta en la placa ESP32.

#### Estructura del Firmware

```
MonitorAgua_ESP32/
│
├── .vscode/                       # Configuración del entorno VSCode
│   └── [archivos de configuración]
│
├── include/                       # Directorio para archivos de cabecera
│   └── README                    # Archivo placeholder
│
├── lib/                          # Librerías personalizadas del proyecto
│   ├── CalibrationManager/      # Gestión de calibración de sensores
│   │   ├── CalibrationManager.cpp
│   │   └── CalibrationManager.h
│   │
│   ├── DeepSleep/               # Implementación de modo Deep Sleep
│   │   ├── DeepSleep.cpp
│   │   └── DeepSleep.h
│   │
│   ├── RTC/                     # Control del reloj de tiempo real (RTC)
│   │   ├── RTC.cpp
│   │   └── RTC.h
│   │
│   ├── RTCMemory/               # Gestión de memoria del RTC
│   │   ├── RTCMemory.cpp
│   │   └── RTCMemory.h
│   │
│   ├── Sensors/                 # Controladores de sensores
│   │   ├── pH.cpp              # Sensor de pH
│   │   ├── pH.h
│   │   ├── Temperature.cpp     # Sensor de temperatura DS18B20
│   │   ├── Temperature.h
│   │   ├── TDS.cpp             # Sensor TDS
│   │   ├── TDS.h
│   │   ├── Turbidity.cpp       # Sensor de turbidez
│   │   └── Turbidity.h
│   │
│   ├── WatchDog/                # Implementación del Watchdog Timer
│   │   ├── WatchDog.cpp
│   │   └── WatchDog.h
│   │
│   └── WifiManager/             # Gestión de conectividad WiFi
│       ├── WifiManager.cpp
│       └── WifiManager.h
│
├── src/                          # Código fuente principal
│   └── main.cpp                 # Punto de entrada del programa
│
├── test/                         # Directorio para tests unitarios
│   └── README                   # Archivo placeholder
│
├── .gitignore                    # Archivos ignorados por Git
├── Doxyfile                      # Configuración de Doxygen
└── platformio.ini                # Configuración de PlatformIO
```

#### Descripción de Componentes del Firmware

##### Librerías Principales

- **CalibrationManager**: Gestiona los valores de calibración de los sensores, permitiendo ajustar las lecturas para mayor precisión
- **DeepSleep**: Implementa el modo de bajo consumo de energía, haciendo que la ESP32 entre en Deep Sleep entre mediciones para optimizar el uso de batería
- **RTC**: Maneja la comunicación con el módulo de reloj de tiempo real (RTC DS3231) para mantener estampas de tiempo precisas
- **RTCMemory**: Gestiona el almacenamiento de datos en la memoria del RTC que persiste durante los ciclos de Deep Sleep
- **Sensors**: Contiene los controladores para cada uno de los cuatro sensores:
  - **pH**: Sensor analógico de pH que mide acidez/alcalinidad del agua
  - **Temperature**: Sensor digital DS18B20 que usa protocolo OneWire
  - **TDS**: Sensor analógico de sólidos totales disueltos
  - **Turbidity**: Sensor analógico de turbidez que mide claridad del agua
- **WatchDog**: Implementa el temporizador Watchdog para reiniciar automáticamente el sistema en caso de fallos o bloqueos
- **WifiManager**: Gestiona la conexión WiFi, reconexión automática y comunicación con el servidor

##### Código Principal

- **main.cpp**: Programa principal que orquesta todas las funcionalidades:
  - Inicialización de sensores y periféricos
  - Bucle principal de medición
  - Almacenamiento local de datos
  - Gestión de ciclos de Deep Sleep
  - Conexión periódica a WiFi para transmisión de datos

#### Archivos de Configuración

- **platformio.ini**: Define la configuración del proyecto para PlatformIO, incluyendo:
  - Placa objetivo (ESP32)
  - Framework (Arduino)
  - Librerías externas requeridas
  - Velocidad de baudios para comunicación serial
  - Flags de compilación

- **Doxyfile**: Configuración para generar documentación automática del código usando Doxygen

#### Funcionalidades Principales del Firmware

- ⚡ **Adquisición de datos**: Lee valores de los cuatro sensores de forma periódica
- 💾 **Almacenamiento local**: Guarda mediciones en memoria del RTC cuando no hay conectividad
- 🌐 **Conectividad WiFi**: Se conecta a red configurada para transmisión de datos al servidor
- ⏰ **RTC externo**: Mantiene estampas de tiempo precisas con batería de respaldo
- 🔋 **Deep Sleep**: Reduce consumo de energía entrando en modo de bajo consumo entre mediciones
- 🛡️ **Watchdog Timer**: Reinicia automáticamente el sistema ante fallos o bloqueos
- 📊 **Calibración**: Sistema de calibración para ajustar lecturas de sensores
- 📡 **Protocolo OneWire**: Comunicación con sensor de temperatura DS18B20

---

### Código del Servidor (monitor_agua_pagina)

La carpeta **`monitor_agua_pagina/`** contiene el servidor web que recibe y visualiza los datos del sistema.

#### Estructura del Servidor

```
monitor_agua_pagina/
│
├── web_interface/                # Interfaz web del sistema
│   ├── css/                     # Hojas de estilo
│   │   └── styles.css          # Estilos de la interfaz
│   │
│   ├── js/                      # JavaScript del cliente
│   │   └── script.js           # Lógica de la interfaz web
│   │
│   └── index.html               # Página principal
│
└── servidor.py                   # Servidor Python
```

#### Descripción de Componentes del Servidor

##### Servidor Backend

- **servidor.py**: Servidor web implementado en Python que maneja:
  - Recepción de datos enviados por la ESP32
  - Procesamiento y validación de mediciones
  - Almacenamiento temporal de datos
  - Servicio de la interfaz web
  - API para consulta de datos históricos

##### Interfaz Web (web_interface/)

- **index.html**: Página principal de la aplicación web que proporciona:
  - Visualización de datos en tiempo real
  - Gráficos de tendencias de mediciones
  - Tabla con historial de datos (últimas 120 mediciones por defecto)
  - Botón para solicitar datos almacenados en la ESP32
  - Indicadores de estado de conexión

- **styles.css**: Hoja de estilos que define:
  - Diseño y apariencia de la interfaz
  - Estilos para gráficos y tablas
  - Diseño responsivo
  - Indicadores visuales de estado

- **script.js**: JavaScript del lado del cliente que maneja:
  - Solicitudes AJAX al servidor
  - Actualización dinámica de la interfaz
  - Generación de gráficos interactivos
  - Procesamiento y visualización de datos recibidos
  - Gestión de eventos de usuario (botones, filtros, etc.)

#### Funcionalidades del Servidor

- 🌐 **API REST**: Endpoints para recibir datos de la ESP32 vía HTTP
- 📊 **Visualización**: Interfaz web para mostrar datos en tiempo real y tendencias
- 💾 **Almacenamiento temporal**: Mantiene datos recientes en memoria para consulta
- 🔄 **Actualización dinámica**: Refresca automáticamente la interfaz con nuevos datos
- 📈 **Gráficos**: Generación de gráficos de series de tiempo para cada sensor
- 🎨 **Interfaz responsiva**: Diseño adaptable a diferentes tamaños de pantalla

#### Tecnologías Utilizadas

- **Backend**: Python (servidor web)
- **Frontend**: HTML5, CSS3, JavaScript
- **Comunicación**: HTTP/REST API
- **Visualización**: JavaScript (librerías de gráficos integradas en script.js)

---

## 📖 Documentación del Código

### Documentación Doxygen del Firmware

El código del firmware en la carpeta **`MonitorAgua_ESP32/`** está completamente documentado utilizando el estándar **Doxygen**. Esto permite generar documentación HTML profesional de forma automática a partir de los comentarios en el código.

#### ¿Qué es Doxygen?

Doxygen es una herramienta que extrae comentarios especialmente formateados del código fuente y genera documentación en varios formatos (HTML, PDF, LaTeX). Es ampliamente utilizado en proyectos de software para mantener la documentación sincronizada con el código.

#### 📚 Generación de la Documentación

##### Prerrequisitos

Antes de generar la documentación, asegúrese de tener Doxygen instalado:

**Windows:**
1. Descargue el instalador desde: https://www.doxygen.nl/download.html
2. Ejecute el instalador y siga las instrucciones

##### Pasos para Generar la Documentación

1. **Abrir una terminal** (Command Prompt o PowerShell) en la carpeta raíz del firmware:
   ```
   cd MonitorAgua_ESP32
   ```

2. **Ejecutar el comando de Doxygen**:
   ```
   doxygen Doxyfile
   ```

3. **Esperar** a que el proceso termine. Verá mensajes en la terminal indicando el progreso de la generación.

4. **Acceder a la documentación generada**:
   - Una vez completado el proceso, se creará una carpeta llamada **`html/`** dentro del proyecto `MonitorAgua_ESP32/`
   - Navegue a la carpeta: `MonitorAgua_ESP32/html/`
   - Abra el archivo **`index.html`** en su navegador web preferido (doble clic sobre el archivo)

##### Contenido de la Documentación

La documentación generada incluye:

- **Índice de archivos**: Listado de todos los archivos del proyecto con su descripción
- **Índice de clases**: Todas las clases definidas con sus métodos y atributos
- **Índice de funciones**: Listado alfabético de todas las funciones del proyecto
- **Diagramas de clases**: Visualización de relaciones entre clases (requiere Graphviz)
- **Diagramas de dependencias**: Muestra qué archivos dependen de otros
- **Gráficos de llamadas**: Ilustra qué funciones llaman a otras funciones
- **Documentación detallada**: Descripción completa de cada función con:
  - Propósito y funcionalidad
  - Parámetros de entrada
  - Valores de retorno
  - Notas y advertencias
  - Ejemplos de uso (cuando aplica)

##### Navegación de la Documentación

Una vez abierto `index.html`:

1. Use el menú superior para navegar entre secciones:
   - **Files**: Ver todos los archivos del proyecto
   - **Classes**: Ver las clases definidas
   - **Functions**: Buscar funciones específicas

2. Use la barra de búsqueda en la esquina superior derecha para encontrar elementos específicos

3. Haga clic en cualquier elemento para ver su documentación detallada

---

## 🚀 Inicio Rápido

### 1. Clonar el Repositorio

```bash
git clone https://github.com/dacostaca/WaterQualityMonitoring.git
cd WaterQualityMonitoring
```

### 2. Configurar el Firmware (ESP32)

1. Abra Visual Studio Code
2. Instale la extensión PlatformIO IDE
3. Abra la carpeta `MonitorAgua_ESP32` con PlatformIO
4. Conecte la ESP32 al computador mediante USB
5. Compile y flashee el programa usando el botón "Upload" (→) en la barra inferior de PlatformIO

**Nota importante**: El puerto USB Tipo C de la ESP32 solo funciona en **una orientación**. Si el computador no detecta la placa, desconecte el cable USB, voltéelo e intente conectar nuevamente.

### 3. Configurar el Servidor

1. Navegue a la carpeta del servidor:
   ```bash
   cd monitor_agua_pagina
   ```

2. Asegúrese de tener Python instalado en su sistema

3. Ejecute el servidor:
   ```bash
   python servidor.py
   ```

### 4. Acceder a la Interfaz Web

1. Abra su navegador web
2. Vaya a la dirección proporcionada por el servidor (típicamente `http://localhost:5000` o similar)
3. Espere a que la ESP32 se conecte a la red WiFi configurada
4. Use el botón "Solicitar Datos" para descargar las mediciones almacenadas en la ESP32

### 5. Configurar Red WiFi

El sistema está configurado por defecto para conectarse a:
- **SSID**: `RED_MONITOREO`
- **Contraseña**: `Holamundo6`

Para cambiar estas credenciales, edite el archivo de configuración en el firmware y vuelva a flashear el programa.

---

## 🔧 Requisitos

### Hardware

- **Placa**: ESP32 S2 o compatible
- **Sensores DFRobot**:
  - Sensor de pH (analógico)
  - Sensor de temperatura DS18B20 (digital, protocolo OneWire)
  - Sensor TDS (analógico)
  - Sensor de turbidez (analógico)
- **Módulo RTC**: DS3231 con batería CR2032 de 3.3V
- **Cable**: USB Tipo C
- **Alimentación**: 5V (USB o fuente externa)
- **Computador**: Windows 10/11 con puerto USB disponible

### Software

- **[Visual Studio Code](https://code.visualstudio.com/)** - Editor de código
- **[PlatformIO IDE](https://platformio.org/)** - Extensión para VSCode
- **[Python](https://www.python.org/downloads/)** - Para ejecutar el servidor (versión 3.7 o superior)
- **[CP210x USB to UART Bridge Drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)** - Drivers para comunicación con ESP32
- **[Doxygen](https://www.doxygen.nl/download.html)** - (Opcional) Para generar documentación del código
- **[Graphviz](https://graphviz.org/download/)** - (Opcional) Para diagramas en Doxygen

### Conocimientos Recomendados

- Manejo básico de Visual Studio Code
- Conceptos básicos de electrónica (voltaje, corriente, conexión de sensores)
- Navegación en interfaces web
- Conocimientos básicos de línea de comandos (para ejecutar el servidor)

---

## 📄 Documentación Adicional

Para información detallada sobre el uso del sistema, consulte:

- **[Manual de Usuario](Etapa%20II/Manual%20de%20Usuario.md)**: Guía completa de instalación, configuración y uso
- **[Pruebas y Validación](Etapa%20II/Pruebas%20y%20Validación.pdf)**: Resultados de las pruebas realizadas al sistema
- **[Registro LaTeX](Etapa%20II/registro%20sistema%20de%20monitoreo.rar)**: Documentación visual y fuente LaTeX (descomprimir para acceder)
- **[Documentación Etapa I](Etapa%20I/documentacion.pdf)**: Trabajo previo del proyecto
- **Documentación del código**: Generar con Doxygen siguiendo las instrucciones de la sección anterior

---

## 👥 Colaboradores

### Etapa I (01 de agosto de 2025)
- **Maria Alejandra González Duque**
- **Juan Carlos Delgado Figueroa**

*Responsables del diseño inicial de hardware y primera implementación del sistema*

### Etapa II (09 de diciembre de 2025)
- **Daniel Felipe Acosta Castro** - [GitHub](https://github.com/dacostaca)
- **Oscar Santiago Erazo Mora**

*Estudiantes de Ingeniería Electrónica*  
*Universidad Nacional de Colombia*  
*Segundo semestre de 2025*

---

## 🙏 Agradecimientos

- DFRobot por la documentación de los sensores utilizados
- Comunidad de PlatformIO por las herramientas de desarrollo
- Espressif por el soporte de ESP32
- Universidad Nacional de Colombia por el apoyo académico

---

**⭐ Si este proyecto te fue útil, considera darle una estrella en GitHub ⭐**

---

*Proyecto desarrollado como parte del programa de Ingeniería Electrónica*  
*Universidad Nacional de Colombia - 2025*