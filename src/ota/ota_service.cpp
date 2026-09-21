#include "ota_service.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>

#include "config.h"

namespace {
void otaTask(void *) {
  bool otaStarted = false;

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!otaStarted) {
        ArduinoOTA.setPort(Config::Ota::PORT);
        ArduinoOTA.setHostname(Config::Ota::HOSTNAME);

        if (strlen(Config::Ota::PASSWORD) > 0) {
          ArduinoOTA.setPassword(Config::Ota::PASSWORD);
        }

        ArduinoOTA
            .onStart([]() {
              String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch"
                                                                 : "filesystem";
              Serial.println("\n[OTA] Mulai update " + type);
            })
            .onEnd([]() {
              Serial.println("\n[OTA] Update selesai, restart ESP32...");
            })
            .onProgress([](unsigned int progress, unsigned int total) {
              Serial.printf("[OTA] Progress: %u%%\r", progress / (total / 100));
            })
            .onError([](ota_error_t error) {
              Serial.printf("[OTA] Error[%u]: ", error);
              if (error == OTA_AUTH_ERROR) Serial.println("Auth Gagal");
              else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Gagal");
              else if (error == OTA_CONNECT_ERROR) Serial.println("Koneksi Gagal");
              else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Gagal");
              else if (error == OTA_END_ERROR) Serial.println("End Gagal");
            });

        ArduinoOTA.begin();
        Serial.printf("[OTA] Siap di IP %s port %u (hostname: %s)\n",
                      WiFi.localIP().toString().c_str(), Config::Ota::PORT,
                      Config::Ota::HOSTNAME);
        otaStarted = true;
      }
      ArduinoOTA.handle();
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
}  // namespace

void otaServiceBegin() {
  xTaskCreate(otaTask, "OTAService", 4096, nullptr, 1, nullptr);
}
