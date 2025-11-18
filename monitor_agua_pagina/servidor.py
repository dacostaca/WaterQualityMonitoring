#!/usr/bin/env python3
"""
Servidor Backend para Monitor de Calidad del Agua ESP32
"""

import asyncio
import websockets
import json
import datetime as dt  
import socket
import csv
import os
from pathlib import Path
from http.server import HTTPServer, SimpleHTTPRequestHandler
import threading
import webbrowser
from collections import deque

# Configuración
WEBSOCKET_PORT = 8765
HTTP_PORT = 8080
CSV_FILENAME = "datos_calidad_agua.csv"

SCRIPT_DIR = Path(__file__).parent.absolute()  # Directorio donde está servidor.py
WEB_DIR = SCRIPT_DIR / "web_interface"  # web_interface junto a servidor.py

class ServidorMonitorAgua:
    def __init__(self):
        self.server_ip = self.obtener_ip()
        self.datos_recibidos = deque(maxlen=1000)
        self.total_mensajes = 0
        self.conexiones_web = set()
        self.conexion_esp32 = None
        self.esperando_datos = False
        self.datos_solicitados = False
        self.ultima_lectura = None
        
        self.session_data = []  # Datos de la sesión actual
        self.session_start_time = None
        self.sessions_file = WEB_DIR / "sessions_history.json"
        self.load_sessions_history()

        self.verificar_archivos_web()
        self.inicializar_csv()

    def load_sessions_history(self):
        """Cargar historial de sesiones desde archivo"""
        try:
            if self.sessions_file.exists():
                with open(self.sessions_file, 'r', encoding='utf-8') as f:
                    self.sessions_history = json.load(f)
            else:
                self.sessions_history = []
            print(f" Cargadas {len(self.sessions_history)} sesiones del historial")
        except Exception as e:
            print(f" Error cargando historial: {e}")
            self.sessions_history = []
    
    def save_session_to_history(self):
        """Guardar sesión actual al historial"""
        print(f"  INICIANDO GUARDADO DE SESIÓN ")
        print(f" Datos de sesión disponibles: {len(self.session_data) if self.session_data else 0}")
        print(f" Hora de inicio: {self.session_start_time}")

        if not self.session_data:
            print(" No hay datos de sesión para guardar - sesión vacía")
            return

        print(f" Sesión válida con {len(self.session_data)} lecturas")
        print(f" Sesiones existentes antes: {len(self.sessions_history)}")
        
        
        print(f" VERIFICANDO DATOS COMPLETOS:")
        print(f"   - Primeras 5 lecturas: {[item.get('reading_number', '?') for item in self.session_data[:5]]}")
        print(f"   - Últimas 5 lecturas: {[item.get('reading_number', '?') for item in self.session_data[-5:]]}")
        print(f"   - Total items en session_data: {len(self.session_data)}")
        
        session_data_copy = []
        for item in self.session_data:
            session_data_copy.append(item.copy())  
        
        print(f"🔍 DESPUÉS DE COPIAR:")
        print(f"   - Items copiados: {len(session_data_copy)}")
        print(f"   - Primeras 3 copias: {[item.get('reading_number', '?') for item in session_data_copy[:3]]}")

        # Crear nueva sesión
        session = {
            "session_id": f"session_{int(dt.datetime.now().timestamp())}",
            "start_time": self.session_start_time,
            "end_time": dt.datetime.now().isoformat(),
            "total_readings": len(session_data_copy), 
            "data": session_data_copy,  
            "summary": self.get_session_summary()
        }

        print(f" Session ID: {session['session_id']}")
        print(f" Período: {session['start_time']} → {session['end_time']}")
        print(f" Total lecturas en sesión: {session['total_readings']}")
        print(f" Items reales en session['data']: {len(session['data'])}")
        
        # VERIFICAR QUE LOS DATOS ESTÉN COMPLETOS ANTES DE GUARDAR 
        if len(session['data']) != len(self.session_data):
            print(f" ERROR: Pérdida de datos en la copia!")
            print(f"   Original: {len(self.session_data)} items")
            print(f"   Copia: {len(session['data'])} items")
            # Intentar recuperar
            session['data'] = [item.copy() for item in self.session_data]
            session['total_readings'] = len(session['data'])
            print(f" Recuperación: {len(session['data'])} items")

        # Agregar al historial
        self.sessions_history.append(session)
        print(f"✅ Sesión agregada al array. Total sesiones: {len(self.sessions_history)}")

        # Guardar en archivo
        try:
            # Verificar que el directorio existe
            self.sessions_file.parent.mkdir(parents=True, exist_ok=True)
            print(f"📁 Guardando en archivo: {self.sessions_file}")
            
            with open(self.sessions_file, 'w', encoding='utf-8') as f:
                json.dump(self.sessions_history, f, indent=2, ensure_ascii=False)
            
            # Verificar que se guardó correctamente
            if self.sessions_file.exists():
                file_size = self.sessions_file.stat().st_size
                print(f" Archivo guardado exitosamente ({file_size} bytes)")
                
                # VERIFICAR EL CONTENIDO DEL ARCHIVO 
                try:
                    with open(self.sessions_file, 'r', encoding='utf-8') as f:
                        saved_data = json.load(f)
                        last_session = saved_data[-1]
                        print(f"VERIFICACIÓN DEL ARCHIVO:")
                        print(f"   - Sesiones en archivo: {len(saved_data)}")
                        print(f"   - Datos en última sesión: {len(last_session.get('data', []))}")
                        print(f"   - Total_readings: {last_session.get('total_readings', 0)}")
                except Exception as e:
                    print(f" Error verificando archivo: {e}")
            else:
                print(f" El archivo no se creó correctamente")
                
            print(f" SESIÓN GUARDADA COMPLETAMENTE")
            
            # Notificar a los navegadores conectados
            asyncio.create_task(self.broadcast_navegadores({
                'type': 'session_saved',
                'session_id': session['session_id'],
                'total_sessions': len(self.sessions_history)
            }))
            
        except Exception as e:
            print(f" ERROR guardando sesión en archivo: {e}")
            import traceback
            traceback.print_exc()

        print(f"=== FIN GUARDADO DE SESIÓN ===\n")
    
    def get_session_summary(self):
        """Obtener resumen de la sesión actual"""
        print(f" Calculando resumen para {len(self.session_data)} lecturas")
    
        if not self.session_data:
            print(" No hay datos para calcular resumen")
            return {}
    
        temps = [d.get('temperature', 0) for d in self.session_data if d.get('temperature')]
        phs = [d.get('ph', 0) for d in self.session_data if d.get('ph', 0) > 0]
        turbs = [d.get('turbidity', 0) for d in self.session_data if d.get('turbidity', 0) >= 0]
        tds_vals = [d.get('tds', 0) for d in self.session_data if d.get('tds', 0) >= 0]
    
        print(f"    Datos válidos: Temp={len(temps)}, pH={len(phs)}, Turb={len(turbs)}, TDS={len(tds_vals)}")
    
        summary = {
            "temperature": {
                "avg": sum(temps) / len(temps) if temps else 0,
                "min": min(temps) if temps else 0,
                "max": max(temps) if temps else 0
            },
            "ph": {
                "avg": sum(phs) / len(phs) if phs else 0,
                "min": min(phs) if phs else 0,
                "max": max(phs) if phs else 0
            },
            "turbidity": {
                "avg": sum(turbs) / len(turbs) if turbs else 0,
                "min": min(turbs) if turbs else 0,
                "max": max(turbs) if turbs else 0
            },
            "tds": {
                "avg": sum(tds_vals) / len(tds_vals) if tds_vals else 0,
                "min": min(tds_vals) if tds_vals else 0,
                "max": max(tds_vals) if tds_vals else 0
            }
        }
    
        print(f" Resumen calculado correctamente")
        return summary    
    
    def obtener_ip(self):
        """Obtiene la IP del PC"""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
                s.connect(("8.8.8.8", 80))
                ip = s.getsockname()[0]
            return ip
        except:
            return "192.168.137.1"
    
    def verificar_archivos_web(self):
        """SOLO verifica archivos existentes, NO los crea"""
        print(" === VERIFICACIÓN DE ARCHIVOS WEB ===")
        print(f" Directorio del script: {SCRIPT_DIR}")
        print(f" Directorio web objetivo: {WEB_DIR}")
        
        # Crear SOLO directorios si no existen 
        if not WEB_DIR.exists():
            print(f" Creando directorio {WEB_DIR}...")
            WEB_DIR.mkdir(parents=True, exist_ok=True)
            (WEB_DIR / "css").mkdir(exist_ok=True)
            (WEB_DIR / "js").mkdir(exist_ok=True)
            print(" Directorio creado pero SIN archivos")
        else:
            print(f" Directorio {WEB_DIR} ya existe")
        
        # Verificar archivos principales
        archivos_principales = {
            WEB_DIR / "index.html": "HTML principal",
            WEB_DIR / "css" / "styles.css": "Estilos CSS", 
            WEB_DIR / "js" / "script.js": "JavaScript"
        }
        
        archivos_existentes = 0
        archivos_con_pestanas = False
        
        for archivo, descripcion in archivos_principales.items():
            if archivo.exists():
                stat = archivo.stat()
                size = stat.st_size
                modified = dt.datetime.fromtimestamp(stat.st_mtime)  
                
                print(f" {descripcion}: {archivo}")
                print(f"    {size} bytes |  {modified.strftime('%H:%M:%S')}")
                
                # Verificar si el HTML tiene pestañas
                if archivo.name == 'index.html':
                    try:
                        with open(archivo, 'r', encoding='utf-8') as f:
                            contenido = f.read()
                            if 'tab-button' in contenido and 'sensor-tabs-section' in contenido:
                                print(f"    HTML contiene sistema de pestañas")
                                archivos_con_pestanas = True
                            else:
                                print(f"    HTML SIN pestañas")
                    except Exception as e:
                        print(f"    Error leyendo HTML: {e}")
                
                archivos_existentes += 1
            else:
                print(f" {descripcion}: {archivo} - FALTANTE")
        
        print(f"\n Resumen: {archivos_existentes}/3 archivos encontrados")
        
        if archivos_existentes == 3 and archivos_con_pestanas:
            print(" ¡Perfecto! Todos los archivos están listos con pestañas")
        elif archivos_existentes == 3:
            print(" Archivos presentes pero HTML sin pestañas")
        else:
            print(" Faltan archivos")
            print(f"\n Para crear archivos faltantes:")
            print(f"1. Ve al directorio: {WEB_DIR}")
            print(f"2. Crea los archivos HTML, CSS y JS allí")
            print(f"3. O ejecuta los comandos PowerShell modificados (ver abajo)")
        
        print("=" * 50)
        
        # Configurar archivo CSV
        self.archivo_csv = WEB_DIR / CSV_FILENAME
    
    def inicializar_csv(self):
        """Inicializa el archivo CSV SOLO si no existe"""
        if not self.archivo_csv.exists():
            print(f" Creando archivo CSV: {self.archivo_csv}")
            try:
                with open(self.archivo_csv, 'w', newline='', encoding='utf-8') as csvfile:
                    fieldnames = [
                        'timestamp_recepcion', 'device_id', 'timestamp_esp32', 
                        'rtc_timestamp', 'datetime_rtc', 'rtc_datetime_esp32',
                        'reading_number', 'sequence', 'temperature', 'ph', 
                        'turbidity', 'tds', 'ec', 'sensor_status', 'valid', 
                        'health_score', 'rssi', 'free_heap'
                    ]
                    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
                    writer.writeheader()
                print(" Archivo CSV inicializado")
            except Exception as e:
                print(f" Error creando CSV: {e}")
        else:
            print(f" Archivo CSV existente: {self.archivo_csv}")
    
    async def manejar_conexion(self, websocket):
        """Maneja todas las conexiones WebSocket"""
        client_ip = websocket.remote_address[0]
        
        try:
            mensaje_inicial = await asyncio.wait_for(websocket.recv(), timeout=5.0)
            data = json.loads(mensaje_inicial)
            
            if data.get('type') == 'web_browser':
                await self.manejar_navegador(websocket, client_ip)
            else:
                await self.manejar_esp32(websocket, client_ip)
                
        except asyncio.TimeoutError:
            await self.manejar_esp32(websocket, client_ip)
        except Exception as e:
            print(f" Error identificando cliente: {e}")
    
    async def manejar_esp32(self, websocket, client_ip):
        """Maneja conexión del ESP32"""
        self.conexion_esp32 = websocket

        self.session_data = []  # Nueva sesión
        self.session_start_time = dt.datetime.now().isoformat()  

        print(f"🔄 Nueva sesión iniciada: {self.session_start_time}")
        print(f"📊 Sesiones totales antes: {len(self.sessions_history)}")
        
        print(f"🌊 ESP32 CONECTADO desde: {client_ip}")
        print(f"⏰ Hora: {dt.datetime.now().strftime('%H:%M:%S')}")
        
        await self.notificar_estado_esp32(True)
        
        try:
            saludo = {
                "status": "conectado",
                "mensaje": "Servidor listo para recibir datos",
                "timestamp": dt.datetime.now().isoformat()
            }
            await websocket.send(json.dumps(saludo))
            
            async for mensaje in websocket:
                try:
                    datos = json.loads(mensaje)

                    if datos.get('action') in ['calibrate', 'get_calibration']:
                        await self.procesar_comando_calibracion(datos, websocket)
                    elif datos.get('action') == 'sending_data':
                        await self.iniciar_descarga()
                    elif datos.get('device_id') == 'ESP32_WaterMonitor':
                        await self.procesar_datos_sensor(datos, websocket)
                    elif datos.get('action') == 'data_complete':
                        await self.finalizar_descarga(datos.get('total', 0))
                    
                        
                except json.JSONDecodeError:
                    print(f" JSON inválido de ESP32")
                    
        except websockets.exceptions.ConnectionClosed:
            print(f" ESP32 desconectado: {client_ip}")
        except Exception as e:
            print(f" Error en ESP32: {e}")
        finally:
            self.save_session_to_history()
            self.conexion_esp32 = None
            await self.notificar_estado_esp32(False)
    
    async def procesar_comando_calibracion(self, datos, websocket):
        """Procesa comandos de calibración del ESP32 o navegador"""
        action = datos.get('action')
        
        print(f"📝 Procesando comando de calibración: {action}")
        
        if action == 'get_calibration':
            # Solicitud de valores actuales
            print("📊 Solicitando valores de calibración al ESP32")
            await websocket.send(json.dumps(datos))
            
        elif action == 'calibrate':
            # Comando de calibración
            print("🔧 Aplicando calibración:")
            
            if datos.get('restore_defaults'):
                print("  🔄 Restaurando valores por defecto")
            else:
                if 'ph_offset' in datos or 'ph_slope' in datos:
                    print(f"  pH: offset={datos.get('ph_offset', '?')}, slope={datos.get('ph_slope', '?')}")
                if 'tds_kvalue' in datos or 'tds_voffset' in datos:
                    print(f"  TDS: k={datos.get('tds_kvalue', '?')}, v={datos.get('tds_voffset', '?')}")
                if any(k in datos for k in ['turb_coeff_a', 'turb_coeff_b', 'turb_coeff_c', 'turb_coeff_d']):
                    print(f"  Turbidez: coeficientes actualizados")
            
            # Reenviar al ESP32
            await websocket.send(json.dumps(datos))

    async def manejar_navegador(self, websocket, client_ip):
        """Maneja conexión del navegador web"""
        self.conexiones_web.add(websocket)
        
        print(f"🌐 Navegador conectado: {client_ip}")
        
        await websocket.send(json.dumps({
            'type': 'esp32_status',
            'connected': self.conexion_esp32 is not None
        }))
        
        try:
            async for mensaje in websocket:
                try:
                    data = json.loads(mensaje)
                    
                    if data.get('type') == 'request_data':
                        await self.solicitar_datos_esp32()
                    elif data.get('type') == 'request_sessions_history':
                        await self.enviar_historial_sesiones(websocket)
                    elif data.get('type') == 'delete_session':
                        await self.eliminar_sesion(websocket, data.get('session_id'))
                    elif data.get('action') in ['calibrate', 'get_calibration']:
                        # Reenviar comando de calibración al ESP32
                        if self.conexion_esp32:
                            print(f"📡 Reenviando comando de calibración al ESP32")
                            await self.conexion_esp32.send(json.dumps(data))
                        else:
                            await websocket.send(json.dumps({
                                'status': 'error',
                                'message': 'ESP32 no conectado'
                            }))
                except json.JSONDecodeError:
                    print(f"⚠ JSON inválido del navegador")
                    
        except websockets.exceptions.ConnectionClosed:
            print(f"🔌 Navegador desconectado: {client_ip}")
        except Exception as e:
            print(f"⚠ Error en navegador: {e}")
        finally:
            self.conexiones_web.discard(websocket)



    async def enviar_historial_sesiones(self, websocket):
        """Enviar historial de sesiones al navegador"""
        try:
            await websocket.send(json.dumps({
                'type': 'sessions_history',
                'sessions': self.sessions_history
            }))
            print(f" Enviado historial: {len(self.sessions_history)} sesiones")
        except Exception as e:
            print(f" Error enviando historial: {e}")
    
    async def eliminar_sesion(self, websocket, session_id):
        """Eliminar una sesión específica del historial"""
        try:
            # Buscar y eliminar la sesión
            sesion_encontrada = None
            for i, session in enumerate(self.sessions_history):
                if session.get('session_id') == session_id:
                    sesion_encontrada = self.sessions_history.pop(i)
                    break
            
            if sesion_encontrada:
                # Guardar historial actualizado
                with open(self.sessions_file, 'w', encoding='utf-8') as f:
                    json.dump(self.sessions_history, f, indent=2, ensure_ascii=False)
                
                print(f"🗑️ Sesión eliminada: {session_id}")
                print(f"📊 Sesiones restantes: {len(self.sessions_history)}")
                
                # Notificar éxito
                await websocket.send(json.dumps({
                    'type': 'session_deleted',
                    'success': True,
                    'session_id': session_id,
                    'total_sessions': len(self.sessions_history)
                }))
                
                # Enviar historial actualizado
                await self.enviar_historial_sesiones(websocket)
                
            else:
                # Sesión no encontrada
                await websocket.send(json.dumps({
                    'type': 'session_deleted',
                    'success': False,
                    'message': 'Sesión no encontrada'
                }))
                
        except Exception as e:
            print(f" Error eliminando sesión: {e}")
            await websocket.send(json.dumps({
                'type': 'session_deleted',
                'success': False,
                'message': f'Error: {str(e)}'
            }))
    
    async def solicitar_datos_esp32(self):
        """Solicita al ESP32 que envíe todos los datos"""
        if not self.conexion_esp32:
            await self.broadcast_navegadores({
                'type': 'download_error',
                'message': 'ESP32 no conectado'
            })
            return
        
        print("📥 Solicitando datos al ESP32...")
        
        solicitud = {
            'action': 'request_all_data',
            'timestamp': dt.datetime.now().isoformat() 
        }
        
        try:
            await self.conexion_esp32.send(json.dumps(solicitud))
            self.esperando_datos = True
            self.datos_solicitados = True
            
        except Exception as e:
            print(f" Error solicitando datos: {e}")
            await self.broadcast_navegadores({
                'type': 'download_error',
                'message': 'Error al solicitar datos'
            })
    
    async def iniciar_descarga(self):
        """Notifica a navegadores que inició la descarga"""
        await self.broadcast_navegadores({
            'type': 'download_start',
            'timestamp': dt.datetime.now().isoformat()  
        })
        print(" Iniciando recepción de datos...")
    
    async def procesar_datos_sensor(self, datos, websocket_esp32):
        """Procesa datos de sensores recibidos con RTC"""
        self.datos_recibidos.append(datos)
        self.session_data.append(datos)
        self.total_mensajes += 1
    
        self.guardar_en_csv(datos)
    
        reading_num = datos.get('reading_number', '?')
        temp = datos.get('temperature', 0)
        rtc_datetime = datos.get('rtc_datetime', None)
    
        if rtc_datetime and rtc_datetime != "No disponible":
            print(f"   Lectura #{reading_num}: {temp:.1f}°C @ {rtc_datetime}")
        elif datos.get('rtc_timestamp', 0) > 1609459200:
            datetime_rtc = dt.datetime.fromtimestamp(datos.get('rtc_timestamp')).strftime('%Y-%m-%d %H:%M:%S')  
            print(f"   Lectura #{reading_num}: {temp:.1f}°C @ {datetime_rtc}")
        else:
            print(f"   Lectura #{reading_num}: {temp:.1f}°C")
    
        await self.broadcast_navegadores(datos)
    
        if self.esperando_datos:
            confirmacion = {
                'status': 'received',
                'reading_number': reading_num
            }
            await websocket_esp32.send(json.dumps(confirmacion))
    
    async def finalizar_descarga(self, total):
        """Finaliza el proceso de descarga"""
        self.esperando_datos = False
        self.datos_solicitados = False
        
        print(f" Descarga completa: {total} lecturas recibidas")
        
        await self.broadcast_navegadores({
            'type': 'download_complete',
            'total': total,
            'timestamp': dt.datetime.now().isoformat()  
        })
    
    async def notificar_estado_esp32(self, conectado):
        """Notifica a navegadores el estado del ESP32"""
        await self.broadcast_navegadores({
            'type': 'esp32_status',
            'connected': conectado
        })
    
    async def broadcast_navegadores(self, datos):
        """Envía datos a todos los navegadores conectados"""
        if not self.conexiones_web:
            return
        
        mensaje = json.dumps(datos)
        desconexiones = []
        
        for websocket in list(self.conexiones_web):
            try:
                await websocket.send(mensaje)
            except:
                desconexiones.append(websocket)
        
        for ws in desconexiones:
            self.conexiones_web.discard(ws)
    
    def guardar_en_csv(self, datos):
        """Guarda datos en CSV con timestamp RTC corregido"""
        try:
            with open(self.archivo_csv, 'a', newline='', encoding='utf-8') as csvfile:
                fieldnames = [
                    'timestamp_recepcion', 'device_id', 'timestamp_esp32', 
                    'rtc_timestamp', 'datetime_rtc', 'rtc_datetime_esp32',
                    'reading_number', 'sequence', 'temperature', 'ph', 
                    'turbidity', 'tds', 'ec', 'sensor_status', 'valid', 
                    'health_score', 'rssi', 'free_heap'
                ]
                writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            
                # Solo escribir headers si el archivo está vacío
                csvfile.seek(0, 2)  
                if csvfile.tell() == 0:
                    writer.writeheader()
            
                rtc_timestamp = datos.get('rtc_timestamp', 0)
                datetime_rtc = ""
                
                # CORRECCIÓN: El timestamp ya viene en hora local de Colombia
                if rtc_timestamp > 1609459200:  # Timestamp válido
                    # Usar directamente el timestamp (ya está en hora local)
                    datetime_rtc = dt.datetime.fromtimestamp(rtc_timestamp).strftime('%Y-%m-%d %H:%M:%S')
            
                datos_csv = {
                    'timestamp_recepcion': dt.datetime.now().isoformat(),
                    'device_id': datos.get('device_id', 'Unknown'),
                    'timestamp_esp32': datos.get('timestamp', 0),
                    'rtc_timestamp': rtc_timestamp,
                    'datetime_rtc': datetime_rtc,
                    'rtc_datetime_esp32': datos.get('rtc_datetime', 'No disponible'),
                    'reading_number': datos.get('reading_number', 0),
                    'sequence': datos.get('sequence', 0),
                    'temperature': datos.get('temperature', 0),
                    'ph': datos.get('ph', 0),
                    'turbidity': datos.get('turbidity', 0),
                    'tds': datos.get('tds', 0),
                    'ec': datos.get('ec', 0),
                    'sensor_status': datos.get('sensor_status', 0),
                    'valid': datos.get('valid', False),
                    'health_score': datos.get('health_score', 0),
                    'rssi': datos.get('rssi', 0),
                    'free_heap': datos.get('free_heap', 0)
                }
            
                writer.writerow(datos_csv)
            
        except Exception as e:
            print(f" Error guardando CSV: {e}")
    
    def iniciar_servidor_http(self):
        """Inicia servidor HTTP que RESPETA archivos existentes"""
        class RespectfulHTTPRequestHandler(SimpleHTTPRequestHandler):
            def __init__(self, *args, **kwargs):
    
                super().__init__(*args, directory=str(WEB_DIR), **kwargs)
        
            def end_headers(self):
                # Headers anti-caché para desarrollo
                self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
                self.send_header('Pragma', 'no-cache')
                self.send_header('Expires', '0')
                super().end_headers()
        
            def do_GET(self):
                if self.path == '/' or self.path == '':
                    self.path = '/index.html'
                
                print(f" Sirviendo: {self.path}")
                return super().do_GET()
        
            def log_message(self, format, *args):
                pass  # Suprimir logs HTTP normales
    
        def run_server():
            try:
                httpd = HTTPServer(('', HTTP_PORT), RespectfulHTTPRequestHandler)
                print(f" Servidor HTTP iniciado: http://{self.server_ip}:{HTTP_PORT}")
                print(f" Directorio web: {WEB_DIR}")
                httpd.serve_forever()
            except Exception as e:
                print(f" Error servidor HTTP: {e}")
    
        http_thread = threading.Thread(target=run_server, daemon=True)
        http_thread.start()
    
    def mostrar_comandos_powershell(self):
        """Muestra comandos PowerShell con las rutas correctas"""
        print("\n === COMANDOS POWERSHELL CORREGIDOS ===")
        print(f"Ejecuta estos comandos desde cualquier lugar:")
        print()
        
        # Comandos con rutas absolutas
        css_dir = WEB_DIR / "css"
        js_dir = WEB_DIR / "js"
        
        print("# 1. Crear directorios (si no existen)")
        print(f'mkdir "{css_dir}" -Force')
        print(f'mkdir "{js_dir}" -Force')
        print()
        
        print("# 2. Crear index.html con pestañas")
        print(f'@"')
        print("<!DOCTYPE html>")
        print("<!-- ... contenido HTML completo ... -->")
        print(f'"@ | Out-File -Encoding UTF8 "{WEB_DIR / "index.html"}"')
        print()
        
        print("# 3. Crear styles.css")
        print(f'@"')
        print("/* ... contenido CSS completo ... */")
        print(f'"@ | Out-File -Encoding UTF8 "{css_dir / "styles.css"}"')
        print()
        
        print("# 4. Crear script.js")
        print(f'@"')
        print("// ... contenido JavaScript completo ...")
        print(f'"@ | Out-File -Encoding UTF8 "{js_dir / "script.js"}"')
        print()
        
        print("=======================================")
    
    async def iniciar(self):
        """Inicia el servidor completo"""
        print("=" * 80)
        print(" SERVIDOR MONITOR DE CALIDAD DEL AGUA")
        print("    VERSIÓN CON RUTAS CORREGIDAS")
        print("=" * 80)
        print(f" Interfaz web: http://{self.server_ip}:{HTTP_PORT}")
        print(f" WebSocket: ws://{self.server_ip}:{WEBSOCKET_PORT}")
        print("=" * 80)
        print(" Configura esta IP en tu ESP32:")
        print(f"   - Server IP: {self.server_ip}")
        print(f"   - Puerto: {WEBSOCKET_PORT}")
        print("=" * 80)
        
        # Verificar que los archivos web existan
        archivos_necesarios = [
            WEB_DIR / "index.html",
            WEB_DIR / "css" / "styles.css", 
            WEB_DIR / "js" / "script.js"
        ]
        
        archivos_faltantes = [archivo for archivo in archivos_necesarios if not archivo.exists()]
        
        if archivos_faltantes:
            print(" FALTAN ARCHIVOS WEB:")
            for archivo in archivos_faltantes:
                print(f"   - {archivo}")
            print(f"\n Los archivos deben estar en: {WEB_DIR}")
            self.mostrar_comandos_powershell()
        
        print(" Iniciando servidores...")
        
        self.iniciar_servidor_http()
        
        server = await websockets.serve(
            self.manejar_conexion,
            "0.0.0.0",
            WEBSOCKET_PORT,
            ping_interval=30,
            ping_timeout=10
        )
        
        print(" Servidores iniciados correctamente")
        print(f" Abre tu navegador en: http://{self.server_ip}:{HTTP_PORT}")
        print(" Este servidor NO sobrescribe archivos existentes")
        print(f" web_interface está en: {WEB_DIR}")
        print(" Presiona Ctrl+C para detener")
        print("=" * 80)
        
        try:
            webbrowser.open(f"http://{self.server_ip}:{HTTP_PORT}")
        except:
            pass
        
        await server.wait_closed()

async def main():
    servidor = ServidorMonitorAgua()
    
    try:
        await servidor.iniciar()
    except KeyboardInterrupt:
        print("\n\n Servidor detenido")
    except Exception as e:
        print(f"\n Error: {e}")

if __name__ == "__main__":
    try:
        import websockets
        asyncio.run(main())
    except ImportError:
        print(" Instala websockets: pip install websockets")