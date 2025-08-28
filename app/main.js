// --- LocalStorage seguro ---
const store = {
  get: (k, f = null) => { try { return JSON.parse(localStorage.getItem(k)) ?? f; } catch { return f; } },
  set: (k, v) => localStorage.setItem(k, JSON.stringify(v)),
  remove: (k) => localStorage.removeItem(k)
};

// --- Selectores ---
const elTemp = () => document.getElementById("temp");
const elHum  = () => document.getElementById("hum");
const elLast = () => document.getElementById("lastUpdate");

// --- Tema ---
function applyTheme(theme) {
  document.documentElement.setAttribute("data-theme", theme);
  document.getElementById("themeSwitch").checked = theme === "dark";
  store.set("theme", theme);
}
function initTheme() {
  const saved = store.get("theme");
  applyTheme(saved || (window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light"));
  document.getElementById("themeSwitch").addEventListener("change", e =>
    applyTheme(e.target.checked ? "dark" : "light")
  );
}

// --- Cargar última lectura ---
function loadLastReading() {
  const last = store.get("dht22:last");
  if (last) {
    elTemp().textContent = last.temp.toFixed(1);
    elHum().textContent  = last.hum.toFixed(1);
    elLast().textContent = `Última actualización: ${new Date(last.ts).toLocaleString()}`;
  }
}

// --- Guardar lectura ---
function saveReading(temp, hum) {
  const entry = { temp, hum, ts: Date.now() };
  store.set("dht22:last", entry);
  const hist = store.get("dht22:history", []);
  hist.push(entry);
  if (hist.length > 100) hist.shift();
  store.set("dht22:history", hist);
}

// --- Simulación ---
function simulateReading() {
  return {
    temp: +(20 + Math.random() * 10).toFixed(1),
    hum: +(40 + Math.random() * 20).toFixed(1)
  };
}

// --- Mostrar gráfico y tabla ---
function showChart() {
  const history = store.get("dht22:history", []);
  if (history.length === 0) {
    Swal.fire("Sin datos", "Aún no hay lecturas guardadas.", "info");
    return;
  }

  const labels = history.map(r => new Date(r.ts).toLocaleTimeString());
  const temps  = history.map(r => r.temp);
  const hums   = history.map(r => r.hum);

  Swal.fire({
    title: "Historial de lecturas",
    html: `
      <canvas id="chart" style="max-width:100%;height:200px;"></canvas>
      <hr>
      <table border="1" style="width:100%;border-collapse:collapse;font-size:12px;">
        <tr><th>Fecha</th><th>Temp (°C)</th><th>Hum (%)</th></tr>
        ${history.map(r => `<tr>
            <td>${new Date(r.ts).toLocaleString()}</td>
            <td>${r.temp}</td>
            <td>${r.hum}</td>
          </tr>`).join("")}
      </table>
    `,
    width: 800,
    didOpen: () => {
      new Chart(document.getElementById("chart"), {
        type: "line",
        data: {
          labels,
          datasets: [
            { label: "Temperatura (°C)", data: temps, borderColor: "red", fill: false },
            { label: "Humedad (%)", data: hums, borderColor: "blue", fill: false }
          ]
        }
      });
    }
  });
}

// --- Init ---
document.addEventListener("DOMContentLoaded", () => {
  initTheme();
  loadLastReading();

  document.getElementById("btnUpdate").addEventListener("click", () => {
    const data = simulateReading();
    elTemp().textContent = data.temp.toFixed(1);
    elHum().textContent  = data.hum.toFixed(1);
    elLast().textContent = `Última actualización: ${new Date().toLocaleString()}`;
    saveReading(data.temp, data.hum);
  });

  document.getElementById("btnClear").addEventListener("click", () => {
    store.remove("dht22:last");
    store.remove("dht22:history");
    loadLastReading();
  });

  document.getElementById("btnChart").addEventListener("click", showChart);
});
