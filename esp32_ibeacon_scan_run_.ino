#include "config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

/*
  BLE Watch Scanner → WiFi → MQTT + OTA (ESP32)


  Expected config.h (example)
  --------------------------
  #define WIFI_SSID      "..."
  #define WIFI_PASSWORD  "..."
  #define MQTT_ADDRESS   "192.168.1.10"
  static const char* hostName = "ble-scanner-1";
  constexpr uint8_t LedPin = 2;
  constexpr uint16_t scanTimeSeconds = 5;

  Notes:
  - MQTT topics are derived from hostName.
  - Publish payload size is chunked to avoid exceeding broker/client limits.
*/

namespace App {

// -----------------------------
// Build-time defaults (override in config.h if you want)
// -----------------------------
#ifndef MQTT_PORT
static constexpr uint16_t MQTT_PORT = 1883;
#endif

static constexpr uint32_t WIFI_RETRY_MS      = 1500;
static constexpr uint32_t MQTT_RETRY_MS      = 1500;
static constexpr uint32_t STATUS_PUBLISH_MS  = 60000;
static constexpr uint32_t BLE_SCAN_GAP_MS    = 2000;  // pause between scans
static constexpr uint8_t  MQTT_PAYLOAD_CHUNK = 8;     // devices per MQTT message

// If config.h defines these, they take precedence via the symbols:
#ifndef LedPin
static constexpr uint8_t LedPin = 2;
#endif

#ifndef scanTimeSeconds
static constexpr uint16_t scanTimeSeconds = 5;
#endif

static const char* WIFI_SSID_C = WIFI_SSID;
static const char* WIFI_PASS_C = WIFI_PASSWORD;
static const char* MQTT_HOST_C = MQTT_ADDRESS;

// -----------------------------
// Globals
// -----------------------------
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
BLEScan* bleScan = nullptr;

// Topics (stable C buffers)
static char topicRequest[128];
static char topicResponse[128];
static char topicGetIpRequest[128];
static char topicGetIpResponse[128];

// Timing state
static uint32_t lastWiFiAttemptMs = 0;
static uint32_t lastMqttAttemptMs = 0;
static uint32_t lastStatusPublishMs = 0;
static uint32_t lastBleScanMs = 0;

// Small LED helper (active-high assumed)
static inline void ledOn()  { digitalWrite(LedPin, HIGH); }
static inline void ledOff() { digitalWrite(LedPin, LOW);  }

static inline bool wifiUp() { return WiFi.status() == WL_CONNECTED; }

// -----------------------------
// MQTT helpers
// -----------------------------
static void mqttPublish(const char* topic, const char* payload, bool retained = false) {
  if (!mqtt.connected()) return;
  mqtt.publish(topic, payload, retained);
}

static void publishIp() {
  if (!wifiUp() || !mqtt.connected()) return;

  char msg[160];
  snprintf(msg, sizeof(msg), "%s ==> %s", hostName, WiFi.localIP().toString().c_str());
  mqttPublish(topicGetIpResponse, msg, false);
}

static void publishStatus() {
  // Minimal “alive” ping. Customize as needed.
  if (!wifiUp() || !mqtt.connected()) return;

  char msg[128];
  snprintf(msg, sizeof(msg), "{\"host\":\"%s\",\"ip\":\"%s\",\"rssi\":%d}",
           hostName, WiFi.localIP().toString().c_str(), WiFi.RSSI());
  mqttPublish(topicResponse, msg, true /*retain*/);
}

static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  // Convert payload to small null-terminated buffer safely
  char msg[128];
  const unsigned int n = (length >= sizeof(msg)) ? (sizeof(msg) - 1) : length;
  memcpy(msg, payload, n);
  msg[n] = '\0';

  const String t(topic);
  const String v(msg);

  Serial.print("[MQTT] ");
  Serial.print(t);
  Serial.print(" => ");
  Serial.println(v);

  if (t == topicGetIpRequest && v == "showip") {
    publishIp();
    return;
  }

  // You can expand this later:
  // - topicRequest for runtime config (scanTime, filters, etc.)
  // - commands like "ping", "status"
}

// -----------------------------
// WiFi (non-blocking)
// -----------------------------
static void ensureWiFi() {
  if (wifiUp()) return;

  const uint32_t now = millis();
  if (now - lastWiFiAttemptMs < WIFI_RETRY_MS) return;
  lastWiFiAttemptMs = now;

  static uint8_t tries = 0;
  if (tries == 0) {
    Serial.print("[WiFi] Connecting to ");
    Serial.println(WIFI_SSID_C);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
  }
  WiFi.begin(WIFI_SSID_C, WIFI_PASS_C);

  tries++;
  Serial.print("[WiFi] attempt=");
  Serial.println(tries);

  // Simple visual feedback
  (tries % 2 == 0) ? ledOn() : ledOff();

  // If you want auto-restart after many failures, add a threshold here.
}

// -----------------------------
// MQTT (non-blocking)
// -----------------------------
static void ensureMqtt() {
  if (!wifiUp()) return;
  if (mqtt.connected()) return;

  const uint32_t now = millis();
  if (now - lastMqttAttemptMs < MQTT_RETRY_MS) return;
  lastMqttAttemptMs = now;

  Serial.print("[MQTT] Connecting to ");
  Serial.println(MQTT_HOST_C);

  if (mqtt.connect(hostName)) {
    Serial.println("[MQTT] Connected");
    ledOn();

    mqtt.subscribe(topicRequest);
    mqtt.subscribe(topicGetIpRequest);

    publishStatus();
    publishIp();
  } else {
    Serial.print("[MQTT] Failed rc=");
    Serial.println(mqtt.state());
    ledOff();
  }
}

