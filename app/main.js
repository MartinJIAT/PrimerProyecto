// Valores simulados por ejemplo (en un ESP32 real vendrían por WebSocket o fetch)
const tempElement = document.getElementById("tempValue");
const humElement = document.getElementById("humValue");
const toggleSwitch = document.getElementById("themeToggle");

// ---------- FUNCIONES -----------

function actualizarDatos(temp, hum) {
  tempElement.textContent = `${temp} °C`;
  humElement.textContent = `${hum} %`;

  // Guardar en localStorage
  localStorage.setItem("temp", temp);
  localStorage.setItem("hum", hum);
}

function cargarDatosPrevios() {
  const temp = localStorage.getItem("temp");
  const hum = localStorage.getItem("hum");

  if (temp && hum) {
    tempElement.textContent = `${temp} °C`;
    humElement.textContent = `${hum} %`;
  }
}

function aplicarTemaGuardado() {
  const tema = localStorage.getItem("theme");
  if (tema === "dark") {
    document.body.classList.add("dark");
    toggleSwitch.checked = true;
  } else {
    document.body.classList.remove("dark");
    toggleSwitch.checked = false;
  }
}

// ------------- EVENTOS -------------

toggleSwitch.addEventListener("change", () => {
  if (toggleSwitch.checked) {
    document.body.classList.add("dark");
    localStorage.setItem("theme", "dark");
  } else {
    document.body.classList.remove("dark");
    localStorage.setItem("theme", "light");
  }
});

// ------------ INICIALIZACIÓN ------------

cargarDatosPrevios();
aplicarTemaGuardado();

// Simulación de datos cada 5 segundos (ejemplo):
setInterval(() => {
  const temp = (20 + Math.random() * 10).toFixed(1);
  const hum = (40 + Math.random() * 20).toFixed(1);
  actualizarDatos(temp, hum);
}, 5000);
