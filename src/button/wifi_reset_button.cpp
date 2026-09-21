#include "wifi_reset_button.h"

#include <Arduino.h>

#include "config.h"
#include "wifi/wifi_service.h"

namespace {
void resetButtonTask(void *) {
  bool wasPressed = false;
  uint32_t pressedAt = 0;

  for (;;) {
    const bool pressed = digitalRead(Config::Pins::WIFI_RESET_BUTTON) == LOW;

    if (pressed && !wasPressed) {
      wasPressed = true;
      pressedAt = millis();
      Serial.printf(
          "[Tombol] GPIO%u ditekan; tahan %lu detik untuk reset WiFi\n",
          Config::Pins::WIFI_RESET_BUTTON,
          Config::Button::RESET_HOLD_MS / 1000UL);
    } else if (!pressed && wasPressed) {
      wasPressed = false;
      Serial.println("[Tombol] Dilepas terlalu cepat; reset dibatalkan");
    }

    if (pressed && wasPressed) {
      const uint32_t elapsed = millis() - pressedAt;
      if (elapsed >= Config::Button::RESET_HOLD_MS && elapsed < (Config::Button::RESET_HOLD_MS + 60000UL)) {
        wifiServiceResetSettingsAndRestart();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(Config::Button::POLL_MS));
  }
}
}  // namespace

void wifiResetButtonBegin() {
  pinMode(Config::Pins::WIFI_RESET_BUTTON, INPUT_PULLUP);
  xTaskCreate(resetButtonTask, "Reset WiFi", 3072, nullptr, 3, nullptr);
}
