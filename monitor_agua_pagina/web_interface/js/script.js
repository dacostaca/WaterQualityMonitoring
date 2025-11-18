// Monitor de Calidad del Agua - Control Manual con RTC y Pestañas
class WaterMonitor {
    constructor() {
        this.ws = null;
        this.charts = {};
        this.largeCharts = {}; 
        this.data = [];
        this.esp32Connected = false;
        this.downloadInProgress = false;
        this.activeTab = 'temperature'; 
        this.tdsDisplayMode = 'tds';
        this.sessionsHistory = [];
        this.currentSession = null;
        this.sidebarOpen = false;
        // ═══════ AGREGAR ═══════
        this.currentCalibration = null;
        this.calibrationTabActive = false;
        // ═══════ FIN NUEVO ═══════
        this.init();
    }
    
    init() {
        this.setupEventListeners();
        this.setupTabs();
        this.createLargeCharts();
        this.connectWebSocket();
        this.updateServerStartTime();
    }
    
    setupEventListeners() {
        // Botón de descarga
        const downloadBtn = document.getElementById('download-btn');
        downloadBtn.addEventListener('click', () => this.requestData());
        
        // Selector de variable TDS/EC
        const tdsSelector = document.getElementById('tds-variable-select');
        if (tdsSelector) {
            tdsSelector.addEventListener('change', (e) => {
                this.tdsDisplayMode = e.target.value;
                this.updateTDSDisplay();
            });
        }
        
        // Cerrar WebSocket al salir
        window.addEventListener('beforeunload', () => {
            if (this.ws) {
                this.ws.close();
            }
        });
    
        const sidebarToggle = document.getElementById('sidebar-toggle');
        const sidebarClose = document.getElementById('sidebar-close');
        const sidebarOverlay = document.getElementById('sidebar-overlay');
        const backToSessions = document.getElementById('back-to-sessions');
    
        sidebarToggle.addEventListener('click', () => this.openSidebar());
        sidebarClose.addEventListener('click', () => this.closeSidebar());
        sidebarOverlay.addEventListener('click', () => this.closeSidebar());
        backToSessions.addEventListener('click', () => this.showSessionsList());

        const deleteSessionBtn = document.getElementById('delete-session-btn');
        deleteSessionBtn.addEventListener('click', () => this.confirmDeleteSession());
    }

    /*
    exportCSV() {
        window.location.href = 'datos_calidad_agua.csv';
    }
    */
    
    setupTabs() {
        // Configurar eventos de pestañas
        const tabButtons = document.querySelectorAll('.tab-button');
        
        tabButtons.forEach(button => {
            button.addEventListener('click', () => {
                const tabName = button.getAttribute('data-tab');
                this.switchTab(tabName);
            });
        });
        const calibrationTab = document.querySelector('[data-tab="calibration"]');
        if (calibrationTab) {
            calibrationTab.addEventListener('click', () => {
                this.switchTab('calibration');
                this.calibrationTabActive = true;
                // Solicitar valores actuales al abrir la pestaña
                if (this.esp32Connected) {
                    this.requestCurrentCalibration();
                }
            });
        }
    }
    
    switchTab(tabName) {
        // Remover clase active de todos los botones y paneles
        document.querySelectorAll('.tab-button').forEach(btn => btn.classList.remove('active'));
        document.querySelectorAll('.tab-panel').forEach(panel => panel.classList.remove('active'));
        
        // Activar el botón y panel correspondiente
        document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');
        document.getElementById(`tab-${tabName}`).classList.add('active');
        
        this.activeTab = tabName;
        
        // Actualizar contenido según la pestaña
        if (this.data.length > 0) {
            if (tabName === 'complete-history') {
                this.updateCompleteHistoryTab();
            } else {
                this.updateLargeChart(tabName);
                this.updateSensorTable(tabName);
                this.updateSensorSummary(tabName);
            }
        }
    }
    
    connectWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.hostname}:8765`;
        
        try {
            this.ws = new WebSocket(wsUrl);
            
            this.ws.onopen = () => {
                console.log(' Conectado al servidor WebSocket');
                this.ws.send(JSON.stringify({
                    type: 'web_browser',
                    action: 'connect'
                }));
            };
            
            this.ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    this.handleMessage(data);
                } catch (e) {
                    console.error('Error parsing message:', e);
                }
            };
            
            this.ws.onclose = () => {
                console.log(' Desconectado del servidor WebSocket');
                this.esp32Connected = false;
                this.updateConnectionStatus(false);
                setTimeout(() => this.connectWebSocket(), 3000);
            };
            
            this.ws.onerror = (error) => {
                console.error(' Error WebSocket:', error);
            };
            
        } catch (error) {
            console.error(' Error creating WebSocket:', error);
        }
    }
    
    handleMessage(data) {
        if (data.type === 'esp32_status') {
            this.esp32Connected = data.connected;
            this.updateConnectionStatus(data.connected);
            document.getElementById('download-btn').disabled = !data.connected;
            document.getElementById('esp32-status').textContent = 
                data.connected ? 'Conectado' : 'Desconectado';
        }
        else if (data.status === 'success' && data.calibration) {
            // Respuesta de calibración
            this.currentCalibration = data.calibration;
            this.updateCalibrationDisplay(data.calibration);
            
            if (data.message) {
                this.addCalibrationLog('✓ ' + data.message, 'success');
            } else {
                this.addCalibrationLog('✓ Valores de calibración recibidos', 'success');
            }
        }
        else if (data.status === 'error' && data.code !== undefined) {
            // Error de calibración
            this.addCalibrationLog('✗ Error: ' + data.message + ' (código: ' + data.code + ')', 'error');
        }



        else if (data.type === 'download_start') {
            this.downloadInProgress = true;
            this.data = [];
            this.updateDownloadStatus('Descargando datos...', 'loading');
        }
        else if (data.device_id === 'ESP32_WaterMonitor' && data.temperature !== undefined) {
            this.addSensorData(data);
        }
        else if (data.type === 'download_complete') {
            this.downloadInProgress = false;
            this.updateDownloadStatus(
                ` Descarga completa: ${data.total} lecturas`, 
                'success'
            );
            this.finalizeDataDisplay();
        }
        else if (data.type === 'download_error') {
            this.downloadInProgress = false;
            this.updateDownloadStatus(
                ` Error: ${data.message}`, 
                'error'
            );
        }
        if (data.type === 'sessions_history') {
            this.sessionsHistory = data.sessions;
            this.updateSessionsList();
        }
        else if (data.type === 'session_deleted') {
            this.handleSessionDeleted(data);
        }
    }

    openSidebar() {
        const sidebar = document.getElementById('sidebar-menu');
        const overlay = document.getElementById('sidebar-overlay');
        
        sidebar.classList.add('open');
        overlay.classList.remove('hidden');
        overlay.classList.add('visible');
        this.sidebarOpen = true;
        
        // Solicitar historial de sesiones
        this.requestSessionsHistory();
    }
    
    closeSidebar() {
        const sidebar = document.getElementById('sidebar-menu');
        const overlay = document.getElementById('sidebar-overlay');
        
        sidebar.classList.remove('open');
        overlay.classList.remove('visible');
        setTimeout(() => overlay.classList.add('hidden'), 300);
        this.sidebarOpen = false;
    }
    
    requestSessionsHistory() {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify({
                type: 'request_sessions_history'
            }));
        }
    }
    
    updateSessionsList() {
        const container = document.getElementById('sessions-container');
        
        if (this.sessionsHistory.length === 0) {
            container.innerHTML = `
                <div class="no-sessions">
                    <div class="no-sessions-icon">📊</div>
                    <div class="no-sessions-text">No hay sesiones guardadas</div>
                    <div class="no-sessions-hint">Las sesiones se guardan automáticamente cuando el ESP32 se conecta</div>
                </div>
            `;
            return;
        }
        
        // Limpiar contenedor
        container.innerHTML = '';
        
        // Crear elementos de sesión con event listeners
        this.sessionsHistory
            .forEach((session, visualIndex) => {
                const startDate = new Date(session.start_time);
                const sessionDiv = document.createElement('div');
                sessionDiv.className = 'session-item';
                sessionDiv.innerHTML = `
                    <div class="session-title">
                        Prueba ${visualIndex + 1}
                    </div>
                    <div class="session-meta">
                        <span>${startDate.toLocaleDateString()} ${startDate.toLocaleTimeString()}</span>
                        <span>${session.total_readings} lecturas</span>
                        <span></span>
                    </div>
                `;
                sessionDiv.addEventListener('click', () => {
                    this.showSessionDetail(visualIndex);
                });
                container.appendChild(sessionDiv);
            });
    }
    
    showSessionDetail(sessionIndex) {
        console.log(' showSessionDetail llamado con índice:', sessionIndex);
        console.log(' Total sesiones:', this.sessionsHistory.length);
        console.log(' Sesiones disponibles:', this.sessionsHistory);
        
        if (sessionIndex < 0 || sessionIndex >= this.sessionsHistory.length) {
            console.error(' Índice fuera de rango');
            return;
        }
        
        this.currentSession = this.sessionsHistory[sessionIndex];
        console.log(' Sesión seleccionada:', this.currentSession);
        

        if (!this.currentSession || !this.currentSession.data) {
            console.error(' Sesión sin datos');
            alert('Esta sesión no tiene datos disponibles');
            return;
        }
        
        document.querySelector('.sessions-list').classList.add('hidden');
        document.getElementById('session-detail').classList.remove('hidden');
        
        this.renderSessionDetail();
    }
    
    showSessionsList() {
        document.querySelector('.sessions-list').classList.remove('hidden');
        document.getElementById('session-detail').classList.add('hidden');
    }
    
    renderSessionDetail() {
        if (!this.currentSession || !this.currentSession.data) {
            console.error('No hay sesión actual para mostrar');
            return;
        }
        
        const sessionInfo = document.getElementById('session-info');
        const sessionDataTable = document.getElementById('session-data-table');
        
        const startDate = new Date(this.currentSession.start_time);
        const endDate = new Date(this.currentSession.end_time);
        const summary = this.currentSession.summary || {};
        
        // Información de la sesión
        sessionInfo.innerHTML = `
            <h4>📊 Información de la Sesión</h4>
            <div style="margin: 15px 0;">
                <p><strong>🕐 Inicio:</strong> ${startDate.toLocaleString()}</p>
                <p><strong>🏁 Fin:</strong> ${endDate.toLocaleString()}</p>
                <p><strong>📈 Total lecturas:</strong> ${this.currentSession.total_readings}</p>
                <p><strong>⏱️ Duración:</strong> ${this.calculateDuration(startDate, endDate)}</p>
            </div>
            
            <div class="session-summary">
                <h5 style="margin-bottom: 10px;">📊 Resumen de Mediciones:</h5>
                <div class="summary-grid" style="display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px;">
                    <div class="summary-item">
                        <div class="summary-label">🌡️ Temperatura</div>
                        <div class="summary-value">${summary.temperature?.avg?.toFixed(1) || '--'}°C</div>
                        <div style="font-size: 0.8em; color: #666;">
                            Min: ${summary.temperature?.min?.toFixed(1) || '--'}°C | 
                            Max: ${summary.temperature?.max?.toFixed(1) || '--'}°C
                        </div>
                    </div>
                    <div class="summary-item">
                        <div class="summary-label">🧪 pH</div>
                        <div class="summary-value">${summary.ph?.avg?.toFixed(2) || '--'}</div>
                        <div style="font-size: 0.8em; color: #666;">
                            Min: ${summary.ph?.min?.toFixed(2) || '--'} | 
                            Max: ${summary.ph?.max?.toFixed(2) || '--'}
                        </div>
                    </div>
                    <div class="summary-item">
                        <div class="summary-label">🌫️ Turbidez</div>
                        <div class="summary-value">${summary.turbidity?.avg?.toFixed(1) || '--'} NTU</div>
                        <div style="font-size: 0.8em; color: #666;">
                            Min: ${summary.turbidity?.min?.toFixed(1) || '--'} | 
                            Max: ${summary.turbidity?.max?.toFixed(1) || '--'} NTU
                        </div>
                    </div>
                    <div class="summary-item">
                        <div class="summary-label">💧 TDS</div>
                        <div class="summary-value">${summary.tds?.avg?.toFixed(0) || '--'} ppm</div>
                        <div style="font-size: 0.8em; color: #666;">
                            Min: ${summary.tds?.min?.toFixed(0) || '--'} | 
                            Max: ${summary.tds?.max?.toFixed(0) || '--'} ppm
                        </div>
                    </div>
                </div>
            </div>
        `;
        
        const maxRecords = Math.min(200, this.currentSession.data.length);
        const displayData = [...this.currentSession.data].reverse().slice(0, maxRecords);
        
        // Tabla de datos
        const tableHtml = `
            <h4>📋 Datos Detallados de la Sesión</h4>
            <div class="table-container" style="max-height: 500px; overflow-y: auto;">
                <table>
                    <thead>
                        <tr>
                            <th>#</th>
                            <th>Fecha/Hora</th>
                            <th>Temp (°C)</th>
                            <th>pH</th>
                            <th>Turbidez</th>
                            <th>TDS</th>
                            <th>EC</th>
                            <th>Estado</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${displayData.map((item, idx) => `
                            <tr>
                                <td>${item.reading_number || (this.currentSession.data.length - idx)}</td>
                                <td>${this.formatDateTime(item)}</td>
                                <td>${item.temperature?.toFixed(1) || '-'}</td>
                                <td>${item.ph > 0 ? item.ph.toFixed(2) : '-'}</td>
                                <td>${item.turbidity >= 0 ? item.turbidity.toFixed(1) : '-'}</td>
                                <td>${item.tds >= 0 ? item.tds.toFixed(0) : '-'}</td>
                                <td>${item.ec >= 0 ? item.ec.toFixed(1) : '-'}</td>
                                <td>${item.valid ? 'SI' : 'NO'}</td>
                            </tr>
                        `).join('')}
                        ${this.currentSession.data.length > maxRecords ? `
                            <tr>
                                <td colspan="8" style="text-align: center; font-style: italic; color: #666; padding: 15px; background: #f8f9fa;">
                                    📊 Mostrando los últimos ${maxRecords} de ${this.currentSession.data.length} registros totales
                                </td>
                            </tr>
                        ` : ''}
                    </tbody>
                </table>
            </div>
        `;
        
        sessionDataTable.innerHTML = tableHtml;
        
        const exportSessionBtn = document.getElementById('export-session-btn');
        if (exportSessionBtn) {
            // Remover event listener anterior si existe
            exportSessionBtn.replaceWith(exportSessionBtn.cloneNode(true));
            
            // Agregar nuevo event listener
            document.getElementById('export-session-btn').addEventListener('click', () => {
                this.exportCurrentSession();
            });
        }
    }

    calculateDuration(startDate, endDate) {
        const diff = endDate - startDate;
        const minutes = Math.floor(diff / 60000);
        const seconds = Math.floor((diff % 60000) / 1000);
        return `${minutes} min ${seconds} seg`;
    }
    
    formatDateTime(item) {
        if (item.rtc_datetime && item.rtc_datetime !== "No disponible") {
            return item.rtc_datetime;
        } else if (item.timestamp_web) {
            return new Date(item.timestamp_web).toLocaleString();
        }
        return '-';
    }

    exportCurrentSession() {
        if (!this.currentSession || !this.currentSession.data) {
            alert('No hay sesión seleccionada para exportar');
            return;
        }
        
        console.log('🔄 Exportando sesión:', this.currentSession.session_id);
        
        // Crear contenido CSV
        const headers = [
            'Numero_Lectura',
            'Fecha_Hora_RTC', 
            'Timestamp_Unix',
            'Temperatura_C',
            'pH',
            'Turbidez_NTU',
            'TDS_ppm',
            'EC_uS_cm',
            'Estado_Valido',
            'RSSI_dBm',
            'Salud_Sistema',
            'Timestamp_Recepcion'
        ];
        
        let csvContent = headers.join(',') + '\n';
        
        // Agregar datos de la sesión
        this.currentSession.data.forEach(item => {
            const row = [
                item.reading_number || '',
                this.formatDateTimeForCSV(item),
                item.rtc_timestamp || '',
                item.temperature?.toFixed(2) || '',
                item.ph > 0 ? item.ph.toFixed(2) : '',
                item.turbidity >= 0 ? item.turbidity.toFixed(1) : '',
                item.tds >= 0 ? item.tds.toFixed(0) : '',
                item.ec >= 0 ? item.ec.toFixed(1) : '',
                item.valid ? 'VALIDA' : 'INVALIDA',
                item.rssi || '',
                item.health_score || '',
                item.timestamp_web || ''
            ];
            csvContent += row.join(',') + '\n';
        });
        
        // Crear y descargar archivo
        const startDate = new Date(this.currentSession.start_time);
        const fileName = `Sesion_${startDate.getFullYear()}-${String(startDate.getMonth()+1).padStart(2,'0')}-${String(startDate.getDate()).padStart(2,'0')}_${String(startDate.getHours()).padStart(2,'0')}-${String(startDate.getMinutes()).padStart(2,'0')}.csv`;
        
        const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
        const link = document.createElement('a');
        
        if (link.download !== undefined) {
            const url = URL.createObjectURL(blob);
            link.setAttribute('href', url);
            link.setAttribute('download', fileName);
            link.style.visibility = 'hidden';
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
            
            console.log(' Sesión exportada como:', fileName);
            
            // Mostrar mensaje de éxito
            const originalText = document.getElementById('export-session-btn').textContent;
            document.getElementById('export-session-btn').textContent = 'Exportado';
            setTimeout(() => {
                document.getElementById('export-session-btn').textContent = originalText;
            }, 2000);
        } else {
            alert('Tu navegador no soporta la descarga automática de archivos');
        }
    }

    formatDateTimeForCSV(item) {
        if (item.rtc_datetime && item.rtc_datetime !== "No disponible") {
            return `"${item.rtc_datetime}"`;
        } else if (item.rtc_timestamp && item.rtc_timestamp > 1609459200) {
            // CORRECCIÓN: El timestamp ya está en hora local de Colombia
            // No aplicar offset adicional
            const date = new Date(item.rtc_timestamp * 1000);
            return `"${date.toLocaleString('es-CO', {
                year: 'numeric',
                month: '2-digit',
                day: '2-digit',
                hour: '2-digit',
                minute: '2-digit',
                second: '2-digit',
                timeZone: 'America/Bogota'  // Asegurar zona horaria correcta
            })}"`;
        } else if (item.timestamp_web) {
            return `"${new Date(item.timestamp_web).toLocaleString()}"`;
        }
        return '"Sin fecha"';
    }
    

    updateTDSDisplay() {
        if (this.data.length === 0) return;
        
        this.updateLargeChart('tds');
        
        this.updateSensorTable('tds');

        this.updateSensorSummary('tds');
        
        // Actualizar títulos según el modo
        const tableTitle = document.getElementById('tds-table-title');
        const tableHeader = document.getElementById('tds-table-header');
        
        if (this.tdsDisplayMode === 'ec') {
            tableTitle.textContent = '📋 Historial de Conductividad Eléctrica';
            tableHeader.textContent = 'EC (µS/cm)';
        } else {
            tableTitle.textContent = '📋 Historial de TDS';
            tableHeader.textContent = 'TDS (ppm)';
        }
    }

    updateCompleteHistoryTab() {
        if (this.data.length === 0) {
        
            document.getElementById('history-total-readings').textContent = '0';
            document.getElementById('history-first-reading').textContent = '--';
            document.getElementById('history-last-reading').textContent = '--';
            document.getElementById('history-esp32-status').textContent = '--';
            return;
        }
    
        const totalReadings = this.data.length;
        const firstReading = this.data[0];
        const lastReading = this.data[this.data.length - 1];
        
        document.getElementById('history-total-readings').textContent = totalReadings.toString();
        
        // Primera lectura
        let firstDateTime = '--';
        if (firstReading.rtc_datetime && firstReading.rtc_datetime !== "No disponible") {
            firstDateTime = firstReading.rtc_datetime;
        } else if (firstReading.rtc_timestamp && firstReading.rtc_timestamp > 1609459200) {
            const date = new Date(firstReading.rtc_timestamp * 1000);
            firstDateTime = date.toLocaleString();
        }
        document.getElementById('history-first-reading').textContent = firstDateTime;
        
        // Última lectura
        let lastDateTime = '--';
        if (lastReading.rtc_datetime && lastReading.rtc_datetime !== "No disponible") {
            lastDateTime = lastReading.rtc_datetime;
        } else if (lastReading.rtc_timestamp && lastReading.rtc_timestamp > 1609459200) {
            const date = new Date(lastReading.rtc_timestamp * 1000);
            lastDateTime = date.toLocaleString();
        }
        document.getElementById('history-last-reading').textContent = lastDateTime;
        
        // Estado ESP32
        const healthScore = lastReading.health_score || 0;
        let status = 'Desconocido';
        if (healthScore >= 80) status = 'Excelente';
        else if (healthScore >= 60) status = 'Bueno';
        else if (healthScore >= 40) status = 'Regular';
        else if (healthScore > 0) status = 'Malo';
        
        document.getElementById('history-esp32-status').textContent = `${status} (${healthScore}%)`;
        
        // Actualizar tabla
        this.updateCompleteDataTable();
    }

    updateCompleteDataTable() {
        const tbody = document.getElementById('complete-data-table-body');
        
        if (this.data.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="11" class="no-data">
                        Presiona "Descargar Datos del ESP32" para obtener las lecturas
                    </td>
                </tr>
            `;
            return;
        }
        
        tbody.innerHTML = '';
        
        const maxRecords = Math.min(200, this.data.length);
        const reversedData = [...this.data].reverse().slice(0, maxRecords);
        
        reversedData.forEach(item => {
            const row = document.createElement('tr');
            
            //Fecha/Hora RTC sin offset adicional
            let rtcDateTimeStr = '-';
            if (item.rtc_datetime && item.rtc_datetime !== "No disponible") {
                rtcDateTimeStr = item.rtc_datetime;
            } 
            else if (item.rtc_timestamp && item.rtc_timestamp > 1609459200) {
                // No aplicar offset, usar directamente el timestamp
                const rtcDate = new Date(item.rtc_timestamp * 1000);
                rtcDateTimeStr = rtcDate.toLocaleString('es-CO', {
                    year: 'numeric',
                    month: '2-digit',
                    day: '2-digit',
                    hour: '2-digit',
                    minute: '2-digit',
                    second: '2-digit',
                    timeZone: 'America/Bogota'
                });
            }
            
            const webDate = new Date(item.timestamp_web);
            const webTimeStr = webDate.toLocaleTimeString();
            
            row.innerHTML = `
                <td class="rtc-timestamp">${rtcDateTimeStr}</td>
                <td>${webTimeStr}</td>
                <td>#${item.reading_number || '-'}</td>
                <td>${item.temperature.toFixed(1)}</td>
                <td>${item.ph > 0 ? item.ph.toFixed(2) : '-'}</td>
                <td>${item.turbidity >= 0 ? item.turbidity.toFixed(1) : '-'}</td>
                <td>${item.tds >= 0 ? item.tds.toFixed(0) : '-'}</td>
                <td>${item.ec >= 0 ? item.ec.toFixed(1) : '-'}</td>
                <td>${item.rssi || '-'} dBm</td>
                <td>${item.health_score || '-'}%</td>
                <td>${item.valid ? ' Válida' : ' Inválida'}</td>
            `;
            tbody.appendChild(row);
        });
        
        // Mostrar información de cuántos registros se están mostrando
        if (this.data.length > maxRecords) {
            const infoRow = document.createElement('tr');
            infoRow.innerHTML = `
                <td colspan="11" style="text-align: center; font-style: italic; color: #666; padding: 10px;">
                     Mostrando los últimos ${maxRecords} de ${this.data.length} registros totales
                </td>
            `;
            tbody.appendChild(infoRow);
        }
    }



    
    requestData() {
        if (!this.esp32Connected || this.downloadInProgress) {
            return;
        }
        
        const downloadBtn = document.getElementById('download-btn');
        downloadBtn.disabled = true;
        downloadBtn.classList.add('loading');
        downloadBtn.textContent = ' Descargando...';
        
        this.ws.send(JSON.stringify({
            type: 'request_data',
            action: 'download_all'
        }));
        
        this.updateDownloadStatus('Solicitando datos al ESP32...', 'loading');
    }
    
    addSensorData(data) {
        data.timestamp_web = new Date().toISOString();
        this.data.push(data);
        
        // Actualizar interfaz
        this.updateCurrentValues(data);
        if (this.activeTab === 'complete-history') {
            this.updateCompleteHistoryTab();
        }
        this.updateSystemInfo(data);
        this.updateLastUpdate();
        this.updateRTCStatus(data);
        
        // Actualizar pestañas
        this.updateAllLargeCharts();
        this.updateAllSensorTables();
        this.updateAllSensorSummaries();
        
        if (this.downloadInProgress) {
            this.updateDownloadStatus(
                `Recibiendo... ${this.data.length} lecturas`, 
                'loading'
            );
        }
    }
    
    updateRTCStatus(data) {
        const rtcStatusEl = document.getElementById('rtc-status');

        if (data.rtc_datetime && data.rtc_datetime !== "No disponible") {
            rtcStatusEl.textContent = `Funcionando - ${data.rtc_datetime}`;
            rtcStatusEl.style.color = '#27ae60';
        } else if (data.rtc_timestamp && data.rtc_timestamp > 1609459200) {
            const date = new Date(data.rtc_timestamp * 1000);
            rtcStatusEl.textContent = `Funcionando - ${date.toLocaleString()}`;
            rtcStatusEl.style.color = '#27ae60';
        } else {
            rtcStatusEl.textContent = 'No disponible - Usando tiempo relativo';
            rtcStatusEl.style.color = '#e74c3c';
        }
    }
    
    finalizeDataDisplay() {
        const downloadBtn = document.getElementById('download-btn');
        downloadBtn.disabled = false;
        downloadBtn.classList.remove('loading');
        downloadBtn.textContent = '📥 Descargar Datos del ESP32';
        
        this.checkAllAlerts();
        
        if (this.data.length > 0) {
            const lastReading = this.data[this.data.length - 1];
            document.getElementById('esp32-health').textContent = 
                `💊 Salud ESP32: ${lastReading.health_score || '--'}%`;
        }
    }
    
    updateDownloadStatus(message, type = '') {
        const statusEl = document.getElementById('download-status');
        statusEl.textContent = message;
        statusEl.className = `download-status ${type}`;
    }
    
    updateConnectionStatus(connected) {
        const statusElement = document.getElementById('connection-status');
        const dot = statusElement.querySelector('.status-dot');
        const text = statusElement.querySelector('span:last-child');
        
        if (connected) {
            dot.className = 'status-dot online';
            text.textContent = 'ESP32 Conectado';
        } else {
            dot.className = 'status-dot offline';
            text.textContent = 'ESP32 Desconectado';
        }
    }
    
    updateCurrentValues(data) {
        // Temperatura
        document.getElementById('temp-value').textContent = `${data.temperature.toFixed(1)}°C`;
        document.getElementById('temp-status').textContent = this.getTempStatus(data.temperature);
        document.getElementById('temp-status').className = `value-status ${this.getTempStatusClass(data.temperature)}`;
        
        // pH
        if (data.ph > 0) {
            document.getElementById('ph-value').textContent = data.ph.toFixed(2);
            document.getElementById('ph-status').textContent = this.getPhStatus(data.ph);
            document.getElementById('ph-status').className = `value-status ${this.getPhStatusClass(data.ph)}`;
        }
        
        // Turbidez
        if (data.turbidity >= 0) {
            document.getElementById('turbidity-value').textContent = `${data.turbidity.toFixed(1)} NTU`;
            document.getElementById('turbidity-status').textContent = this.getTurbidityStatus(data.turbidity);
            document.getElementById('turbidity-status').className = `value-status ${this.getTurbidityStatusClass(data.turbidity)}`;
        }
        
        // TDS
        if (data.tds >= 0) {
            document.getElementById('tds-value').textContent = `${data.tds.toFixed(0)} ppm`;
            document.getElementById('tds-status').textContent = this.getTdsStatus(data.tds);
            document.getElementById('tds-status').className = `value-status ${this.getTdsStatusClass(data.tds)}`;
        }

        //Ec
        if (data.ec >= 0) {
            document.getElementById('ec-value').textContent = `${data.ec.toFixed(1)} µS/cm`;
            document.getElementById('ec-status').textContent = this.getEcStatus(data.ec);
            document.getElementById('ec-status').className = `value-status ${this.getEcStatusClass(data.ec)}`;
        }
    }
    
    // Funciones de estado
    getTempStatus(temp) {
        if (temp < 0 || temp > 35) return 'Fuera de rango';
        if (temp < 5 || temp > 30) return 'Advertencia';
        return 'Normal';
    }
    
    getTempStatusClass(temp) {
        if (temp < 0 || temp > 35) return 'danger';
        if (temp < 5 || temp > 30) return 'warning';
        return 'normal';
    }
    
    getPhStatus(ph) {
        if (ph < 2 || ph > 13) return 'Aceptable';
        if (ph < 3 || ph > 12) return 'Optimo';
        return 'Óptimo';
    }
    
    getPhStatusClass(ph) {
        if (ph < 2 || ph > 13) return 'danger';
        if (ph < 3 || ph > 12) return 'warning';
        return 'normal';
    }
    
    getTurbidityStatus(turbidity) {
        if (turbidity > 5) return 'Alta';
        if (turbidity > 1) return 'Moderada';
        return 'Baja';
    }
    
    getTurbidityStatusClass(turbidity) {
        if (turbidity > 5) return 'danger';
        if (turbidity > 1) return 'warning';
        return 'normal';
    }
    
    getTdsStatus(tds) {
        if (tds > 1000) return 'Muy alto';
        if (tds > 500) return 'Alto';
        return 'Aceptable';
    }
    
    getTdsStatusClass(tds) {
        if (tds > 1000) return 'danger';
        if (tds > 500) return 'warning';
        return 'normal';
    }
    getEcStatus(ec) {
        if (ec > 2000) return 'Muy alta';
        if (ec > 1000) return 'Alta';
        return 'Aceptable';
    }
    
    getEcStatusClass(ec) {
        if (ec > 2000) return 'danger';
        if (ec > 1000) return 'warning';
        return 'normal';
    }
    
    
    
    createLargeCharts() {
        const largeChartOptions = {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: true,
                    position: 'top'
                },
                title: {
                    display: true,
                    font: {
                        size: 16,
                        weight: 'bold'
                    }
                }
            },
            scales: {
                y: {
                    beginAtZero: false,
                    grid: {
                        color: 'rgba(0,0,0,0.1)'
                    }
                },
                x: {
                    display: true,
                    grid: {
                        color: 'rgba(0,0,0,0.1)'
                    },
                    ticks: {
                        maxRotation: 45,
                        minRotation: 45
                    }
                }
            }
        };
        
        
        this.largeCharts.temperature = new Chart(document.getElementById('tempChartLarge'), {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Temperatura (°C)',
                    data: [],
                    borderColor: '#e74c3c',
                    backgroundColor: 'rgba(231, 76, 60, 0.1)',
                    fill: true,
                    tension: 0.4,
                    pointRadius: 4,
                    pointHoverRadius: 6
                }]
            },
            options: {
                ...largeChartOptions,
                plugins: {
                    ...largeChartOptions.plugins,
                    title: {
                        ...largeChartOptions.plugins.title,
                        text: 'Histórico de Temperatura'
                    }
                }
            }
        });
        
        this.largeCharts.ph = new Chart(document.getElementById('phChartLarge'), {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'pH',
                    data: [],
                    borderColor: '#3498db',
                    backgroundColor: 'rgba(52, 152, 219, 0.1)',
                    fill: true,
                    tension: 0.4,
                    pointRadius: 4,
                    pointHoverRadius: 6
                }]
            },
            options: {
                ...largeChartOptions,
                plugins: {
                    ...largeChartOptions.plugins,
                    title: {
                        ...largeChartOptions.plugins.title,
                        text: 'Histórico de pH'
                    }
                }
            }
        });
        
        this.largeCharts.turbidity = new Chart(document.getElementById('turbidityChartLarge'), {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Turbidez (NTU)',
                    data: [],
                    borderColor: '#95a5a6',
                    backgroundColor: 'rgba(149, 165, 166, 0.1)',
                    fill: true,
                    tension: 0.4,
                    pointRadius: 4,
                    pointHoverRadius: 6
                }]
            },
            options: {
                ...largeChartOptions,
                plugins: {
                    ...largeChartOptions.plugins,
                    title: {
                        ...largeChartOptions.plugins.title,
                        text: 'Histórico de Turbidez'
                    }
                }
            }
        });
        
        this.largeCharts.tds = new Chart(document.getElementById('tdsChartLarge'), {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'TDS (ppm)',
                    data: [],
                    borderColor: '#27ae60',
                    backgroundColor: 'rgba(39, 174, 96, 0.1)',
                    fill: true,
                    tension: 0.4,
                    pointRadius: 4,
                    pointHoverRadius: 6
                }]
            },
            options: {
                ...largeChartOptions,
                plugins: {
                    ...largeChartOptions.plugins,
                    title: {
                        ...largeChartOptions.plugins.title,
                        text: 'Histórico de TDS'
                    }
                }
            }
        });
    }
    
    
    
    updateAllLargeCharts() {
        ['temperature', 'ph', 'turbidity', 'tds'].forEach(sensor => {
            this.updateLargeChart(sensor);
        });
    }
    
    updateLargeChart(sensor) {
        if (!this.largeCharts[sensor] || this.data.length === 0) return;
        
        const labels = this.data.map((d, i) => {
            // CORRECCIÓN: Manejo correcto de timestamps
            if (d.rtc_datetime && d.rtc_datetime !== "No disponible") {
                return d.rtc_datetime;
            }
            else if (d.rtc_timestamp && d.rtc_timestamp > 1609459200) {
                // No aplicar offset adicional, el timestamp ya está en hora local
                const date = new Date(d.rtc_timestamp * 1000);
                return date.toLocaleString('es-CO', {
                    timeZone: 'America/Bogota'
                });
            } 
            else {
                const date = new Date(d.timestamp_web);
                return date.toLocaleString();
            }
        });
        
        let data = [];
        switch(sensor) {
            case 'temperature':
                data = this.data.map(d => d.temperature);
                break;
            case 'ph':
                data = this.data.map(d => d.ph > 0 ? d.ph : null);
                break;
            case 'turbidity':
                data = this.data.map(d => d.turbidity >= 0 ? d.turbidity : null);
                break;
            case 'tds':
                data = this.data.map(d => d.tds >= 0 ? d.tds : null);
                break;
        }
        
        this.largeCharts[sensor].data.labels = labels;
        this.largeCharts[sensor].data.datasets[0].data = data;
        this.largeCharts[sensor].update('none');
    }
    
    updateAllSensorTables() {
        ['temperature', 'ph', 'turbidity', 'tds'].forEach(sensor => {
            this.updateSensorTable(sensor);
        });
    }
    
    updateSensorTable(sensor) {
        const tbody = document.getElementById(`${sensor}-data-body`);
        
        if (this.data.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="4" class="no-data">
                        Presiona "Descargar Datos del ESP32" para ver el historial
                    </td>
                </tr>
            `;
            return;
        }
        
        tbody.innerHTML = '';
        
        const maxRecords = Math.min(200, this.data.length);
        const reversedData = [...this.data].reverse().slice(0, maxRecords);
        
        reversedData.forEach(item => {
            let value, status, unit;
            
            switch(sensor) {
                case 'temperature':
                    value = item.temperature.toFixed(1);
                    status = this.getTempStatus(item.temperature);
                    unit = '°C';
                    break;
                case 'ph':
                    if (item.ph <= 0) return; 
                    value = item.ph.toFixed(2);
                    status = this.getPhStatus(item.ph);
                    unit = '';
                    break;
                case 'turbidity':
                    if (item.turbidity < 0) return; 
                    value = item.turbidity.toFixed(1);
                    status = this.getTurbidityStatus(item.turbidity);
                    unit = 'NTU';
                    break;
                case 'tds':
                    if (this.tdsDisplayMode === 'ec') {
                        if (item.ec < 0) return;
                        value = item.ec.toFixed(1);
                        status = this.getEcStatus(item.ec);
                        unit = 'µS/cm';
                    } else {
                        if (item.tds < 0) return;
                        value = item.tds.toFixed(0);
                        status = this.getTdsStatus(item.tds);
                        unit = 'ppm';
                    }
                    break;
            }
            
            // CORRECCIÓN: Fecha/Hora RTC sin offset adicional
            let rtcDateTimeStr = '-';
            if (item.rtc_datetime && item.rtc_datetime !== "No disponible") {
                rtcDateTimeStr = item.rtc_datetime;
            } 
            else if (item.rtc_timestamp && item.rtc_timestamp > 1609459200) {
                // No aplicar offset, usar directamente el timestamp
                const rtcDate = new Date(item.rtc_timestamp * 1000);
                rtcDateTimeStr = rtcDate.toLocaleString('es-CO', {
                    year: 'numeric',
                    month: '2-digit',
                    day: '2-digit',
                    hour: '2-digit',
                    minute: '2-digit',
                    second: '2-digit',
                    timeZone: 'America/Bogota'
                });
            }
            
            const row = document.createElement('tr');
            row.innerHTML = `
                <td class="rtc-timestamp">${rtcDateTimeStr}</td>
                <td>${value} ${unit}</td>
                <td><span class="value-status ${this.getStatusClass(sensor, parseFloat(value))}">${status}</span></td>
                <td>#${item.reading_number || '-'}</td>
            `;
            tbody.appendChild(row);
        });
        
        // Mostrar información de cuántos registros se están mostrando
        if (this.data.length > maxRecords) {
            const infoRow = document.createElement('tr');
            infoRow.innerHTML = `
                <td colspan="4" style="text-align: center; font-style: italic; color: #666; padding: 10px;">
                    Mostrando los últimos ${maxRecords} de ${this.data.length} registros totales
                </td>
            `;
            tbody.appendChild(infoRow);
        }
    }
    
    getStatusClass(sensor, value) {
        switch(sensor) {
            case 'temperature':
                return this.getTempStatusClass(value);
            case 'ph':
                return this.getPhStatusClass(value);
            case 'turbidity':
                return this.getTurbidityStatusClass(value);
            case 'tds':
                return this.getTdsStatusClass(value);
        }
    }
    
    updateAllSensorSummaries() {
        ['temperature', 'ph', 'turbidity', 'tds'].forEach(sensor => {
            this.updateSensorSummary(sensor);
        });
    }
    
    updateSensorSummary(sensor) {
        if (this.data.length === 0) return;
        
        let values = [];
        let unit = '';
        
        switch(sensor) {
            case 'temperature':
                values = this.data.map(d => d.temperature).filter(v => !isNaN(v));
                unit = '°C';
                break;
            case 'ph':
                values = this.data.map(d => d.ph).filter(v => v > 0 && !isNaN(v));
                unit = '';
                break;
            case 'turbidity':
                values = this.data.map(d => d.turbidity).filter(v => v >= 0 && !isNaN(v));
                unit = ' NTU';
                break;
            case 'tds':
                values = this.data.map(d => d.tds).filter(v => v >= 0 && !isNaN(v));
                unit = ' ppm';
                break;
        }
        
        if (values.length === 0) {
            document.getElementById(`${sensor}-last-value`).textContent = `--${unit}`;
            document.getElementById(`${sensor}-average`).textContent = `--${unit}`;
            document.getElementById(`${sensor}-range`).textContent = `-- / --${unit}`;
            return;
        }
        
        const lastValue = values[values.length - 1];
        const average = values.reduce((a, b) => a + b, 0) / values.length;
        const min = Math.min(...values);
        const max = Math.max(...values);
        
        const precision = sensor === 'tds' ? 0 : (sensor === 'ph' ? 2 : 1);
        
        document.getElementById(`${sensor}-last-value`).textContent = 
            `${lastValue.toFixed(precision)}${unit}`;
        document.getElementById(`${sensor}-average`).textContent = 
            `${average.toFixed(precision)}${unit}`;
        document.getElementById(`${sensor}-range`).textContent = 
            `${min.toFixed(precision)} / ${max.toFixed(precision)}${unit}`;
    }
    
    
    updateSystemInfo(data) {
        document.getElementById('total-readings').textContent = this.data.length;
        document.getElementById('esp32-memory').textContent = `${data.free_heap || '---'} bytes`;
    }
    
    updateLastUpdate() {
        const now = new Date();
        document.getElementById('last-update').textContent = 
            `Última actualización: ${now.toLocaleTimeString()}`;
    }
    
    updateServerStartTime() {
        const now = new Date();
        document.getElementById('server-start-time').textContent = now.toLocaleTimeString();
    }
    
    checkAllAlerts() {
        const alerts = [];
        
        this.data.forEach((data, index) => {
            if (data.temperature < 0 || data.temperature > 35) {
                alerts.push(` Lectura #${data.reading_number}: Temperatura fuera de rango (${data.temperature.toFixed(1)}°C)`);
            }
            
            if (data.ph > 0 && (data.ph < 0 || data.ph > 14)) {
                alerts.push(` Lectura #${data.reading_number}: pH fuera de rango (${data.ph.toFixed(2)})`);
            }
            
            if (data.health_score && data.health_score < 70) {
                alerts.push(` Lectura #${data.reading_number}: Salud del sistema baja (${data.health_score}%)`);
            }
            
            if (!data.rtc_timestamp || data.rtc_timestamp < 1609459200) {
                if (index === this.data.length - 1) {
                    alerts.push(` RTC no disponible - Usando timestamps relativos`);
                }
            }
        });
        
        this.updateAlerts(alerts.slice(-5));
    }
    
    updateAlerts(alerts) {
        const container = document.getElementById('alerts-container');
        const list = document.getElementById('alerts-list');
        
        if (alerts.length > 0) {
            container.classList.remove('hidden');
            list.innerHTML = alerts.map(alert => 
                `<div class="alert-item">${alert}</div>`
            ).join('');
        } else {
            container.classList.add('hidden');
        }
    }

    confirmDeleteSession() {
        if (!this.currentSession) return;
        
        // Crear modal de confirmación
        const modal = document.createElement('div');
        modal.className = 'confirmation-modal';
        modal.innerHTML = `
            <div class="modal-content">
                <h3>🗑️ Confirmar Eliminación</h3>
                <p>¿Estás seguro de que quieres eliminar esta sesión de medición?</p>
                <p><strong>Esta acción no se puede deshacer.</strong></p>
                <div class="modal-buttons">
                    <button class="modal-button cancel" id="cancel-delete-btn">
                        Cancelar
                    </button>
                    <button class="modal-button confirm" id="confirm-delete-btn">
                        Sí, Eliminar
                    </button>
                </div>
            </div>
        `;
        
        document.body.appendChild(modal);
        
        // Configurar eventos del modal
        document.getElementById('cancel-delete-btn').addEventListener('click', () => {
            modal.remove();
        });
        
        document.getElementById('confirm-delete-btn').addEventListener('click', () => {
            this.deleteCurrentSession();
            modal.remove();
        });
        
        // Cerrar modal al hacer click fuera
        modal.addEventListener('click', (e) => {
            if (e.target === modal) {
                modal.remove();
            }
        });
    }
    
    deleteCurrentSession() {
        if (!this.currentSession || !this.ws) {
            console.error(' No hay sesión actual o WebSocket no conectado');
            console.log('currentSession:', this.currentSession);
            console.log('WebSocket state:', this.ws ? this.ws.readyState : 'null');
            return;
        }
        
        console.log('🗑️ Eliminando sesión:', this.currentSession.session_id);
        console.log(' WebSocket ready state:', this.ws.readyState);
        
        // Deshabilitar botón mientras se procesa
        const deleteBtn = document.getElementById('delete-session-btn');
        deleteBtn.disabled = true;
        deleteBtn.textContent = '⏳ Eliminando...';
        
        // Enviar solicitud al servidor
        const message = {
            type: 'delete_session',
            session_id: this.currentSession.session_id
        };
        
        console.log(' Enviando mensaje:', message);
        
        try {
            this.ws.send(JSON.stringify(message));
            console.log(' Mensaje enviado al servidor');
        } catch (error) {
            console.error(' Error enviando mensaje:', error);
            deleteBtn.disabled = false;
            deleteBtn.textContent = '🗑️ Eliminar Sesión';
        }
    }
    
    handleSessionDeleted(data) {
        const deleteBtn = document.getElementById('delete-session-btn');
        
        if (data.success) {
            console.log(' Sesión eliminada exitosamente');
            
            // Mostrar mensaje de éxito
            deleteBtn.textContent = ' Eliminada';
            deleteBtn.style.background = '#27ae60';
            
            
            setTimeout(() => {
                this.showSessionsList();
                deleteBtn.disabled = false;
                deleteBtn.textContent = '🗑️ Eliminar Sesión';
                deleteBtn.style.background = '';
            }, 1500);
            
        } else {
            console.error(' Error eliminando sesión:', data.message);
            
            // Mostrar error
            deleteBtn.textContent = ' Error';
            deleteBtn.style.background = '#e74c3c';
            
            // Restaurar botón 
            setTimeout(() => {
                deleteBtn.disabled = false;
                deleteBtn.textContent = '🗑️ Eliminar Sesión';
                deleteBtn.style.background = '';
            }, 2000);
        }
    }
    
    



    // ═══════ FUNCIONES DE CALIBRACIÓN ═══════

    requestCurrentCalibration() {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
            this.addCalibrationLog('✗ No hay conexión WebSocket', 'error');
            return;
        }
        
        const command = {
            action: "get_calibration"
        };
        
        this.addCalibrationLog('📊 Solicitando valores actuales...', 'info');
        this.ws.send(JSON.stringify(command));
    }

    sendCalibrationCommand(command, message) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
            this.addCalibrationLog('✗ No hay conexión WebSocket', 'error');
            return;
        }
        
        if (!this.esp32Connected) {
            this.addCalibrationLog('✗ ESP32 no conectado', 'error');
            return;
        }
        
        this.addCalibrationLog(message, 'info');
        this.ws.send(JSON.stringify(command));
        
        // Log del comando enviado
        this.addCalibrationLog('→ ' + JSON.stringify(command), 'info');
    }

    calibratePH() {
        const offset = parseFloat(document.getElementById('calib-ph-offset').value);
        const slope = parseFloat(document.getElementById('calib-ph-slope').value);
        
        if (isNaN(offset) || isNaN(slope)) {
            this.addCalibrationLog('⚠ Valores de pH inválidos', 'error');
            return;
        }
        
        if (offset < -5 || offset > 5) {
            this.addCalibrationLog('⚠ pH Offset fuera de rango (-5 a +5)', 'error');
            return;
        }
        
        if (slope < -10 || slope > 10) {
            this.addCalibrationLog('⚠ pH Slope fuera de rango (-10 a +10)', 'error');
            return;
        }
        
        const command = {
            action: "calibrate",
            ph_offset: offset,
            ph_slope: slope
        };
        
        this.sendCalibrationCommand(command, '🔧 Calibrando sensor pH...');
    }

    calibrateTDS() {
        const kvalue = parseFloat(document.getElementById('calib-tds-kvalue').value);
        const voffset = parseFloat(document.getElementById('calib-tds-voffset').value);
        
        if (isNaN(kvalue) || isNaN(voffset)) {
            this.addCalibrationLog('⚠ Valores de TDS inválidos', 'error');
            return;
        }
        
        if (kvalue < 0.1 || kvalue > 5.0) {
            this.addCalibrationLog('⚠ TDS K-Value fuera de rango (0.1 a 5.0)', 'error');
            return;
        }
        
        if (voffset < -1.0 || voffset > 1.0) {
            this.addCalibrationLog('⚠ TDS V-Offset fuera de rango (-1.0 a +1.0)', 'error');
            return;
        }
        
        const command = {
            action: "calibrate",
            tds_kvalue: kvalue,
            tds_voffset: voffset
        };
        
        this.sendCalibrationCommand(command, '🔧 Calibrando sensor TDS...');
    }

    calibrateTurbidity() {
        const a = parseFloat(document.getElementById('calib-turb-a').value);
        const b = parseFloat(document.getElementById('calib-turb-b').value);
        const c = parseFloat(document.getElementById('calib-turb-c').value);
        const d = parseFloat(document.getElementById('calib-turb-d').value);
        
        if (isNaN(a) || isNaN(b) || isNaN(c) || isNaN(d)) {
            this.addCalibrationLog('⚠ Coeficientes de turbidez inválidos', 'error');
            return;
        }
        
        const command = {
            action: "calibrate",
            turb_coeff_a: a,
            turb_coeff_b: b,
            turb_coeff_c: c,
            turb_coeff_d: d
        };
        
        this.sendCalibrationCommand(command, '🔧 Calibrando sensor de turbidez...');
    }

    restoreDefaultsCalibration() {
        if (!confirm('¿Restaurar valores de calibración por defecto? Esta acción no se puede deshacer.')) {
            return;
        }
        
        const command = {
            action: "calibrate",
            restore_defaults: true
        };
        
        this.sendCalibrationCommand(command, '🔄 Restaurando valores por defecto...');
    }

    updateCalibrationDisplay(calib) {
        // pH
        document.getElementById('calib-ph-offset-current').textContent = calib.ph_offset.toFixed(2);
        document.getElementById('calib-ph-slope-current').textContent = calib.ph_slope.toFixed(2);
        document.getElementById('calib-ph-offset').value = calib.ph_offset.toFixed(2);
        document.getElementById('calib-ph-slope').value = calib.ph_slope.toFixed(2);
        
        // TDS
        document.getElementById('calib-tds-kvalue-current').textContent = calib.tds_kvalue.toFixed(6);
        document.getElementById('calib-tds-voffset-current').textContent = calib.tds_voffset.toFixed(6);
        document.getElementById('calib-tds-kvalue').value = calib.tds_kvalue.toFixed(6);
        document.getElementById('calib-tds-voffset').value = calib.tds_voffset.toFixed(6);
        
        // Turbidez
        document.getElementById('calib-turb-a-current').textContent = calib.turb_coeff_a.toFixed(1);
        document.getElementById('calib-turb-b-current').textContent = calib.turb_coeff_b.toFixed(1);
        document.getElementById('calib-turb-c-current').textContent = calib.turb_coeff_c.toFixed(1);
        document.getElementById('calib-turb-d-current').textContent = calib.turb_coeff_d.toFixed(1);
        document.getElementById('calib-turb-a').value = calib.turb_coeff_a.toFixed(1);
        document.getElementById('calib-turb-b').value = calib.turb_coeff_b.toFixed(1);
        document.getElementById('calib-turb-c').value = calib.turb_coeff_c.toFixed(1);
        document.getElementById('calib-turb-d').value = calib.turb_coeff_d.toFixed(1);
        
        // Metadata
        if (calib.last_update) {
            const date = new Date(calib.last_update);
            document.getElementById('calib-last-update').textContent = date.toLocaleString('es-CO');
        }
        
        if (calib.update_count !== undefined) {
            document.getElementById('calib-update-count').textContent = calib.update_count;
        }
        
        if (calib.crc) {
            document.getElementById('calib-crc').textContent = '0x' + calib.crc.toString(16).toUpperCase();
        }
    }

    addCalibrationLog(message, type = 'info') {
        const logContent = document.getElementById('calibration-log-content');
        if (!logContent) return;
        
        const timestamp = new Date().toLocaleTimeString('es-CO');
        const logEntry = document.createElement('div');
        logEntry.className = 'log-entry log-' + type;
        logEntry.innerHTML = '<span class="log-time">[' + timestamp + ']</span> ' + message;
        
        logContent.appendChild(logEntry);
        logContent.scrollTop = logContent.scrollHeight;
        
        // Limitar a 50 entradas
        while (logContent.children.length > 50) {
            logContent.removeChild(logContent.firstChild);
        }
    }

    // ═══════ FIN FUNCIONES DE CALIBRACIÓN ═══════

}


document.addEventListener('DOMContentLoaded', () => {
    new WaterMonitor();
});