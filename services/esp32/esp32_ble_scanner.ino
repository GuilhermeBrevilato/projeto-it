/**
 * ESP32 – BLE Scanner + HTTP POST Batch para Cloud Run (Projeto IT)
 *
 * Fluxo por ciclo:
 *  1. Conecta WiFi e sincroniza NTP
 *  2. Desliga WiFi — libera antena para BLE
 *  3. BLE scan contínuo por COLLECT_DURATION_MS
 *     → a cada SCAN_INTERVAL_MS armazena {timestamp, found, rssi}
 *  4. Deinit BLE — reconecta WiFi
 *  5. HTTP POST batch com todos os registros
 *  6. Reinicia o ciclo via esp_restart()
 *
 * Dependências:
 *  - ArduinoJson 6.x
 *  - ESP32 core 3.x
 *
 * Board: ESP32 Dev Module
 * Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)
 */

// ──────────────────────────────────────────────
//  Includes
// ──────────────────────────────────────────────
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <time.h>

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ──────────────────────────────────────────────
//  Configurações
// ──────────────────────────────────────────────
namespace Cfg {
  constexpr char WIFI_SSID[]    = "VIVOFIBRA-5318";
  constexpr char WIFI_PASS[]    = "E645BE7091";

  constexpr char API_URL[]      = "https://ingestion-api-744027147092.us-central1.run.app/ingest/batch";
  constexpr char API_KEY[]      = "DkZU2gICJnTGWcwADqsHekwitwI6-PBkIydLC89n3C0";
  constexpr char GATEWAY_ID[]   = "gw-esp32-01";
  constexpr char TARGET_MAC[]   = "7c:ec:79:47:73:62";

  constexpr uint32_t COLLECT_DURATION_MS = 3UL * 60UL * 1000UL;
  constexpr uint32_t SCAN_INTERVAL_MS    = 3000;
  constexpr uint32_t SCAN_DURATION_SEC   = 2;
  constexpr uint32_t WIFI_TIMEOUT_MS     = 15000;
  constexpr uint32_t WDT_TIMEOUT_SEC     = 60;
  constexpr uint16_t MAX_RECORDS         = 100;
}

// ──────────────────────────────────────────────
//  Estrutura de um registro coletado
// ──────────────────────────────────────────────
struct BLERecord {
  char timestamp[25];
  bool found;
  int8_t rssi;
};

// ──────────────────────────────────────────────
//  Buffer de coleta
// ──────────────────────────────────────────────
BLERecord records[Cfg::MAX_RECORDS];
uint16_t  recordCount = 0;

// ──────────────────────────────────────────────
//  Resultado do último scan BLE
// ──────────────────────────────────────────────
volatile int8_t  g_rssi  = -127;
volatile bool    g_found = false;

// ──────────────────────────────────────────────
//  Callback BLE — instância única global
// ──────────────────────────────────────────────
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    char buf[18];
    strlcpy(buf, advertisedDevice.getAddress().toString().c_str(), sizeof(buf));
    for (int i = 0; buf[i]; i++) buf[i] = tolower((unsigned char)buf[i]);

    if (strcmp(buf, Cfg::TARGET_MAC) == 0) {
      g_rssi  = (int8_t)advertisedDevice.getRSSI();
      g_found = true;
      BLEDevice::getScan()->stop();
    }
  }
};

MyAdvertisedDeviceCallbacks bleCallbacks;

// ──────────────────────────────────────────────
//  Um ciclo de scan BLE
// ──────────────────────────────────────────────
void scanOnce() {
  g_found = false;
  g_rssi  = -127;

  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(&bleCallbacks, false);
  pScan->setActiveScan(false);
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(Cfg::SCAN_DURATION_SEC, false);
  pScan->clearResults();
}

// ──────────────────────────────────────────────
//  WiFi
// ──────────────────────────────────────────────
bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  WiFi.begin(Cfg::WIFI_SSID, Cfg::WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > Cfg::WIFI_TIMEOUT_MS) {
      Serial.println("[WiFi] Timeout!");
      return false;
    }
    esp_task_wdt_reset();
    delay(500);
    Serial.print('.');
  }
  Serial.printf("\n[WiFi] Conectado. IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

// ──────────────────────────────────────────────
//  Sincronização NTP
// ──────────────────────────────────────────────
bool syncNTP() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.print("[NTP] Sincronizando");

  time_t now = 0;
  uint32_t t0 = millis();
  while (now < 1000000000UL && millis() - t0 < 10000) {
    delay(200);
    time(&now);
    Serial.print('.');
    esp_task_wdt_reset();
  }

  if (now < 1000000000UL) {
    Serial.println(" FALHOU");
    return false;
  }

  Serial.printf(" OK (%lu)\n", now);
  return true;
}

