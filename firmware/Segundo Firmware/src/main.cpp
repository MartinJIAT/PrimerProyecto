#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <time.h>

// --- CONFIG: credenciales WiFi ---
const char* ssid     = "Tobar_2";
const char* password = "Homb0549";

// ---------------------------------
WebServer server(80);

// Utilidad: formatear timestamp ISO8601
String isoNow(time_t t) {
  struct tm *tmstruct = localtime(&t);
  char buf[30];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
           tmstruct->tm_year + 1900,
           tmstruct->tm_mon + 1,
           tmstruct->tm_mday,
           tmstruct->tm_hour,
           tmstruct->tm_min,
           tmstruct->tm_sec);
  return String(buf);
}

// Simulación de DHT22
float simulateTempBase = 24.0;
float simulateHumBase  = 55.0;

void generateSample(time_t ts, float &temp, float &hum) {
  long seed = ts;
  randomSeed((uint32_t)seed);
  float tNoise = ((float)random(-50, 51)) / 100.0;   // ±0.5 °C
  float hNoise = ((float)random(-200, 201)) / 100.0; // ±2.0 %
  temp = simulateTempBase + tNoise;
  hum  = simulateHumBase + hNoise;
}

String buildHistoryJson(int hoursBack = 168, int stepMinutes = 60) {
  String out = "[";
  time_t now = time(nullptr);
  time_t step = stepMinutes * 60;
  int samples = (hoursBack * 60) / stepMinutes;
  for (int i = samples - 1; i >= 0; --i) {
    time_t ts = now - (i * step);
    float t, h;
    generateSample(ts, t, h);
    if (out.length() > 1) out += ",";
    out += "{";
    out += "\"ts\":\"" + isoNow(ts) + "\",";
    out += "\"temp\":" + String(t, 2) + ",";
    out += "\"hum\":" + String(h, 2);
    out += "}";
  }
  out += "]";
  return out;
}

// Servir archivos estáticos desde LittleFS
bool handleFileRead(String path) {
  if (path.endsWith("/")) path += "index.html";

  String contentType = "text/plain";
  if (path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";
  else if (path.endsWith(".json")) contentType = "application/json";

  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void setupRoutes() {
  server.on("/api/latest", HTTP_GET, []() {
    time_t now = time(nullptr);
    float t, h;
    generateSample(now, t, h);
    String payload = "{";
    payload += "\"ts\":\"" + isoNow(now) + "\",";
    payload += "\"temp\":" + String(t, 2) + ",";
    payload += "\"hum\":" + String(h, 2);
    payload += "}";
    server.send(200, "application/json", payload);
  });

  server.on("/api/history", HTTP_GET, []() {
    int hoursBack   = server.hasArg("hoursBack") ? server.arg("hoursBack").toInt() : 168;
    int stepMinutes = server.hasArg("stepMinutes") ? server.arg("stepMinutes").toInt() : 60;
    String payload  = buildHistoryJson(hoursBack, stepMinutes);
    server.send(200, "application/json", payload);
  });

  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "404: Not Found");
    }
  });
}

void setupTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  // esperar hasta que haya tiempo válido (máx 10s)
  for (int i = 0; i < 20; i++) {
    time_t now = time(nullptr);
    if (now > 100000) break;
    delay(500);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  if (!LittleFS.begin()) {
    Serial.println("Error montando LittleFS. ¿Subiste la carpeta data/ con 'Upload Filesystem Image'?");
  }

  // Conexión WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Conectando a %s ...\n", ssid);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFallo WiFi. Creando Access Point 'ESP32-AP'.");
    WiFi.softAP("ESP32-AP");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  setupTime();
  setupRoutes();
  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();
}
