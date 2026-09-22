#include "mqtt_service.h"

#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <mqtt_client.h>
#include <string.h>

#include "config.h"
#include "led/led_status.h"
#include "modbus/modbus_rtu.h"

extern "C" esp_err_t esp_crt_bundle_attach(void *conf);

// WiFiClientSecure membawa header bernama sama yang menutupi header bundle CA
// bawaan ESP-IDF. Simbol ini tetap tersedia dari komponen mbedTLS framework.
extern "C" esp_err_t esp_crt_bundle_attach(void *conf);

namespace {
esp_mqtt_client_handle_t mqttClient = nullptr;
std::atomic_bool mqttConnected{false};
char clientId[48];
char statusTopic[96];

void mqttEventHandler(void *, esp_event_base_t, int32_t eventId,
                      void *eventData) {
  auto *event = static_cast<esp_mqtt_event_handle_t>(eventData);

  switch (static_cast<esp_mqtt_event_id_t>(eventId)) {
    case MQTT_EVENT_CONNECTED: {
      mqttConnected.store(true);
      ledStatusSetMqttConnected(true);
      const int messageId = esp_mqtt_client_publish(
          event->client, statusTopic, "{\"status\":\"ONLINE\"}", 0, 0, 1);
      Serial.printf("[MQTT] Terhubung ke broker WSS (RSSI: %d dBm); status %s\n",
                    WiFi.RSSI(), messageId >= 0 ? "terkirim" : "gagal dikirim");
      break;
    }

    case MQTT_EVENT_DISCONNECTED:
      mqttConnected.store(false);
      ledStatusSetMqttConnected(false);
      Serial.println("[MQTT] Terputus dari broker; mencoba kembali...");
      break;

    case MQTT_EVENT_ERROR:
      mqttConnected.store(false);
      ledStatusSetMqttConnected(false);
      if (event->error_handle == nullptr) {
        Serial.println("[MQTT] Koneksi gagal tanpa detail error");
      } else if (event->error_handle->error_type ==
                 MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        Serial.printf("[MQTT] Broker menolak koneksi, kode=%d\n",
                      event->error_handle->connect_return_code);
      } else {
        Serial.printf(
            "[MQTT] Transport/TLS gagal, esp=0x%X tls=0x%X verify=0x%X "
            "socket=%d\n",
            static_cast<unsigned>(event->error_handle->esp_tls_last_esp_err),
            static_cast<unsigned>(event->error_handle->esp_tls_stack_err),
            static_cast<unsigned>(
                event->error_handle->esp_tls_cert_verify_flags),
            event->error_handle->esp_transport_sock_errno);
      }
      break;

    default:
      break;
  }
}

bool startMqttClient() {
  const uint64_t chipId = ESP.getEfuseMac();

  snprintf(clientId, sizeof(clientId), "%s-%04X%08X",
           Config::Mqtt::CLIENT_PREFIX,
           static_cast<uint16_t>(chipId >> 32),
           static_cast<uint32_t>(chipId));
  snprintf(statusTopic, sizeof(statusTopic), "%s/status",
           Config::Mqtt::BASE_TOPIC);

  esp_mqtt_client_config_t mqttConfig{};
  mqttConfig.uri = Config::Mqtt::URI;
  mqttConfig.client_id = clientId;
  mqttConfig.username = strlen(Config::Mqtt::USER) > 0
                            ? Config::Mqtt::USER
                            : nullptr;
  mqttConfig.password = strlen(Config::Mqtt::PASSWORD) > 0
                            ? Config::Mqtt::PASSWORD
                            : nullptr;
  mqttConfig.lwt_topic = statusTopic;
  mqttConfig.lwt_msg = "{\"status\":\"OFFLINE\"}";
  mqttConfig.lwt_qos = 0;
  mqttConfig.lwt_retain = 1;
  mqttConfig.keepalive = 30;
  mqttConfig.disable_auto_reconnect = false;
  mqttConfig.buffer_size = 2048;
  mqttConfig.out_buffer_size = 2048;
  mqttConfig.crt_bundle_attach = esp_crt_bundle_attach;
  mqttConfig.reconnect_timeout_ms = Config::Mqtt::RECONNECT_INTERVAL_MS;
  mqttConfig.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
  mqttConfig.skip_cert_common_name_check = false;
  mqttConfig.network_timeout_ms = 10000;

  mqttClient = esp_mqtt_client_init(&mqttConfig);
  if (mqttClient == nullptr) {
    Serial.println("[MQTT] Gagal membuat client");
    return false;
  }

  if (esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_ANY,
                                     mqttEventHandler, nullptr) != ESP_OK) {
    Serial.println("[MQTT] Gagal memasang event handler");
    esp_mqtt_client_destroy(mqttClient);
    mqttClient = nullptr;
    return false;
  }

  if (esp_mqtt_client_start(mqttClient) != ESP_OK) {
    Serial.println("[MQTT] Gagal memulai client");
    esp_mqtt_client_destroy(mqttClient);
    mqttClient = nullptr;
    return false;
  }

  Serial.printf("[MQTT] Menghubungkan ke %s\n", Config::Mqtt::URI);
  return true;
}

void stopAndDestroyMqttClient() {
  if (mqttClient != nullptr) {
    esp_mqtt_client_stop(mqttClient);
    esp_mqtt_client_destroy(mqttClient);
    mqttClient = nullptr;
  }
  mqttConnected.store(false);
  ledStatusSetMqttConnected(false);
}

