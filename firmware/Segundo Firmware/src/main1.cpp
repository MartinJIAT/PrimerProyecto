#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <sstream>

// --- CONFIG: Reemplaza por tus credenciales ---
const char* ssid = "Tobar_2";
const char* password = "Homb0549";
// ---------------------------------------------

WebServer server(80);

// Utility: ISO8601 timestamp
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

// Simulate DHT22 reading (we don't read hardware here)
float simulateTempBase = 24.0;
float simulateHumBase = 55.0;

// Generate a single sample near base with some noise
void generateSample(time_t ts, float &temp, float &hum) {
  long seed = ts;
  randomSeed((uint32_t)seed);
  float tNoise = ((float)random(-50, 51))/100.0; // -0.5 .. 0.5
  float hNoise = ((float)random(-200, 201))/100.0; // -2.0 .. 2.0
  temp = simulateTempBase + tNoise;
  hum  = simulateHumBase + hNoise;
}

// Generate history: last N samples at interval seconds
String buildHistoryJson(int hoursBack = 168, int stepMinutes = 60) {
  // returns JSON array of {ts, temp, hum}
  String out = "[";
  time_t now = time(nullptr);
  time_t step = stepMinutes * 60;
  int samples = (hoursBack*60) / stepMinutes;
  for (int i = samples-1; i >= 0; --i) {
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

// Serve static files from LittleFS (index, css, js)
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
  // API: latest simulated reading
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

  // API: history. Query params optional: hoursBack, stepMinutes
  server.on("/api/history", HTTP_GET, []() {
    int hoursBack = 168; // last 7 days by default
    int stepMinutes = 60; // 1 sample/hour
    if (server.hasArg("hoursBack")) hoursBack = server.arg("hoursBack").toInt();
    if (server.hasArg("stepMinutes")) stepMinutes = server.arg("stepMinutes").toInt();
    String payload = buildHistoryJson(hoursBack, stepMinutes);
    server.send(200, "application/json", payload);
  });

  // Serve static files or 404
  server.onNotFound([]() {
    String path = server.uri();
    if (handleFileRead(path)) {
      return;
    }
    server.send(404, "text/plain", "404: Not Found");
  });
}

void setupTime() {
  // Use NTP to keep timestamps accurate
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  // wait a little for time to sync (non-blocking minimal)
  delay(2000);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // LittleFS init
  if(!LittleFS.begin()){
    Serial.println("LittleFS mount failed. Asegurate de subir data/ usando Upload Filesystem Image.");
  }

  // WiFi connect
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
    Serial.println();
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nNo se pudo conectar a WiFi. Levantando Access Point 'ESP32-AP'.");
    WiFi.softAP("ESP32-AP");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  setupTime();
  setupRoutes();
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  // nothing more to do; API is stateless and simulated
}
