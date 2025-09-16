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

// ====== CONFIGURACIÓN HARDWARE ======
#define DHTPIN 4 // GPIO para el DHT22
#define DHTTYPE DHT22 // Tipo de sensor
DHT dht(DHTPIN, DHTTYPE);

// ====== SERVIDOR WEB ======
WebServer server(80);

// ====== VARIABLES GLOBALES ======
String apSuffix; // últimos 4 dígitos del MAC
String apName; // SSID del AP en WiFiManager
String mdnsName; // nombre mDNS: <mdnsName>.local

float currentTemp = NAN;
float currentHum = NAN;

// ====== NTP (hora para logs) ======
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0; // ajusta si quieres hora local por firmware
const int daylightOffset_sec = 0;

// ====== UTILIDADES ======
String macSuffix(){
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[5];
  snprintf(buf, sizeof(buf), "%02X%02X", mac[4], mac[5]);
  return String(buf);
}

bool ensureFS(){
  if(!SPIFFS.begin(true)){
    Serial.println(F("[SPIFFS] Falló el montaje"));
    return false;
  }
  return true;
}

void handleFile(String path){
  if (path.endsWith("/")) {
    path += "index.html";
  }

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

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("Iniciando..."));

  // Inicia SPIFFS
  if (!ensureFS()) {
    return;
  }

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

  // ====== Rutas HTTP ======
  // Ruta para los datos del sensor
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

  // Ruta para el historial de datos
  server.on("/api/history", HTTP_GET, []() {
    File file = SPIFFS.open("/data.csv", "r");
    if (!file) {
      server.send(500, "text/plain", "Failed to open history file");
      return;
    }
    server.streamFile(file, "text/csv");
    file.close();
  });

  // Manejar todas las demás peticiones como archivos estáticos
  server.onNotFound([]() {
    handleFile(server.uri());
  });

  server.begin();
  Serial.println(F("Servidor HTTP iniciado"));
}

// ====== LOOP ======
void loop() {
  server.handleClient();

  static unsigned long lastSensorReadTime = 0;
  const unsigned long readInterval = 10000; // 10 segundos en milisegundos

  if (millis() - lastSensorReadTime > readInterval) {
    lastSensorReadTime = millis();
    currentHum = dht.readHumidity();
    currentTemp = dht.readTemperature();
    
    if (isnan(currentHum) || isnan(currentTemp)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      Serial.print("Current Temp: ");
      Serial.println(currentTemp);
      Serial.print("Current Hum: ");
      Serial.println(currentHum);
    }
  }

  static unsigned long lastLogTime = 0;
  const unsigned long logInterval = 300000; // 5 minutos en milisegundos

  if (millis() - lastLogTime > logInterval) {
    lastLogTime = millis();
    if (!isnan(currentHum) && !isnan(currentTemp)) {
      File file = SPIFFS.open("/data.csv", FILE_APPEND);
      if (!file) {
        Serial.println("Failed to open file for appending");
        return;
      }
      
      time_t now = time(nullptr);
      char buffer[50];
      sprintf(buffer, "%ld,%.2f,%.2f\n", now, currentTemp, currentHum);
      file.print(buffer);
      file.close();
      Serial.println("Data logged successfully!");
    }
  }
}