// ──────────────────────────────────────────────
//  Monta timestamp ISO 8601 do momento atual
// ──────────────────────────────────────────────
void getCurrentTimestamp(char* buf, size_t len) {
  time_t now;
  time(&now);
  strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
}

// ──────────────────────────────────────────────
//  Envia batch para a API
// ──────────────────────────────────────────────
bool postBatch() {
  if (recordCount == 0) {
    Serial.println("[HTTP] Nenhum registro para enviar.");
    return true;
  }

  const size_t jsonSize = 10 + (recordCount * 150);
  DynamicJsonDocument doc(jsonSize);
  JsonArray array = doc.to<JsonArray>();

  for (uint16_t i = 0; i < recordCount; i++) {
    JsonObject obj = array.createNestedObject();
    obj["gateway_id"]       = Cfg::GATEWAY_ID;
    obj["device_timestamp"] = records[i].timestamp;
    obj["tag_mac"]          = Cfg::TARGET_MAC;
    obj["found"]            = records[i].found;
    obj["rssi"]             = records[i].rssi;
  }

  String payload;
  serializeJson(doc, payload);

  Serial.printf("[HTTP] Enviando %d registros (%d bytes)...\n",
                recordCount, payload.length());

  HTTPClient http;
  http.begin(Cfg::API_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", Cfg::API_KEY);
  http.setTimeout(30000);

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.printf("[HTTP] Resposta: %d  Body: %s\n",
                  httpCode, http.getString().c_str());
  } else {
    Serial.printf("[HTTP] Erro: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  return httpCode == 200;
}

// ──────────────────────────────────────────────
//  Setup
// ──────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Projeto IT — iniciando ===");

  const esp_task_wdt_config_t wdt_cfg = {
    .timeout_ms     = Cfg::WDT_TIMEOUT_SEC * 1000,
    .idle_core_mask = 0,
    .trigger_panic  = true
  };
  esp_task_wdt_reconfigure(&wdt_cfg);
  esp_task_wdt_add(nullptr);

  // 1. WiFi + NTP
  if (!connectWiFi()) {
    Serial.println("[ERRO] WiFi falhou. Reiniciando...");
    esp_restart();
  }
  esp_task_wdt_reset();

  if (!syncNTP()) {
    Serial.println("[ERRO] NTP falhou. Reiniciando...");
    esp_restart();
  }
  esp_task_wdt_reset();

  // 2. Desliga WiFi — libera antena para BLE
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("[WiFi] Desligado para coleta BLE.");

  // 3. Inicializa BLE uma única vez
  BLEDevice::init("");
  Serial.printf("[BLE] Iniciado. Heap: %u\n", ESP.getFreeHeap());

  // 4. Coleta por COLLECT_DURATION_MS
  recordCount = 0;
  uint32_t collectStart = millis();

  Serial.printf("[COLETA] Iniciando. Duração: %lu min. Intervalo: %lu s.\n",
                Cfg::COLLECT_DURATION_MS / 60000,
                Cfg::SCAN_INTERVAL_MS / 1000);

  while (millis() - collectStart < Cfg::COLLECT_DURATION_MS) {
    esp_task_wdt_reset();

    uint32_t scanStart = millis();

    scanOnce();

    if (recordCount < Cfg::MAX_RECORDS) {
      getCurrentTimestamp(records[recordCount].timestamp,
                          sizeof(records[recordCount].timestamp));
      records[recordCount].found = g_found;
      records[recordCount].rssi  = g_rssi;
      recordCount++;

      Serial.printf("[COLETA] #%03d  found=%s  rssi=%d  heap=%u\n",
                    recordCount,
                    g_found ? "true" : "false",
                    g_rssi,
                    ESP.getFreeHeap());
    }

    uint32_t elapsed = millis() - scanStart;
    if (elapsed < Cfg::SCAN_INTERVAL_MS) {
      delay(Cfg::SCAN_INTERVAL_MS - elapsed);
    }
  }

  // 5. Deinit BLE — reconecta WiFi
  BLEDevice::deinit(true);
  Serial.printf("[BLE] Deinit. Heap: %u\n", ESP.getFreeHeap());

  if (!connectWiFi()) {
    Serial.println("[ERRO] WiFi falhou no envio. Reiniciando...");
    esp_restart();
  }
  esp_task_wdt_reset();

  // 6. Envia batch
  postBatch();
  esp_task_wdt_reset();

  // 7. Reinicia o ciclo
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_task_wdt_delete(nullptr);

  Serial.println("[CICLO] Concluído. Reiniciando...");
  Serial.flush();
  delay(500);
  esp_restart();
}

// ──────────────────────────────────────────────
//  Loop — não utilizado
// ──────────────────────────────────────────────
void loop() {}