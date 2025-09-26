document.addEventListener('DOMContentLoaded', () => {
    const tempValue = document.getElementById('temp-value');
    const humValue = document.getElementById('hum-value');
    const themeToggle = document.getElementById('theme-toggle-checkbox');
    const actuatorToggle = document.getElementById('actuator-toggle');

    // Load theme preference from localStorage
    const savedTheme = localStorage.getItem('theme');
    if (savedTheme === 'dark') {
        document.body.classList.add('dark-mode');
        themeToggle.checked = true;
    }

    // Toggle theme
    themeToggle.addEventListener('change', () => {
        if (themeToggle.checked) {
            document.body.classList.add('dark-mode');
            localStorage.setItem('theme', 'dark');
        } else {
            document.body.classList.remove('dark-mode');
            localStorage.setItem('theme', 'light');
        }
    });

    // Function to fetch and update sensor data
    const fetchSensorData = async () => {
        try {
            const response = await fetch('/api/latest');
            const data = await response.json();
            if (data.error) {
                throw new Error(data.error);
            }
            tempValue.textContent = `${data.temp.toFixed(1)} °C`;
            humValue.textContent = `${data.hum.toFixed(1)} %`;
        } catch (error) {
            console.error('Error fetching sensor data:', error);
            tempValue.textContent = 'Error';
            humValue.textContent = 'Error';
        }
    };

    // Initial fetch and set interval
    fetchSensorData();
    setInterval(fetchSensorData, 30000); // Update every 30 seconds

    // --- Funcionalidad del Actuador ---

    // Variable para almacenar el historial de cambios del actuador
    const actuatorHistory = JSON.parse(localStorage.getItem('actuatorHistory')) || [];

    // Función para mostrar el historial del actuador
    window.showActuatorHistory = () => {
        let historyContent = '<div class="history-table-container">';
        if (actuatorHistory.length === 0) {
            historyContent += '<p>No hay registros de cambios de estado.</p>';
        } else {
            historyContent += '<table class="history-table"><thead><tr><th>Fecha y Hora</th><th>Estado</th></tr></thead><tbody>';
            actuatorHistory.forEach(record => {
                historyContent += `<tr><td>${record.timestamp}</td><td>${record.state}</td></tr>`;
            });
            historyContent += '</tbody></table>';
        }
        historyContent += '</div>';

        Swal.fire({
            title: 'Historial del Actuador',
            html: historyContent,
            showConfirmButton: true,
            confirmButtonText: 'Cerrar',
            didOpen: () => {
                const tableContainer = document.querySelector('.history-table-container');
                if (tableContainer) {
                    // Actualizar el color de la tabla si el tema es oscuro
                    if (document.body.classList.contains('dark-mode')) {
                         document.querySelector('.swal2-title').style.color = '#e0e0e0';
                         const ths = document.querySelectorAll('.history-table th');
                         ths.forEach(th => th.style.backgroundColor = '#444');
                         const tds = document.querySelectorAll('.history-table td');
                         tds.forEach(td => td.style.color = '#e0e0e0');
                    }
                }
            }
        });
    };

    // Manejar el cambio del switch del actuador
    actuatorToggle.addEventListener('change', async () => {
        const newState = actuatorToggle.checked ? 'ON' : 'OFF';
        const timestamp = new Date().toLocaleString('es-ES', {
            year: 'numeric',
            month: '2-digit',
            day: '2-digit',
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit'
        });

        // Enviar el comando al ESP32 a través de una API
        try {
            const response = await fetch('/api/actuator', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded'
                },
                body: `state=${newState}`
            });
            if (!response.ok) {
                throw new Error('No se pudo enviar el comando al ESP32');
            }

            // Añadir el registro al historial si la llamada a la API fue exitosa
            actuatorHistory.push({
                timestamp: timestamp,
                state: newState
            });

            // Limitar el historial para evitar un crecimiento excesivo (ej. 50 registros)
            if (actuatorHistory.length > 50) {
                actuatorHistory.shift();
            }

            // Guardar el historial en localStorage
            localStorage.setItem('actuatorHistory', JSON.stringify(actuatorHistory));

        } catch (error) {
            console.error('Error al controlar el actuador:', error);
            Swal.fire('Error', 'No se pudo comunicar con el ESP32 para controlar el actuador.', 'error');
            // Revertir el estado del switch si la llamada a la API falla
            actuatorToggle.checked = !actuatorToggle.checked;
        }
    });

    // --- Funcionalidad del Historial de Sensores (ya existente) ---
    const parseCSV = (csv) => {
        const lines = csv.split('\n').filter(line => line.trim() !== '');
        const data = lines.map(line => {
            const [timestamp, temp, hum] = line.split(',');
            const date = new Date(parseInt(timestamp) * 1000); // Unix timestamp
            return { date, temp: parseFloat(temp), hum: parseFloat(hum) };
        });
        return data;
    };

    window.openHistoryPopup = async (type) => {
        try {
            const response = await fetch('/api/history');
            const csvData = await response.text();
            const historicalData = parseCSV(csvData);

            let chartData = [];
            let label = '';
            let unit = '';
            let color = '';

            if (type === 'temperatura') {
                chartData = historicalData.map(d => d.temp);
                label = 'Temperatura (°C)';
                unit = '°C';
                color = 'rgba(255, 99, 132, 1)';
            } else {
                chartData = historicalData.map(d => d.hum);
                label = 'Humedad (%)';
                unit = '%';
                color = 'rgba(54, 162, 235, 1)';
            }

            const dates = historicalData.map(d => d.date.toLocaleString());

            const content = `
                <div>
                    <input id="date-range-picker" type="text" placeholder="Selecciona un rango de fechas">
                    <canvas id="historyChart" width="400" height="200"></canvas>
                </div>
            `;

            Swal.fire({
                title: `Historial de ${type}`,
                html: content,
                showConfirmButton: false,
                width: '80%',
                didOpen: () => {
                    new Chart(document.getElementById('historyChart').getContext('2d'), {
                        type: 'line',
                        data: {
                            labels: dates,
                            datasets: [{
                                label: label,
                                data: chartData,
                                borderColor: color,
                                backgroundColor: 'rgba(0,0,0,0)',
                                borderWidth: 2,
                                tension: 0.1
                            }]
                        },
                        options: {
                            scales: {
                                y: {
                                    beginAtZero: false,
                                    title: {
                                        display: true,
                                        text: unit
                                    }
                                }
                            }
                        }
                    });

                    flatpickr("#date-range-picker", {
                        mode: "range",
                        dateFormat: "Y-m-d",
                        onClose: function(selectedDates, dateStr, instance) {
                            if (selectedDates.length === 2) {
                                const [start, end] = selectedDates;
                                const filteredData = historicalData.filter(d => d.date >= start && d.date <= end);
                                
                                const filteredDates = filteredData.map(d => d.date.toLocaleString());
                                const filteredChartData = filteredData.map(d => d[type === 'temperatura' ? 'temp' : 'hum']);

                                // Update the chart
                                const chart = Chart.getChart("historyChart");
                                chart.data.labels = filteredDates;
                                chart.data.datasets[0].data = filteredChartData;
                                chart.update();
                            }
                        }
                    });
                }
            });

        } catch (error) {
            Swal.fire('Error', 'No se pudo cargar el historial.', 'error');
            console.error('Error fetching history data:', error);
        }
    };
});