// -----------------------------
// OTA
// -----------------------------
static void setupOta() {
  ArduinoOTA.setHostname(hostName);

  ArduinoOTA
      .onStart([]() { Serial.println("[OTA] Start"); })
      .onEnd([]() { Serial.println("\n[OTA] End"); })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] Progress: %u%%\r", (progress * 100U) / total);
      })
      .onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });

  ArduinoOTA.begin();
}

// -----------------------------
// BLE scanning
// -----------------------------
class ScanCallbacks final : public BLEAdvertisedDeviceCallbacks {
public:
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    // Optional: You can filter devices here (by address prefix, RSSI threshold, etc.)
    // Keep empty to collect all results.
    (void)advertisedDevice;
  }
};

static void setupBle() {
  Serial.println("[BLE] Init + scanning config...");
  BLEDevice::init("");

  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new ScanCallbacks(), false);
  bleScan->setActiveScan(true);  // faster results, more power
  bleScan->setInterval(100);
  bleScan->setWindow(99);
}

static void publishScanResults(const BLEScanResults& results) {
  if (!mqtt.connected()) return;

  const int count = results.getCount();
  if (count <= 0) return;

  // Chunk JSON array to avoid oversize MQTT payloads.
  // Format: [{"Address":"..","Rssi":-65}, ...]
  String payload;
  payload.reserve(512);

  uint8_t inChunk = 0;
  payload = "[";

  for (int i = 0; i < count; i++) {
    const BLEAdvertisedDevice d = results.getDevice(i);

    if (inChunk > 0) payload += ",";
    payload += "{\"Address\":\"";
    payload += d.getAddress().toString().c_str();
    payload += "\",\"Rssi\":";
    payload += String(d.getRSSI());
    payload += "}";

    inChunk++;

    if (inChunk >= MQTT_PAYLOAD_CHUNK) {
      payload += "]";
      mqttPublish(topicResponse, payload.c_str(), false);
      delay(10);  // tiny yield for WiFi stack

      payload = "[";
      inChunk = 0;
    }
  }

  if (inChunk > 0) {
    payload += "]";
    mqttPublish(topicResponse, payload.c_str(), false);
  }
}

static void logScanResults(const BLEScanResults& results) {
  const int count = results.getCount();
  Serial.print("[BLE] Devices found: ");
  Serial.println(count);

  // Keep logging light; printing everything can be very noisy.
  // If you want per-device logs, enable it behind a flag.
  for (int i = 0; i < count; i++) {
    const BLEAdvertisedDevice d = results.getDevice(i);
    Serial.print("  - ");
    Serial.print(d.getAddress().toString().c_str());
    Serial.print(" rssi=");
    Serial.print(d.getRSSI());

    if (d.haveName()) {
      Serial.print(" name=\"");
      Serial.print(d.getName().c_str());
      Serial.print("\"");
    }
    Serial.println();
  }
}

static void maybeScanBle() {
  const uint32_t now = millis();
  if (now - lastBleScanMs < (scanTimeSeconds * 1000UL + BLE_SCAN_GAP_MS)) return;
  lastBleScanMs = now;

  if (!wifiUp()) return;  // scanning is fine without wifi, but publishing needs it

  // NOTE: start() blocks for scanTimeSeconds; OTA/MQTT still works between scans.
  BLEScanResults results = bleScan->start(scanTimeSeconds, false);

  logScanResults(results);
  publishScanResults(results);

  bleScan->clearResults();  // important: free memory
}

// -----------------------------
// Topic init
// -----------------------------
static void buildTopics() {
  // Keep backward-compatible topic naming from your original sketch.
  // Feel free to change to global topics like "ble_all/request" later.
  snprintf(topicRequest, sizeof(topicRequest), "%s/request", hostName);
  snprintf(topicResponse, sizeof(topicResponse), "%s/response", hostName);
  snprintf(topicGetIpRequest, sizeof(topicGetIpRequest), "%s/getip", hostName);
  snprintf(topicGetIpResponse, sizeof(topicGetIpResponse), "%s/ip", hostName);

  Serial.print("[MQTT] topicRequest: ");
  Serial.println(topicRequest);
  Serial.print("[MQTT] topicResponse: ");
  Serial.println(topicResponse);
  Serial.print("[MQTT] topicGetIpRequest: ");
  Serial.println(topicGetIpRequest);
  Serial.print("[MQTT] topicGetIpResponse: ");
  Serial.println(topicGetIpResponse);
}

} // namespace App

// -----------------------------
// Arduino entrypoints
// -----------------------------
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(App::LedPin, OUTPUT);
  App::ledOff();

  Serial.println();
  Serial.println("[BOOT] Starting...");

  App::buildTopics();

  App::mqtt.setServer(App::MQTT_HOST_C, App::MQTT_PORT);
  App::mqtt.setCallback(App::onMqttMessage);

  App::setupOta();
  App::setupBle();

  Serial.println("[BOOT] Ready.");
}

void loop() {
  App::ensureWiFi();
  App::ensureMqtt();

  if (App::mqtt.connected()) {
    App::mqtt.loop();
  }

  ArduinoOTA.handle();

  // Periodic small status publish (optional)
  const uint32_t now = millis();
  if (App::wifiUp() && App::mqtt.connected() && (now - App::lastStatusPublishMs) > App::STATUS_PUBLISH_MS) {
    App::lastStatusPublishMs = now;
    App::publishStatus();
  }

  App::maybeScanBle();

  delay(5); // cooperative yield
