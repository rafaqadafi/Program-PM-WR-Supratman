#include "wifi_service.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "config.h"
#include "led/led_status.h"

namespace {
WiFiManager wifiManager;
SemaphoreHandle_t wifiResetMutex = nullptr;

void wifiTask(void *) {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);

  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConnectTimeout(Config::WiFiPortal::CONNECT_TIMEOUT_SECONDS);
  wifiManager.setConfigPortalTimeout(0);

  Serial.println("[WiFi] Mencoba kredensial tersimpan...");
  ledStatusSetWifiConnected(false);
  wifiManager.autoConnect(Config::WiFiPortal::NAME,
                          Config::WiFiPortal::PASSWORD);

  wl_status_t previousStatus = WL_NO_SHIELD;
  for (;;) {
    if (xSemaphoreTake(wifiResetMutex, 0) == pdTRUE) {
      wifiManager.process();
      xSemaphoreGive(wifiResetMutex);
    }
    const wl_status_t currentStatus = WiFi.status();

    if (currentStatus != previousStatus) {
      previousStatus = currentStatus;
      if (currentStatus == WL_CONNECTED) {
        Serial.print("[WiFi] Terhubung, IP: ");
        Serial.println(WiFi.localIP());
        ledStatusSetWifiConnected(true);
      } else {
        Serial.printf("[WiFi] Belum terhubung; portal %s aktif\n",
                      Config::WiFiPortal::NAME);
        ledStatusSetWifiConnected(false);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}  // namespace

void wifiServiceBegin() {
  wifiResetMutex = xSemaphoreCreateMutex();
  xTaskCreate(wifiTask, "WiFiManager", 5120, nullptr, 2, nullptr);
}

void wifiServiceResetSettingsAndRestart() {
  Serial.println("[WiFi reset] Menghapus SSID dan password dari NVS...");
  if (xSemaphoreTake(wifiResetMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    wifiManager.resetSettings();
    delay(300);
    Serial.println("[WiFi reset] Kredensial terhapus. ESP32 restart.");
    ESP.restart();
  }
}
