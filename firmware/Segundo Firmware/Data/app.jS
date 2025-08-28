// app.js - client-side logic
const apiBase = "/";
const tempEl = document.getElementById("tempValue");
const humEl = document.getElementById("humValue");
const lastUpdateEl = document.getElementById("lastUpdate");
const ipEl = document.getElementById("deviceIp");

const hoursBackInput = document.getElementById("hoursBack");
const stepInput = document.getElementById("step");
const refreshBtn = document.getElementById("refreshBtn");
const showChartBtn = document.getElementById("showChart");
const themeToggle = document.getElementById("themeToggle");

function setTheme(theme){
  if(theme === "dark"){
    document.documentElement.setAttribute("data-theme","dark");
    localStorage.setItem("theme","dark");
  } else {
    document.documentElement.removeAttribute("data-theme");
    localStorage.setItem("theme","light");
  }
}
(function initTheme(){
  const stored = localStorage.getItem("theme") || "light";
  setTheme(stored);
  themeToggle.addEventListener("click", ()=> setTheme(localStorage.getItem("theme")==="dark" ? "light" : "dark"));
})();

async function fetchLatest(){
  try {
    const res = await fetch(apiBase + "api/latest");
    if(!res.ok) throw new Error("HTTP " + res.status);
    const data = await res.json();
    tempEl.textContent = data.temp.toFixed(2) + " °C";
    humEl.textContent = data.hum.toFixed(2) + " %";
    lastUpdateEl.textContent = new Date(data.ts).toLocaleString();
    // Try to show device IP (via server header? Not available). We can show location origin
    ipEl.textContent = location.hostname;
  } catch (err){
    console.error("Error fetchLatest:", err);
    tempEl.textContent = "-- °C";
    humEl.textContent = "-- %";
    lastUpdateEl.textContent = "Error";
  }
}

async function fetchHistory(hours=168, step=60){
  try {
    const res = await fetch(`${apiBase}api/history?hoursBack=${hours}&stepMinutes=${step}`);
    if(!res.ok) throw new Error("HTTP " + res.status);
    return await res.json();
  } catch(err){
    console.error("fetchHistory err", err);
    return [];
  }
}

refreshBtn.addEventListener("click", () => fetchLatest());

showChartBtn.addEventListener("click", async () => {
  const hours = parseInt(hoursBackInput.value) || 168;
  const step = parseInt(stepInput.value) || 60;
  const data = await fetchHistory(hours, step);
  if(!data || data.length === 0){
    Swal.fire("No hay datos", "No se pudieron obtener datos de historial.", "warning");
    return;
  }

  // Build DOM content for SweetAlert with two canvases and date-range
  const content = document.createElement("div");
  content.style.minWidth = "320px";
  content.innerHTML = `
    <div style="display:flex;gap:12px;flex-wrap:wrap;align-items:center;margin-bottom:8px;">
      <label>Desde: <input id="sw-from" type="date"></label>
      <label>Hasta: <input id="sw-to" type="date"></label>
      <button id="sw-apply" class="sw-btn">Aplicar</button>
    </div>
    <div>
      <canvas id="chartTemp" width="600" height="200"></canvas>
      <canvas id="chartHum" width="600" height="200" style="margin-top:12px"></canvas>
    </div>
  `;
  await Swal.fire({
    title: "Gráficos de temperatura y humedad",
    html: content,
    showConfirmButton: false,
    width: "900px",
    didOpen: () => {
      const fromInput = document.getElementById("sw-from");
      const toInput = document.getElementById("sw-to");
      const applyBtn = document.getElementById("sw-apply");

      // set defaults based on data range
      const times = data.map(d => new Date(d.ts));
      const minDate = times[0];
      const maxDate = times[times.length-1];
      fromInput.value = formatDateISO(minDate);
      toInput.value = formatDateISO(maxDate);

      const ctxT = document.getElementById("chartTemp").getContext("2d");
      const ctxH = document.getElementById("chartHum").getContext("2d");

      let chartT = createChart(ctxT, data.map(d => ({x:new Date(d.ts), y:d.temp})), "Temperatura (°C)");
      let chartH = createChart(ctxH, data.map(d => ({x:new Date(d.ts), y:d.hum})), "Humedad (%)");

      applyBtn.addEventListener("click", () => {
        const from = new Date(fromInput.value);
        const to = new Date(toInput.value);
        // include whole day for "to"
        to.setHours(23,59,59,999);
        const filtered = data.filter(d=>{
          const t = new Date(d.ts);
          return t >= from && t <= to;
        });
        // update
        updateChart(chartT, filtered.map(d=>({x:new Date(d.ts), y:d.temp})));
        updateChart(chartH, filtered.map(d=>({x:new Date(d.ts), y:d.hum})));
      });
    }
  });
});

// helpers for charts
function formatDateISO(d){
  const yyyy = d.getFullYear();
  const mm = String(d.getMonth()+1).padStart(2,'0');
  const dd = String(d.getDate()).padStart(2,'0');
  return `${yyyy}-${mm}-${dd}`;
}

function createChart(ctx, dataPoints, label){
  return new Chart(ctx, {
    type: 'line',
    data: {
      datasets: [{
        label: label,
        data: dataPoints,
        tension: 0.25,
        pointRadius: 0,
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: {
        x: {
          type: 'time',
          time: { tooltipFormat: 'yyyy-MM-dd HH:mm', unit: 'hour' },
          title: { display: true, text: 'Fecha' }
        },
        y: {
          title: { display: true, text: label }
        }
      },
      plugins: {
        legend: { display: true }
      }
    }
  });
}

function updateChart(chart, newData){
  chart.data.datasets[0].data = newData;
  chart.update();
}

// initial load
fetchLatest();
// also auto refresh periodically (e.g., 60s)
setInterval(fetchLatest, 60000);

// expose to window for debugging
window._debug_fetchHistory = fetchHistory;
