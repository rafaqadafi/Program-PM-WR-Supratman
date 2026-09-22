#pragma once

#include <Arduino.h>
#include "secret.h"

// ================================================================
// KONFIGURASI YANG BOLEH DIUBAH
// Semua pengaturan hardware, jaringan, dan register dikumpulkan di sini.
// ================================================================
namespace Config {

namespace Project {
constexpr char NAME[] = "PM W.R. Supratman";
constexpr uint32_t SERIAL_BAUD = 115200;
}  // namespace Project

namespace Pins {
// ESP32 NodeMCU-32S: UART2 untuk Modbus RS-485.
// Hubungkan RO modul RS-485 ke RX dan DI modul ke TX.
constexpr uint8_t MODBUS_RX = 16;
constexpr uint8_t MODBUS_TX = 17;
constexpr uint8_t WIFI_RESET_BUTTON = 4;  // Tombol ke GND (INPUT_PULLUP).
// RGB LED 4-kaki common-anode: kaki bersama ke +3V3, kanal aktif LOW.
constexpr uint8_t RGB_RED = 25;
constexpr uint8_t RGB_GREEN = 26;
constexpr uint8_t RGB_BLUE = 27;
constexpr bool RGB_COMMON_ANODE = true;
}  // namespace Pins

namespace Modbus {
// Sesuaikan dengan menu komunikasi pada power meter.
constexpr uint8_t SLAVE_ID = 1;
constexpr uint32_t BAUD = 9600;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 500;
constexpr uint32_t POLLING_INTERVAL_MS = 5000;
constexpr uint32_t SERIAL_FORMAT = SERIAL_8N1;  // 8 data, no parity, 1 stop.
}  // namespace Modbus

namespace Register {
// Alamat berikut berasal dari manual PM W.R. Supratman.
// Ganti bagian ini jika tipe power meter berubah.
constexpr uint16_t ELECTRICAL_BLOCK_START = 0x0041;
constexpr uint16_t ELECTRICAL_BLOCK_COUNT = 64;  // Sampai 0x0080.
constexpr uint16_t CONSUMED_ENERGY = 0x008A;

constexpr uint16_t VOLTAGE[3] = {0x0042, 0x0044, 0x0046};
constexpr uint16_t CURRENT[3] = {0x0058, 0x005A, 0x005C};
constexpr uint16_t ACTIVE_POWER[3] = {0x0064, 0x0066, 0x0068};
constexpr uint16_t REACTIVE_POWER[3] = {0x006C, 0x006E, 0x0070};
constexpr uint16_t APPARENT_POWER[3] = {0x0074, 0x0076, 0x0078};

constexpr uint16_t TOTAL_ACTIVE_POWER = 0x006A;
constexpr uint16_t TOTAL_REACTIVE_POWER = 0x0072;
constexpr uint16_t TOTAL_APPARENT_POWER = 0x007A;
constexpr uint16_t TOTAL_POWER_FACTOR = 0x007F;
constexpr uint16_t FREQUENCY = 0x0080;

// Nilai register energi menggunakan skala 0,001 kWh.
constexpr float ENERGY_DIVISOR = 1000.0f;
static_assert(ENERGY_DIVISOR > 0.0f, "ENERGY_DIVISOR harus lebih dari 0");

// Ubah menjadi true jika nilai energy di hardware tidak masuk akal karena
// urutan dua word float ternyata CDAB, bukan ABCD.
constexpr bool SWAP_ENERGY_WORDS = false;
}  // namespace Register

namespace WiFiPortal {
// Nama hotspot konfigurasi. Password kosong berarti portal tanpa password.
// Jika diisi, password WiFi AP minimal 8 karakter.
constexpr char NAME[] = "PM-WR-Supratman-Setup";
constexpr char PASSWORD[] = "";
constexpr uint16_t CONNECT_TIMEOUT_SECONDS = 20;

// Konfigurasi IP Statis dari src/secret.h
constexpr bool USE_STATIC_IP = Secret::USE_STATIC_IP;
constexpr const char *STATIC_IP = Secret::STATIC_IP;
constexpr const char *STATIC_GATEWAY = Secret::STATIC_GATEWAY;
constexpr const char *STATIC_SUBNET = Secret::STATIC_SUBNET;
constexpr const char *STATIC_DNS = Secret::STATIC_DNS;
}  // namespace WiFiPortal

namespace Mqtt {
// Konfigurasi broker diambil dari src/secret.h
constexpr const char *URI = Secret::MQTT_URI;
constexpr const char *SERVER = Secret::MQTT_SERVER;
constexpr uint16_t PORT = Secret::MQTT_PORT;
constexpr const char *USER = Secret::MQTT_USER;
constexpr const char *PASSWORD = Secret::MQTT_PASSWORD;
constexpr const char *BASE_TOPIC = Secret::MQTT_BASE_TOPIC;
constexpr const char *CLIENT_PREFIX = Secret::MQTT_CLIENT_PREFIX;
constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;

// Pembagi W/VAR/VA sebelum dikirim sebagai kW/kVAR/kVA.
constexpr float POWER_DIVISOR = 1000.0f;
static_assert(POWER_DIVISOR > 0.0f, "POWER_DIVISOR harus lebih dari 0");
}  // namespace Mqtt

namespace ThingSpeak {
// Diambil dari src/secret.h
constexpr bool ENABLED = true;
constexpr const char *WRITE_API_KEY = Secret::THINGSPEAK_WRITE_API_KEY;
constexpr uint32_t UPDATE_INTERVAL_MS = 60000;
// Field 1=kWh, 2=P(W), 3=S(VA), 4=PF, 5=I-A, 6=I-B, 7=I-C, 8=V rata-rata.
}  // namespace ThingSpeak

namespace Button {
constexpr uint32_t RESET_HOLD_MS = 5000;
constexpr uint32_t POLL_MS = 20;
}  // namespace Button

namespace Led {
constexpr uint8_t BRIGHTNESS = 128;  // Rentang 0-255; 128 sekitar 50%.
constexpr uint8_t SUCCESS_BLINK_COUNT = 3;
constexpr uint32_t BLINK_MS = 150;
}  // namespace Led

namespace Ota {
constexpr const char *HOSTNAME = Secret::OTA_HOSTNAME;
constexpr const char *PASSWORD = Secret::OTA_PASSWORD;
constexpr uint16_t PORT = Secret::OTA_PORT;
}  // namespace Ota

namespace Watchdog {
constexpr uint32_t TIMEOUT_SECONDS = 15;  // Auto-reset ESP32 jika freeze > 15 detik
}  // namespace Watchdog

namespace Hermes {
constexpr bool ENABLED = true;
// Konfigurasi alert & webhook diambil dari src/secret.h
constexpr const char *ENDPOINT_URL = Secret::HERMES_ENDPOINT_URL;
constexpr const char *API_KEY = Secret::HERMES_API_KEY;
constexpr uint32_t TELEGRAM_THREAD_ID = Secret::TELEGRAM_THREAD_ID;
constexpr uint32_t TIMEOUT_MS = 10000;  // 10s untuk koneksi cloud
}  // namespace Hermes

}  // namespace Config