bool publishMeterData(const MeterData &data) {
  if (!mqttConnected.load() || mqttClient == nullptr || !data.online ||
      !data.dataReady) {
    return false;
  }

  char topic[96];
  snprintf(topic, sizeof(topic), "%s/telemetry", Config::Mqtt::BASE_TOPIC);

  char payload[768];
  const int length = snprintf(
      payload, sizeof(payload),
      "{\"phaseA\":{\"voltage\":%.1f,\"current\":%.2f,\"frequency\":%.2f,"
      "\"phaseAngle\":0.0,\"activePower\":%.3f,\"reactivePower\":%.3f,"
      "\"apparentPower\":%.3f,\"energy\":0.0},"
      "\"phaseB\":{\"voltage\":%.1f,\"current\":%.2f,\"frequency\":%.2f,"
      "\"phaseAngle\":0.0,\"activePower\":%.3f,\"reactivePower\":%.3f,"
      "\"apparentPower\":%.3f,\"energy\":0.0},"
      "\"phaseC\":{\"voltage\":%.1f,\"current\":%.2f,\"frequency\":%.2f,"
      "\"phaseAngle\":0.0,\"activePower\":%.3f,\"reactivePower\":%.3f,"
      "\"apparentPower\":%.3f,\"energy\":0.0},"
      "\"total\":{\"activePower\":%.3f,\"reactivePower\":%.3f,"
      "\"apparentPower\":%.3f,\"powerFactor\":%.2f,\"energy\":%.3f}}",
      data.phase[0].voltage, data.phase[0].current,
      data.phase[0].frequency,
      data.phase[0].activePower / Config::Mqtt::POWER_DIVISOR,
      data.phase[0].reactivePower / Config::Mqtt::POWER_DIVISOR,
      data.phase[0].apparentPower / Config::Mqtt::POWER_DIVISOR,
      data.phase[1].voltage, data.phase[1].current,
      data.phase[1].frequency,
      data.phase[1].activePower / Config::Mqtt::POWER_DIVISOR,
      data.phase[1].reactivePower / Config::Mqtt::POWER_DIVISOR,
      data.phase[1].apparentPower / Config::Mqtt::POWER_DIVISOR,
      data.phase[2].voltage, data.phase[2].current,
      data.phase[2].frequency,
      data.phase[2].activePower / Config::Mqtt::POWER_DIVISOR,
      data.phase[2].reactivePower / Config::Mqtt::POWER_DIVISOR,
      data.phase[2].apparentPower / Config::Mqtt::POWER_DIVISOR,
      data.totalActive / Config::Mqtt::POWER_DIVISOR,
      data.totalReactive / Config::Mqtt::POWER_DIVISOR,
      data.totalApparent / Config::Mqtt::POWER_DIVISOR,
      data.powerFactor,
      data.totalEnergy);

  if (length < 0 || length >= static_cast<int>(sizeof(payload))) {
    Serial.println("[MQTT] Payload overflow");
    return false;
  }

  ledStatusNotifyUpload();
  const bool published =
      esp_mqtt_client_publish(mqttClient, topic, payload, length, 0, 1) >= 0;
  Serial.printf("[MQTT] Publish %s: %s (RSSI: %d dBm)\n", topic,
                published ? "berhasil" : "gagal", WiFi.RSSI());
  if (published) ledStatusNotifyUploadSuccess();
  return published;
}

void mqttTask(void *) {
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(250));
  }

  while (!startMqttClient()) {
    Serial.printf("[MQTT] Inisialisasi diulang dalam %lu ms\n",
                  static_cast<unsigned long>(
                      Config::Mqtt::RECONNECT_INTERVAL_MS));
    vTaskDelay(pdMS_TO_TICKS(Config::Mqtt::RECONNECT_INTERVAL_MS));
  }

  uint32_t publishedSequence = 0;
  bool previousWifiConnected = true;
  uint32_t continuousDisconnectStart = 0;

  for (;;) {
    const bool currentWifiConnected = (WiFi.status() == WL_CONNECTED);

    // Deteksi WiFi baru saja pulih
    if (currentWifiConnected && !previousWifiConnected) {
      Serial.println("[MQTT] WiFi pulih, memicu reconnect MQTT segera...");
      if (mqttClient != nullptr && !mqttConnected.load()) {
        esp_mqtt_client_reconnect(mqttClient);
      }
    }
    previousWifiConnected = currentWifiConnected;

    // Fail-safe Reboot jika terputus terus-menerus > 10 menit padahal WiFi aktif
    if (currentWifiConnected) {
      if (mqttConnected.load()) {
        continuousDisconnectStart = 0;
      } else {
        if (continuousDisconnectStart == 0) {
          continuousDisconnectStart = millis();
        }
        if (millis() - continuousDisconnectStart >=
            Config::Mqtt::WATCHDOG_REBOOT_MS) {
          Serial.println(
              "[MQTT] Terputus >10 menit berturut-turut. Memulai reboot preventif...");
          vTaskDelay(pdMS_TO_TICKS(1000));
          ESP.restart();
        }
      }
    } else {
      continuousDisconnectStart = 0;
    }

    if (mqttConnected.load()) {
      MeterData data{};
      uint32_t sequence = 0;
      if (modbusRtuGetLatest(data, sequence) && sequence != publishedSequence &&
          publishMeterData(data)) {
        publishedSequence = sequence;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}  // namespace

void mqttServiceBegin() {
  xTaskCreatePinnedToCore(mqttTask, "MQTT", 8192, nullptr, 2, nullptr, 1);
}
