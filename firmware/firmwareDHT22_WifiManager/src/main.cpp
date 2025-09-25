#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <FS.h>
#include <SPIFFS.h>
#include <time.h>
#include <DHT.h>
#include <HTTPClient.h>

// ====== CONFIGURACIÓN HARDWARE ======
#define DHTPIN 4      // GPIO para el DHT22
#define DHTTYPE DHT22 // Tipo de sensor
DHT dht(DHTPIN, DHTTYPE);

// ====== SERVIDOR WEB ======
WebServer server(80);

// ====== VARIABLES GLOBALES ======
String apSuffix;   // últimos dígitos del MAC
String apName;     // SSID del AP en WiFiManager
String mdnsName;   // nombre mDNS: <mdnsName>.local
float currentTemp = NAN;
float currentHum = NAN;

// ====== CONFIGURACIÓN GOOGLE SHEETS ======
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbzWphbim0zWUsFUjIM9X-1GdNkVObZN8qPPy0jY_UBYGOSIMc_nOiRqoAnUQZFI1HvFuw/exec";
const char* deviceId = "ESP32_01"; // ID único del dispositivo

// ====== NTP (hora para logs) ======
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0; // ajusta según tu zona horaria
const int daylightOffset_sec = 0;

// ====== UTILIDADES ======
String macSuffix() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[7];
  snprintf(buf, sizeof(buf), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(buf);
}

bool ensureFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println(F("[SPIFFS] Falló el montaje"));
    return false;
  }
  return true;
}

void handleFile(String path) {
  if (path.endsWith("/")) path += "index.html";

  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    String contentType = "text/plain";
    if (path.endsWith(".html")) contentType = "text/html";
    else if (path.endsWith(".css")) contentType = "text/css";
    else if (path.endsWith(".js")) contentType = "application/javascript";
    else if (path.endsWith(".png")) contentType = "image/png";
    else if (path.endsWith(".jpg")) contentType = "image/jpeg";
    else if (path.endsWith(".gif")) contentType = "image/gif";
    else if (path.endsWith(".ico")) contentType = "image/x-icon";
    server.streamFile(file, contentType);
    file.close();
  } else {
    server.send(404, "text/plain", "404 Not Found");
  }
}

// ====== ENVIAR DATOS A GOOGLE SHEETS (Hoja: Datos) ======
void sendToGoogleSheets(float temp, float hum) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(googleScriptURL);
    http.addHeader("Content-Type", "application/json");

    String mac = macSuffix();
    String jsonData = "{";
    jsonData += "\"type\":\"Datos\",";
    jsonData += "\"deviceId\":\"" + String(deviceId) + "\",";
    jsonData += "\"mac\":\"" + mac + "\",";
    jsonData += "\"temp\":" + String(temp, 2) + ",";
    jsonData += "\"hum\":" + String(hum, 2);
    jsonData += "}";

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      Serial.printf("Datos enviados! Código: %d\n", httpResponseCode);
    } else {
      Serial.printf("Error enviando datos: %d\n", httpResponseCode);
    }
    http.end();
  }
}

// ====== ENVIAR EVENTOS A GOOGLE SHEETS (Hoja: Estados) ======
void sendEvent(String evento, String motivo) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(googleScriptURL);
    http.addHeader("Content-Type", "application/json");

    String mac = macSuffix();
    float chipTemp = temperatureRead(); // temperatura interna del chip

    String jsonData = "{";
    jsonData += "\"type\":\"Estados\",";
    jsonData += "\"deviceId\":\"" + String(deviceId) + "\",";
    jsonData += "\"mac\":\"" + mac + "\",";
    jsonData += "\"evento\":\"" + evento + "\",";
    jsonData += "\"motivo\":\"" + motivo + "\",";
    jsonData += "\"tempChip\":" + String(chipTemp, 2);
    jsonData += "}";

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      Serial.printf("Evento enviado! Código: %d\n", httpResponseCode);
    } else {
      Serial.printf("Error enviando evento: %d\n", httpResponseCode);
    }
    http.end();
  }
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  Serial.println(F("Iniciando..."));

  // Inicia SPIFFS
  if (!ensureFS()) return;

  // Configurar WiFi con WiFiManager
  WiFiManager wm;
  apSuffix = macSuffix();
  apName = "ESP32-" + apSuffix;
  wm.setHostname(apName.c_str());

  if (!wm.autoConnect(apName.c_str())) {
    Serial.println(F("No se pudo conectar al WiFi, reiniciando..."));
    ESP.restart();
  }

  Serial.println(F("WiFi conectado!"));
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());

  // Configurar mDNS
  mdnsName = "esp32";
  if (MDNS.begin(mdnsName.c_str())) {
    Serial.printf("mDNS iniciado: http://%s.local\n", mdnsName.c_str());
  }

  // Configurar NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Iniciar DHT
  dht.begin();

  // Registrar evento de reinicio
  sendEvent("Reinicio", "Encendido o Reset manual");

  // ====== Rutas HTTP ======
  server.on("/api/latest", HTTP_GET, []() {
    if (isnan(currentTemp) || isnan(currentHum)) {
      server.send(500, "application/json", "{\"error\":\"Error leyendo DHT22\"}");
      return;
    }
    String payload = "{";
    payload += "\"temp\":" + String(currentTemp, 1) + ",";
    payload += "\"hum\":" + String(currentHum, 1);
    payload += "}";
    server.send(200, "application/json", payload);
  });

  server.on("/api/history", HTTP_GET, []() {
    File file = SPIFFS.open("/data.csv", "r");
    if (!file) {
      server.send(500, "text/plain", "Failed to open history file");
      return;
    }
    server.streamFile(file, "text/csv");
    file.close();
  });

  server.onNotFound([]() {
    handleFile(server.uri());
  });

  server.begin();
  Serial.println(F("Servidor HTTP iniciado"));
}

// ====== LOOP ======
void loop() {
  server.handleClient();

  // Leer sensores cada 10s
  static unsigned long lastSensorReadTime = 0;
  const unsigned long readInterval = 3000;

  if (millis() - lastSensorReadTime > readInterval) {
    lastSensorReadTime = millis();
    currentHum = dht.readHumidity();
    currentTemp = dht.readTemperature();

    if (isnan(currentHum) || isnan(currentTemp)) {
      Serial.println("Error leyendo DHT22!");
    } else {
      Serial.printf("Temp: %.2f °C | Hum: %.2f %%\n", currentTemp, currentHum);
    }
  }

  // Guardar en CSV y enviar a Google Sheets cada 5 min
  static unsigned long lastLogTime = 0;
  const unsigned long logInterval = 10000;

  if (millis() - lastLogTime > logInterval) {
    lastLogTime = millis();
    if (!isnan(currentHum) && !isnan(currentTemp)) {
      // Guardar en SPIFFS
      File file = SPIFFS.open("/data.csv", FILE_APPEND);
      if (file) {
        time_t now = time(nullptr);
        file.printf("%ld,%.2f,%.2f\n", now, currentTemp, currentHum);
        file.close();
        Serial.println("Datos guardados en CSV!");
      }
      // Enviar a Google Sheets (Hoja Datos)
      sendToGoogleSheets(currentTemp, currentHum);
    }
  }

  // Ejemplo: evento si chip sobrecalentado (>70 °C)
  if (temperatureRead() > 70) {
    sendEvent("Alerta", "Chip sobrecalentado");
  }
}
