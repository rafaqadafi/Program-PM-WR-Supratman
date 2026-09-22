#include <WiFi.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "button/wifi_reset_button.h"
#include "hermes/hermes_alert.h"
#include "led/led_status.h"
#include "modbus/modbus_rtu.h"
#include "mqtt/mqtt_service.h"
#include "ota/ota_service.h"
#include "thingspeak/thingspeak_service.h"
#include "wifi/wifi_service.h"

void setup() {
  Serial.begin(Config::Project::SERIAL_BAUD);
  const uint32_t serialWaitStarted = millis();
  while (!Serial && millis() - serialWaitStarted < 3000UL) {
    delay(10);
  }
  delay(500);
  Serial.printf("\nESP32 %s - FreeRTOS\n", Config::Project::NAME);
  Serial.println("Ketik RW: reset WiFi | TW: test Watchdog freeze | TA: test alert Hermes");

  ledStatusBegin();
  wifiResetButtonBegin();
  wifiServiceBegin();

  // Menunggu koneksi WiFi dan IP valid sebelum program lain boleh berjalan
  Serial.println("[System] Menunggu koneksi WiFi...");
  while (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    if (Serial.available() > 0) {
      const char c = static_cast<char>(Serial.read());
      if (c == 'R' || c == 'r') {
        Serial.println("[Serial] Perintah reset WiFi...");
        wifiServiceResetSettingsAndRestart();
      }
    }
    delay(100);
  }
  Serial.println("[System] WiFi terhubung! Menjalankan layanan lainnya...");

  hermesAlertBegin();
  modbusRtuBegin();
  mqttServiceBegin();
  thingspeakServiceBegin();
  otaServiceBegin();

  // Hardware Task Watchdog Timer diaktifkan setelah seluruh layanan siap
  esp_task_wdt_init(Config::Watchdog::TIMEOUT_SECONDS, true);
  esp_task_wdt_add(NULL);
}

void loop() {
  static char previousCommandChar = 0;

  while (Serial.available() > 0) {
    const char received = static_cast<char>(Serial.read());

    if (previousCommandChar == 'R' && (received == 'W' || received == 'w')) {
      Serial.println("[Serial] Perintah RW: reset konfigurasi WiFi...");
      wifiServiceResetSettingsAndRestart();
      previousCommandChar = 0;
      return;
    }

    if (previousCommandChar == 'T' && (received == 'W' || received == 'w')) {
      Serial.println("[Serial] Perintah TW: Membekukan loop untuk menguji Task Watchdog Timer...");
      while (true) {
        // Sengaja freeze tanpa esp_task_wdt_reset() sampai WDT trigger reset
      }
    }

    if (previousCommandChar == 'T' && (received == 'A' || received == 'a')) {
      Serial.println("[Serial] Perintah TA: Mengirim test alert ke Hermes...");
      hermesSendAlert("MANUAL_TEST_ALERT", "Pengujian pengiriman alert manual dari Serial");
      previousCommandChar = 0;
      return;
    }

    if (received == 'R' || received == 'r') {
      previousCommandChar = 'R';
    } else if (received == 'T' || received == 't') {
      previousCommandChar = 'T';
    } else if (received != '\r' && received != '\n') {
      previousCommandChar = 0;
    }
  }

  // Feed watchdog timer
  esp_task_wdt_reset();

  // Semua pekerjaan utama dijalankan oleh task FreeRTOS masing-masing.
  // Loop tetap periodik agar dapat menerima perintah dari Serial Monitor.
  vTaskDelay(pdMS_TO_TICKS(20));
}
