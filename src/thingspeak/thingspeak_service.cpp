#include "thingspeak_service.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <string.h>

#include "config.h"

namespace {
constexpr char THINGSPEAK_HOST[] = "api.thingspeak.com";
constexpr uint16_t THINGSPEAK_PORT = 443;

float averageVoltage(const MeterData &data) {
  return (data.phase[0].voltage + data.phase[1].voltage +
          data.phase[2].voltage) /
         3.0f;
}
}  // namespace

bool thingSpeakUpdate(const MeterData &data) {
  if (!Config::ThingSpeak::ENABLED ||
      strlen(Config::ThingSpeak::WRITE_API_KEY) == 0 ||
      strcmp(Config::ThingSpeak::WRITE_API_KEY, "ISI_WRITE_API_KEY") == 0) {
    return false;
  }

  if (!data.online || !data.dataReady || WiFi.status() != WL_CONNECTED) {
    return false;
  }

  WiFiClientSecure client;
  // Enkripsi HTTPS aktif; validasi CA dapat ditambahkan jika sertifikat CA
  // root disimpan di konfigurasi produk.
  client.setInsecure();
  client.setTimeout(10000);
  if (!client.connect(THINGSPEAK_HOST, THINGSPEAK_PORT)) {
    Serial.println("[ThingSpeak] Koneksi HTTPS gagal");
    return false;
  }

  char request[640];
  const int length = snprintf(
      request, sizeof(request),
      "GET /update?api_key=%s&field1=%.3f&field2=%.1f&field3=%.1f&field4=%.3f"
      "&field5=%.3f&field6=%.3f&field7=%.3f&field8=%.2f HTTP/1.1\r\n"
      "Host: %s\r\nConnection: close\r\n\r\n",
      Config::ThingSpeak::WRITE_API_KEY, data.totalEnergy, data.totalActive,
      data.totalApparent, data.powerFactor, data.phase[0].current,
      data.phase[1].current, data.phase[2].current, averageVoltage(data),
      THINGSPEAK_HOST);

  if (length < 0 || length >= static_cast<int>(sizeof(request))) {
    client.stop();
    Serial.println("[ThingSpeak] Request terlalu panjang");
    return false;
  }

  client.print(request);
  char statusLine[64] = {};
  const size_t statusLength =
      client.readBytesUntil('\n', statusLine, sizeof(statusLine) - 1);
  statusLine[statusLength] = '\0';
  const bool accepted = strstr(statusLine, " 200 ") != nullptr;

  const uint32_t startedAt = millis();
  while (client.connected() && millis() - startedAt < 10000UL) {
    while (client.available()) client.read();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  client.stop();
  Serial.printf("[ThingSpeak] Update 8 field %s\n",
                accepted ? "berhasil" : "ditolak");
  return accepted;
}
