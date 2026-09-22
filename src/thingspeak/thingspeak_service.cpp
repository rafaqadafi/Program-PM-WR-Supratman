#include "thingspeak_service.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <string.h>

#include "config.h"
#include "led/led_status.h"
#include "modbus/modbus_rtu.h"

namespace {
float averageVoltage(const MeterData &data) {
  return (data.phase[0].voltage + data.phase[1].voltage +
          data.phase[2].voltage) /
         3.0f;
}

bool thingSpeakUpload(const MeterData &data) {
  WiFiClient client;
  client.setTimeout(5000);

  char url[512];
  const int length = snprintf(
      url, sizeof(url),
      "http://api.thingspeak.com/update?api_key=%s&field1=%.3f&field2=%.1f"
      "&field3=%.1f&field4=%.3f&field5=%.3f&field6=%.3f&field7=%.3f&field8=%.2f",
      Config::ThingSpeak::WRITE_API_KEY, data.totalEnergy, data.totalActive,
      data.totalApparent, data.powerFactor, data.phase[0].current,
      data.phase[1].current, data.phase[2].current, averageVoltage(data));

  if (length < 0 || length >= static_cast<int>(sizeof(url))) {
    Serial.println("[ThingSpeak] URL terlalu panjang");
    return false;
  }

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("[ThingSpeak] Gagal inisialisasi HTTPClient");
    return false;
  }
  http.setTimeout(10000);

  const int httpCode = http.GET();
  String response = (httpCode > 0) ? http.getString() : "";
  http.end();

  response.trim();
  const long entryId = response.toInt();
  const bool success = (httpCode == HTTP_CODE_OK && entryId > 0);

  if (success) {
    Serial.printf("[ThingSpeak] Update 8 field berhasil (Entry ID: %ld)\n", entryId);
  } else if (httpCode == HTTP_CODE_OK && entryId == 0) {
    Serial.println("[ThingSpeak] Ditolak oleh server (Rate limit / interval < 15 detik)");
  } else if (httpCode < 0) {
    Serial.printf("[ThingSpeak] Gagal koneksi HTTP: %s\n", http.errorToString(httpCode).c_str());
  } else {
    Serial.printf("[ThingSpeak] Gagal, HTTP respon: %d\n", httpCode);
  }

  return success;
}

void thingspeakTask(void *) {
  // Tunggu hingga WiFi terhubung
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  // Tunda eksekusi pertama 30 detik agar MQTT dapat terhubung lebih dahulu tanpa rebutan memori TLS
  vTaskDelay(pdMS_TO_TICKS(30000));

  for (;;) {
    if (Config::ThingSpeak::ENABLED &&
        strlen(Config::ThingSpeak::WRITE_API_KEY) > 0 &&
        strcmp(Config::ThingSpeak::WRITE_API_KEY, "ISI_WRITE_API_KEY") != 0 &&
        WiFi.status() == WL_CONNECTED) {

      MeterData data{};
      uint32_t sequence = 0;
      if (modbusRtuGetLatest(data, sequence) && data.online && data.dataReady) {
        ledStatusSetUploading(true);
        const bool tsSuccess = thingSpeakUpload(data);
        ledStatusSetUploading(false);
        ledStatusSetThingSpeakHealthy(tsSuccess);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(Config::ThingSpeak::UPDATE_INTERVAL_MS));
  }
}
}  // namespace

void thingspeakServiceBegin() {
  // Task ThingSpeak berjalan independen di Core 0 Priority 1
  // Tidak membebani Core 1 yang didedikasikan untuk Modbus RTU dan MQTT
  xTaskCreatePinnedToCore(thingspeakTask, "ThingSpeak", 8192, nullptr, 1, nullptr, 0);
}
