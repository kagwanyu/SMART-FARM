/*
 * ESP32 Soil Moisture Sensor - HTTP POST + WebSocket
 * Test endpoint: httpbin.org (swap API_HOST when real server is ready)
 * Sensor: Capacitive Soil Moisture Sensor v2.0 on GPIO34
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "credentials.h"
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Test endpoint (httpbin echoes your POST back so you can see it worked)
const char* API_HOST      = "httpbin.org";
const int   API_PORT      = 443;
const char* API_PATH      = "/post";          // swap to e.g. "/api/sensor-data" later
const char* DEVICE_ID     = "naila-esp32-01"; // identifies your device on the server

// WebSocket (httpbin doesn't support WS, swap host/path when real server is ready)
const char* WS_HOST       = "echo.websocket.org"; // free WS echo server for testing
const int   WS_PORT       = 80;
const char* WS_PATH       = "/";

// Sensor
const int   MOISTURE_PIN  = 34;   // GPIO34 = AOUT from sensor
const int   DRY_VALUE     = 4095; // raw ADC when sensor is in air (dry)
const int   WET_VALUE     = 1500; // raw ADC when sensor is fully submerged

// Timing
const unsigned long HTTP_INTERVAL  = 10000;  // send HTTP POST every 10 seconds
const unsigned long WIFI_TIMEOUT   = 20000;  // give up WiFi connect after 20s
const int           MAX_HTTP_RETRIES = 3;    // retry failed POSTs this many times
// ──────────────────────────────────────────────────────────────────────────────

WebSocketsClient webSocket;

// Rate limiting
unsigned long lastHttpSend   = 0;
unsigned long lastWsReconnect = 0;
bool          wsConnected    = false;

// ─── HELPERS ──────────────────────────────────────────────────────────────────

// Map raw ADC to 0-100% moisture
int getMoisturePercent(int raw) {
  int percent = map(raw, DRY_VALUE, WET_VALUE, 0, 100);
  return constrain(percent, 0, 100);
}

// Read sensor (average 10 samples to reduce noise)
int readMoisture() {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(MOISTURE_PIN);
    delay(10);
  }
  return sum / 10;
}

// Build JSON payload
String buildPayload(int rawValue, int percent) {
  StaticJsonDocument<256> doc;
  doc["device_id"]        = DEVICE_ID;
  doc["moisture_raw"]     = rawValue;
  doc["moisture_percent"] = percent;
  doc["timestamp"]        = millis(); // replace with NTP time when available
  doc["wifi_rssi"]        = WiFi.RSSI();

  String payload;
  serializeJson(doc, payload);
  return payload;
}

// ─── WIFI ─────────────────────────────────────────────────────────────────────

bool connectWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.print(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT) {
      Serial.println("\n[WiFi] TIMEOUT - could not connect");
      return false;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[WiFi] Connected! IP: " + WiFi.localIP().toString());
  return true;
}

bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.println("[WiFi] Disconnected - reconnecting...");
  WiFi.disconnect();
  delay(1000);
  return connectWiFi();
}

// ─── HTTP POST ────────────────────────────────────────────────────────────────

void sendHTTP(String payload) {
  if (!ensureWiFi()) {
    Serial.println("[HTTP] Skipping POST - no WiFi");
    return;
  }

  HTTPClient http;
  String url = "https://" + String(API_HOST) + String(API_PATH);

  for (int attempt = 1; attempt <= MAX_HTTP_RETRIES; attempt++) {
    Serial.printf("[HTTP] Attempt %d/%d → POST %s\n", attempt, MAX_HTTP_RETRIES, url.c_str());

    // Skip SSL cert verification (use for testing; add cert fingerprint for production)
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(8000); // 8 second timeout

    int httpCode = http.POST(payload);

    if (httpCode > 0) {
      String response = http.getString();
      Serial.printf("[HTTP] Response code: %d\n", httpCode);

      if (httpCode == 200 || httpCode == 201) {
        // Parse JSON response
        StaticJsonDocument<512> responseDoc;
        DeserializationError err = deserializeJson(responseDoc, response);

        if (!err) {
          Serial.println("[HTTP] Server response parsed OK");
          // httpbin echoes your JSON under "json" key
          // When using real server, read: responseDoc["status"], responseDoc["message"] etc.
          Serial.println("[HTTP] Sent data: " + payload);
        } else {
          Serial.println("[HTTP] Response parse error: " + String(err.c_str()));
        }

        http.end();
        return; // success, stop retrying
      } else {
        Serial.printf("[HTTP] Server error: %d\n", httpCode);
      }
    } else {
      // Negative codes = connection-level errors
      Serial.printf("[HTTP] Connection error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();

    if (attempt < MAX_HTTP_RETRIES) {
      Serial.println("[HTTP] Retrying in 3s...");
      delay(3000);
    }
  }

  Serial.println("[HTTP] All retries failed - will try next interval");
}

// ─── WEBSOCKET ────────────────────────────────────────────────────────────────

void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {

    case WStype_CONNECTED:
      wsConnected = true;
      Serial.println("[WS] Connected to " + String(WS_HOST));
      webSocket.sendTXT("{\"status\":\"esp32 connected\"}");
      break;

    case WStype_DISCONNECTED:
      wsConnected = false;
      Serial.println("[WS] Disconnected");
      break;

    case WStype_TEXT: {
      String msg = String((char*)payload);
      Serial.println("[WS] Received: " + msg);

      // Parse incoming command from server
      StaticJsonDocument<256> cmd;
      DeserializationError err = deserializeJson(cmd, msg);
      if (!err) {
        // Example: server sends {"command":"read_now"} to trigger immediate reading
        const char* command = cmd["command"];
        if (command && strcmp(command, "read_now") == 0) {
          Serial.println("[WS] Server requested immediate sensor read");
          int raw     = readMoisture();
          int percent = getMoisturePercent(raw);
          String data = buildPayload(raw, percent);
          webSocket.sendTXT(data);
        }
      }
      break;
    }

    case WStype_ERROR:
      Serial.println("[WS] Error occurred");
      wsConnected = false;
      break;

    default:
      break;
  }
}

void setupWebSocket() {
  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(onWebSocketEvent);
  webSocket.setReconnectInterval(5000); // auto-reconnect every 5s if dropped
  Serial.println("[WS] WebSocket connecting to " + String(WS_HOST));
}

// ─── SETUP & LOOP ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=============================");
  Serial.println("  ESP32 Soil Moisture Node");
  Serial.println("=============================");

  analogReadResolution(12); // ESP32 ADC = 12-bit (0-4095)
  pinMode(MOISTURE_PIN, INPUT);

  if (!connectWiFi()) {
    Serial.println("[SETUP] Will retry WiFi in loop...");
  }

  setupWebSocket();
}

void loop() {
  // Keep WebSocket alive (handles ping/pong and reconnects)
  webSocket.loop();

  unsigned long now = millis();

  // Send HTTP POST every HTTP_INTERVAL ms
  if (now - lastHttpSend >= HTTP_INTERVAL) {
    lastHttpSend = now;

    int raw     = readMoisture();
    int percent = getMoisturePercent(raw);

    Serial.printf("\n[SENSOR] Raw: %d | Moisture: %d%%\n", raw, percent);

    // Send via HTTP POST
    String payload = buildPayload(raw, percent);
    sendHTTP(payload);

    // Also push via WebSocket if connected
    if (wsConnected) {
      webSocket.sendTXT(payload);
      Serial.println("[WS] Data sent via WebSocket");
    }
  }
}