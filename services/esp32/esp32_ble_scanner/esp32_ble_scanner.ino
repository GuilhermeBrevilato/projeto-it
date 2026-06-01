/**
 * ESP32 – BLE Scanner + HTTP POST Batch (Projeto IT)
 *
 * Monitora 3 beacons simultaneamente por gateway.
 * Fluxo por ciclo:
 *  1. Conecta WiFi e sincroniza NTP
 *  2. Desliga WiFi — libera antena para BLE
 *  3. BLE scan contínuo por COLLECT_DURATION_MS
 *     → detecta qualquer um dos 3 beacons cadastrados
 *  4. Deinit BLE — reconecta WiFi
 *  5. HTTP POST em sub-lotes de 50 registros
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
//  Credenciais e configurações (config.h)
// ──────────────────────────────────────────────
#include "config.h"

// ──────────────────────────────────────────────
//  Beacons monitorados
// ──────────────────────────────────────────────
#define BEACON_COUNT 3

const char* BEACON_MACS[BEACON_COUNT] = {
  "7c:ec:79:47:73:62",  // idoso
  "d4:f5:13:79:e4:5b",  // mobilidade_reduzida
  "7c:ec:79:44:e0:5f"   // jovem
};

// ──────────────────────────────────────────────
//  Configurações de timing
// ──────────────────────────────────────────────
constexpr uint32_t COLLECT_DURATION_MS  = 3UL * 60UL * 1000UL;
constexpr uint32_t SCAN_INTERVAL_MS     = 4000;
constexpr uint32_t SCAN_DURATION_SEC    = 3;
constexpr uint32_t WIFI_TIMEOUT_MS      = 15000;
constexpr uint32_t WDT_TIMEOUT_MS       = 25UL * 60UL * 1000UL;
constexpr uint16_t MAX_RECORDS          = 400;
constexpr uint32_t HTTP_TIMEOUT_MS      = 30000;
constexpr uint8_t  BATCH_SIZE           = 50;

// ──────────────────────────────────────────────
//  Estrutura de um registro coletado
// ──────────────────────────────────────────────
struct BLERecord {
  char timestamp[25];
  char tag_mac[18];
  bool found;
  int8_t rssi;
};

// ──────────────────────────────────────────────
//  Buffer de coleta
// ──────────────────────────────────────────────
BLERecord records[MAX_RECORDS];
uint16_t  recordCount = 0;

// ──────────────────────────────────────────────
//  Resultado do scan por beacon
// ──────────────────────────────────────────────
struct BeaconResult {
  bool found;
  int8_t rssi;
};

BeaconResult scanResults[BEACON_COUNT];

// ──────────────────────────────────────────────
//  Callback BLE — detecta qualquer beacon cadastrado
// ──────────────────────────────────────────────
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    char buf[18];
    strlcpy(buf, advertisedDevice.getAddress().toString().c_str(), sizeof(buf));
    for (int i = 0; buf[i]; i++) buf[i] = tolower((unsigned char)buf[i]);

    for (uint8_t i = 0; i < BEACON_COUNT; i++) {
      if (strcmp(buf, BEACON_MACS[i]) == 0) {
        scanResults[i].rssi  = (int8_t)advertisedDevice.getRSSI();
        scanResults[i].found = true;
        break;
      }
    }
  }
};

MyAdvertisedDeviceCallbacks bleCallbacks;

// ──────────────────────────────────────────────
//  WiFi
// ──────────────────────────────────────────────
bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > WIFI_TIMEOUT_MS) {
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
  Serial.print("[NTP] Sincronizando.");

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
//  Envia um sub-lote de registros
// ──────────────────────────────────────────────
bool postSubBatch(uint16_t start, uint16_t count) {
  const size_t jsonSize = 16 + (count * 160);
  DynamicJsonDocument doc(jsonSize);
  JsonArray array = doc.to<JsonArray>();

  for (uint16_t i = start; i < start + count; i++) {
    JsonObject obj = array.createNestedObject();
    obj["gateway_id"]       = GATEWAY_ID;
    obj["device_timestamp"] = records[i].timestamp;
    obj["tag_mac"]          = records[i].tag_mac;
    obj["found"]            = records[i].found;
    obj["rssi"]             = records[i].rssi;
  }

  String payload;
  payload.reserve(measureJson(doc) + 10);
  serializeJson(doc, payload);

  Serial.printf("[HTTP] Sub-lote %d-%d (%d bytes). Heap: %u\n",
                start + 1, start + count, payload.length(), ESP.getFreeHeap());

  for (uint8_t attempt = 1; attempt <= 3; attempt++) {
    esp_task_wdt_reset();

    WiFiClient client;

    HTTPClient http;
    http.begin(client, API_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Content-Length", String(payload.length()));
    http.addHeader("X-API-Key", API_KEY);
    http.setTimeout(HTTP_TIMEOUT_MS);

    int httpCode = http.POST(payload);
    http.end();

    if (httpCode == 200) {
      Serial.printf("[HTTP] OK (tentativa %d)\n", attempt);
      return true;
    }

    Serial.printf("[HTTP] Falhou código %d: %s (tentativa %d/3)\n",
                  httpCode, http.errorToString(httpCode).c_str(), attempt);

    if (attempt < 3) {
      delay(5000);
      esp_task_wdt_reset();
    }
  }

  return false;
}

// ──────────────────────────────────────────────
//  Envia todos os registros em sub-lotes
// ──────────────────────────────────────────────
void postAllBatches() {
  if (recordCount == 0) {
    Serial.println("[HTTP] Nenhum registro para enviar.");
    return;
  }

  uint16_t totalBatches = (recordCount + BATCH_SIZE - 1) / BATCH_SIZE;
  Serial.printf("[HTTP] Enviando %d registros em %d sub-lotes.\n",
                recordCount, totalBatches);

  uint16_t sent   = 0;
  uint16_t failed = 0;

  for (uint16_t b = 0; b < totalBatches; b++) {
    uint16_t start = b * BATCH_SIZE;
    uint16_t count = min((uint16_t)BATCH_SIZE, (uint16_t)(recordCount - start));

    if (postSubBatch(start, count)) {
      sent += count;
    } else {
      failed += count;
      Serial.printf("[HTTP] Sub-lote %d falhou. Continuando...\n", b + 1);
    }

    esp_task_wdt_reset();
    delay(500);
  }

  Serial.printf("[HTTP] Concluído. Enviados: %d  Falharam: %d  Total: %d\n",
                sent, failed, recordCount);
}

// ──────────────────────────────────────────────
//  Setup
// ──────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Projeto IT — iniciando ===");

  disableLoopWDT();

  const esp_task_wdt_config_t wdt_cfg = {
    .timeout_ms     = WDT_TIMEOUT_MS,
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
  delay(200);
  Serial.println("[WiFi] Desligado para coleta BLE.");

  // 3. Inicializa BLE e configura scan uma única vez
  BLEDevice::init("");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(&bleCallbacks, false);
  pScan->setActiveScan(false);
  pScan->setInterval(100);
  pScan->setWindow(99);
  Serial.printf("[BLE] Iniciado. Heap: %u\n", ESP.getFreeHeap());

  // 4. Coleta por COLLECT_DURATION_MS
  recordCount = 0;
  uint32_t collectStart = millis();

  Serial.printf("[COLETA] Iniciando. Duração: %lu min. Intervalo: %lu s.\n",
                COLLECT_DURATION_MS / 60000,
                SCAN_INTERVAL_MS / 1000);

  while (millis() - collectStart < COLLECT_DURATION_MS) {
    esp_task_wdt_reset();

    if (recordCount + BEACON_COUNT > MAX_RECORDS) {
      Serial.println("[COLETA] Buffer cheio. Encerrando coleta.");
      break;
    }

    // Reseta resultados do scan
    for (uint8_t i = 0; i < BEACON_COUNT; i++) {
      scanResults[i].found = false;
      scanResults[i].rssi  = -127;
    }

    uint32_t scanStart = millis();
    pScan->start(SCAN_DURATION_SEC, false);
    pScan->clearResults();

    // Gera timestamp e armazena um registro por beacon
    char timestamp[25];
    getCurrentTimestamp(timestamp, sizeof(timestamp));

    for (uint8_t i = 0; i < BEACON_COUNT; i++) {
      strlcpy(records[recordCount].timestamp, timestamp, sizeof(records[recordCount].timestamp));
      strlcpy(records[recordCount].tag_mac, BEACON_MACS[i], sizeof(records[recordCount].tag_mac));
      records[recordCount].found = scanResults[i].found;
      records[recordCount].rssi  = scanResults[i].rssi;

      Serial.printf("[COLETA] #%03d  mac=%s  found=%s  rssi=%d  heap=%u\n",
                    recordCount + 1,
                    BEACON_MACS[i],
                    scanResults[i].found ? "true" : "false",
                    scanResults[i].rssi,
                    ESP.getFreeHeap());

      recordCount++;
    }

    uint32_t elapsed = millis() - scanStart;
    if (elapsed < SCAN_INTERVAL_MS) {
      delay(SCAN_INTERVAL_MS - elapsed);
    }
  }

  // 5. Deinit BLE — aguarda e reconecta WiFi
  BLEDevice::deinit(true);
  Serial.printf("[BLE] Deinit. Heap: %u\n", ESP.getFreeHeap());

  delay(500);
  esp_task_wdt_reset();

  if (!connectWiFi()) {
    Serial.println("[ERRO] WiFi falhou no envio. Reiniciando...");
    esp_restart();
  }

  delay(2000);
  esp_task_wdt_reset();

  // 6. Envia todos os registros em sub-lotes
  postAllBatches();
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
