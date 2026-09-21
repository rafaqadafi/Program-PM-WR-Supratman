#include "hermes_alert.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>

#include "config.h"

namespace {
struct AlertMessage {
  char alert[96];
  char detail[160];
};

esp_reset_reason_t bootResetReason = ESP_RST_UNKNOWN;
bool pendingBootAlert = false;
QueueHandle_t alertQueue = nullptr;

const char *resetReasonToString(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "ESP_RST_POWERON (Power-on)";
    case ESP_RST_EXT:       return "ESP_RST_EXT (External pin reset / EN)";
    case ESP_RST_SW:        return "ESP_RST_SW (Software restart)";
    case ESP_RST_PANIC:     return "ESP_RST_PANIC (Crash / Guru Meditation)";
    case ESP_RST_INT_WDT:   return "ESP_RST_INT_WDT (Interrupt Watchdog)";
    case ESP_RST_TASK_WDT:  return "ESP_RST_TASK_WDT (Task Watchdog Timer)";
    case ESP_RST_WDT:       return "ESP_RST_WDT (Other Watchdog)";
    case ESP_RST_BROWNOUT:  return "ESP_RST_BROWNOUT (Tegangan drop)";
    case ESP_RST_DEEPSLEEP: return "ESP_RST_DEEPSLEEP";
    default:                return "ESP_RST_UNKNOWN";
  }
}

bool postToHermes(const char *alert, const char *detail, const char *reasonStr) {
  if (!Config::Hermes::ENABLED || strlen(Config::Hermes::ENDPOINT_URL) == 0 ||
      strstr(Config::Hermes::ENDPOINT_URL, "your-hermes") != nullptr) {
    Serial.println("[Hermes] Endpoint URL belum disetel di config.h");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  WiFiClientSecure secureClient;
  WiFiClient plainClient;

  const bool isHttps = (strncmp(Config::Hermes::ENDPOINT_URL, "https://", 8) == 0);
  if (isHttps) {
    secureClient.setInsecure();
    secureClient.setTimeout(Config::Hermes::TIMEOUT_MS / 1000);
    http.begin(secureClient, Config::Hermes::ENDPOINT_URL);
  } else {
    plainClient.setTimeout(Config::Hermes::TIMEOUT_MS / 1000);
    http.begin(plainClient, Config::Hermes::ENDPOINT_URL);
  }

  http.setTimeout(Config::Hermes::TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");

  if (strlen(Config::Hermes::API_KEY) > 0) {
    char authHeader[160];
    snprintf(authHeader, sizeof(authHeader), "Bearer %s", Config::Hermes::API_KEY);
    http.addHeader("Authorization", authHeader);
    http.addHeader("X-API-Key", Config::Hermes::API_KEY);
  }

  char jsonPayload[512];
  if (Config::Hermes::TELEGRAM_THREAD_ID > 0) {
    snprintf(jsonPayload, sizeof(jsonPayload),
             "{\"device\":\"%s\",\"alert\":\"%s\",\"reset_reason\":\"%s\",\"detail\":\"%s\","
             "\"thread_id\":%u,\"message_thread_id\":%u}",
             Config::Project::NAME, alert, reasonStr ? reasonStr : "-", detail,
             Config::Hermes::TELEGRAM_THREAD_ID, Config::Hermes::TELEGRAM_THREAD_ID);
  } else {
    snprintf(jsonPayload, sizeof(jsonPayload),
             "{\"device\":\"%s\",\"alert\":\"%s\",\"reset_reason\":\"%s\",\"detail\":\"%s\"}",
             Config::Project::NAME, alert, reasonStr ? reasonStr : "-", detail);
  }

  Serial.printf("[Hermes] Mengirim alert ke %s ...\n", Config::Hermes::ENDPOINT_URL);
  const int httpCode = http.POST(reinterpret_cast<uint8_t *>(jsonPayload), strlen(jsonPayload));
  if (httpCode > 0) {
    Serial.printf("[Hermes] Respon HTTP: %d\n", httpCode);
  } else {
    Serial.printf("[Hermes] Gagal mengirim: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
  return httpCode >= 200 && httpCode < 300;
}

void hermesTask(void *) {
  // Tunggu hingga WiFi terhubung
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  // Jika booting sebelumnya disebabkan crash / WDT restart
  if (pendingBootAlert) {
    vTaskDelay(pdMS_TO_TICKS(1500));  // Tunggu koneksi stabil
    const char *reasonStr = resetReasonToString(bootResetReason);
    postToHermes("WATCHDOG_OR_ABNORMAL_RESET",
                 "ESP32 restart otomatis terdeteksi akibat Watchdog atau Panic",
                 reasonStr);
    pendingBootAlert = false;
  }

  AlertMessage msg;
  for (;;) {
    if (xQueueReceive(alertQueue, &msg, portMAX_DELAY) == pdTRUE) {
      if (WiFi.status() == WL_CONNECTED) {
        postToHermes(msg.alert, msg.detail, nullptr);
      }
    }
  }
}
}  // namespace

void hermesAlertBegin() {
  bootResetReason = esp_reset_reason();
  Serial.printf("[System] Reset reason: %s\n", resetReasonToString(bootResetReason));

  if (bootResetReason == ESP_RST_TASK_WDT ||
      bootResetReason == ESP_RST_INT_WDT ||
      bootResetReason == ESP_RST_WDT ||
      bootResetReason == ESP_RST_PANIC ||
      bootResetReason == ESP_RST_BROWNOUT) {
    pendingBootAlert = true;
    Serial.println("[Hermes] Restart abnormal/WDT terdeteksi, alert dijadwalkan saat WiFi online.");
  }

  alertQueue = xQueueCreate(4, sizeof(AlertMessage));
  xTaskCreate(hermesTask, "HermesAlert", 4096, nullptr, 1, nullptr);
}

void hermesSendAlert(const char *alert, const char *detail) {
  if (alertQueue == nullptr || alert == nullptr) return;
  AlertMessage msg{};
  strncpy(msg.alert, alert, sizeof(msg.alert) - 1);
  if (detail != nullptr) {
    strncpy(msg.detail, detail, sizeof(msg.detail) - 1);
  }
  xQueueSend(alertQueue, &msg, 0);
}
