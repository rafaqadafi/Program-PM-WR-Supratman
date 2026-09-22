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

  if (Config::WiFiPortal::USE_STATIC_IP) {
    IPAddress staticIP, staticGW, staticSN, staticDNS;
    if (staticIP.fromString(Config::WiFiPortal::STATIC_IP) &&
        staticGW.fromString(Config::WiFiPortal::STATIC_GATEWAY) &&
        staticSN.fromString(Config::WiFiPortal::STATIC_SUBNET) &&
        staticDNS.fromString(Config::WiFiPortal::STATIC_DNS)) {
      IPAddress staticDNS2(8, 8, 8, 8);
      wifiManager.setSTAStaticIPConfig(staticIP, staticGW, staticSN, staticDNS);
      WiFi.config(staticIP, staticGW, staticSN, staticDNS, staticDNS2);
      Serial.printf("[WiFi] IP Statis aktif: %s (GW: %s, MASK: %s, DNS: %s, 8.8.8.8)\n",
                    Config::WiFiPortal::STATIC_IP,
                    Config::WiFiPortal::STATIC_GATEWAY,
                    Config::WiFiPortal::STATIC_SUBNET,
                    Config::WiFiPortal::STATIC_DNS);
    } else {
      Serial.println("[WiFi] Format IP Statis tidak valid, fallback ke DHCP");
    }
  }

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
        Serial.println("[WiFi] Terhubung!");
        Serial.printf("[WiFi] IP      : %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] Gateway : %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("[WiFi] Subnet  : %s\n", WiFi.subnetMask().toString().c_str());
        Serial.printf("[WiFi] DNS     : %s\n", WiFi.dnsIP().toString().c_str());
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
