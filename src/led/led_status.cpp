#include "led_status.h"

#include <Arduino.h>
#include <atomic>

#include "config.h"

namespace {
struct LedState {
  bool wifiConnected = false;
  bool modbusHealthy = false;
  bool mqttConnected = false;
  bool thingspeakHealthy = true;
};

SemaphoreHandle_t ledStateMutex = nullptr;
LedState ledState;
std::atomic_bool uploadingActive{false};
std::atomic_uint32_t uploadPulseUntil{0};

LedState getLedState() {
  LedState state;
  if (ledStateMutex != nullptr &&
      xSemaphoreTake(ledStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    state = ledState;
    xSemaphoreGive(ledStateMutex);
  }
  return state;
}

void setLedState(bool LedState::*field, bool value) {
  if (ledStateMutex == nullptr) return;
  if (xSemaphoreTake(ledStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    ledState.*field = value;
    xSemaphoreGive(ledStateMutex);
  }
}

void writeColor(uint8_t red, uint8_t green, uint8_t blue) {
  const auto output = [](uint8_t pin, uint8_t brightness) {
    const uint8_t duty = Config::Pins::RGB_COMMON_ANODE
                             ? static_cast<uint8_t>(255 - brightness)
                             : brightness;
    analogWrite(pin, duty);
  };
  output(Config::Pins::RGB_RED, red);
  output(Config::Pins::RGB_GREEN, green);
  output(Config::Pins::RGB_BLUE, blue);
}

void ledTask(void *) {
  uint8_t lastRed = 0;
  uint8_t lastGreen = 0;
  uint8_t lastBlue = 0;

  for (;;) {
    const LedState state = getLedState();
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    const bool isUploading = uploadingActive.load() ||
                             (millis() < uploadPulseUntil.load());
    const bool hasError = !state.wifiConnected || !state.modbusHealthy ||
                          !state.mqttConnected || !state.thingspeakHealthy;

    // Aturan warna LED:
    // 1. Blue: saat ada proses upload (MQTT atau ThingSpeak)
    // 2. Red: saat ada error (error apapun)
    // 3. Green: saat program berjalan normal
    if (isUploading) {
      blue = Config::Led::BRIGHTNESS;
    } else if (hasError) {
      red = Config::Led::BRIGHTNESS;
    } else {
      green = Config::Led::BRIGHTNESS;
    }

    if (red != lastRed || green != lastGreen || blue != lastBlue) {
      writeColor(red, green, blue);
      lastRed = red;
      lastGreen = green;
      lastBlue = blue;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
}  // namespace

void ledStatusBegin() {
  pinMode(Config::Pins::RGB_RED, OUTPUT);
  pinMode(Config::Pins::RGB_GREEN, OUTPUT);
  pinMode(Config::Pins::RGB_BLUE, OUTPUT);
  writeColor(0, 0, 0);

  ledStateMutex = xSemaphoreCreateMutex();
  xTaskCreate(ledTask, "RGB LED status", 2048, nullptr, 1, nullptr);
}

void ledStatusSetWifiConnected(bool connected) {
  setLedState(&LedState::wifiConnected, connected);
}

void ledStatusSetModbusHealthy(bool healthy) {
  setLedState(&LedState::modbusHealthy, healthy);
}

void ledStatusSetMqttConnected(bool connected) {
  setLedState(&LedState::mqttConnected, connected);
}

void ledStatusSetThingSpeakHealthy(bool healthy) {
  setLedState(&LedState::thingspeakHealthy, healthy);
}

void ledStatusSetUploading(bool uploading) {
  uploadingActive.store(uploading);
  if (uploading) {
    uploadPulseUntil.store(millis() + 500);
  }
}

void ledStatusNotifyUpload() {
  uploadPulseUntil.store(millis() + 500);
}

void ledStatusNotifyUploadSuccess() {}
