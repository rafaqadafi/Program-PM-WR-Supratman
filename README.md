# IoT Power Meter Gateway (PM W.R. Supratman)

Firmware IoT berbasis **ESP32** (NodeMCU-32S) dan **FreeRTOS** untuk membaca parameter kelistrikan dari digital power meter via industri **Modbus RS-485 (RTU)**, kemudian mendistribusikan data secara real-time ke broker **MQTT over Secure WebSocket (WSS)**, cloud **ThingSpeak**, serta sistem alert darurat ke **Telegram Topic / AI Agent Hermes**.

---

## 🚀 Fitur Utama

- **Modbus RTU RS-485**: Polling register tegangan 3-fasa, arus 3-fasa, daya aktif/reaktif/semu, power factor, frekuensi, dan total energi (kWh).
- **MQTT over Secure WebSocket (WSS)**: Mengirim data telemetry berformat JSON ke broker.
- **ThingSpeak Cloud Logging**: Backup logging periodik 8 field metrik listrik via HTTPS POST.
- **Hardware Task Watchdog Timer (TWDT)**: Perlindungan freeze/lockup 15 detik menggunakan native ESP-IDF `<esp_task_wdt.h>` dengan reboot otomatis.
- **Deteksi Reboot & Emergency Alert Webhook**: Membaca `esp_reset_reason()` saat booting. Jika terjadi reboot akibat Watchdog (`ESP_RST_TASK_WDT`), Panic/Crash (`ESP_RST_PANIC`), atau Brownout, sistem langsung mengirim payload HTTPS POST ke webhook yang diteruskan ke topik khusus Telegram (`🚨 ESP32 Alerts`).
- **Over-The-Air (OTA) Update**: Pembaruan firmware nirkabel via jaringan lokal (`ArduinoOTA`) menggunakan skema partisi dual-slot `min_spiffs.csv`.
- **WiFi Captive Portal**: Auto-connect ke WiFi tersimpan; jika gagal, ESP32 mengaktifkan portal AP konfigurasi mandiri via `WiFiManager`.
- **Indikator Visual RGB LED**:
  - 🔴 **Merah**: Terjadi error (WiFi terputus, Modbus gagal membaca, atau MQTT terputus).
  - 🟢 **Hijau**: Sistem berjalan normal dan sehat.
  - 🔵 **Biru**: Pulse aktif saat proses pengiriman telemetry (MQTT / ThingSpeak).

---

## 🛠️ Pinout Hardware (NodeMCU-32S)

| Komponen | Pin ESP32 | Keterangan |
|---|---|---|
| **RS-485 RO (Receiver Output)** | GPIO 16 (RX2) | Serial UART2 |
| **RS-485 DI (Driver Input)** | GPIO 17 (TX2) | Serial UART2 |
| **RGB LED - Red** | GPIO 25 | Active LOW (Common Anode) |
| **RGB LED - Green** | GPIO 26 | Active LOW (Common Anode) |
| **RGB LED - Blue** | GPIO 27 | Active LOW (Common Anode) |
| **Tombol Reset WiFi** | GPIO 4 | INPUT_PULLUP (Hubungkan ke GND) |

---

## 📂 Struktur Proyek

```text
├── include/                  # Header include bawaan
├── src/
│   ├── config.h              # Konfigurasi hardware, register, dan interval
│   ├── secret.h.example      # Template kredensial publik aman
│   ├── main.cpp              # Setup, Task WDT, dan loop serial command
│   ├── button/               # Handler tombol fisik reset WiFi (GPIO 4)
│   ├── hermes/               # HTTP client webhook alert & crash detection
│   ├── led/                  # State-machine indikator status RGB LED
│   ├── modbus/               # Polling & decoding register Modbus RTU RS-485
│   ├── mqtt/                 # Client native ESP-IDF MQTT over WSS (port 443)
│   ├── ota/                  # Service background ArduinoOTA
│   ├── thingspeak/           # Client HTTPS upload ke ThingSpeak
│   └── wifi/                 # WiFiManager & background reconnection task
└── platformio.ini            # Konfigurasi PlatformIO & environment upload
```

---

## ⚙️ Petunjuk Instalasi & Setup

### 1. Salin File Kredensial
Salin file template kredensial dan sesuaikan nilainya:
```bash
cp src/secret.h.example src/secret.h
```
Isi variabel di dalam [`src/secret.h`](src/secret.h):
```cpp
namespace Secret {
constexpr char MQTT_URI[] = "mqtt://broker.example.com:1883";
constexpr char MQTT_SERVER[] = "broker.example.com";
constexpr uint16_t MQTT_PORT = 1883;
constexpr char MQTT_USER[] = "username";
constexpr char MQTT_PASSWORD[] = "password";
constexpr char MQTT_BASE_TOPIC[] = "device/topic";
constexpr char MQTT_CLIENT_PREFIX[] = "client-id";

constexpr char THINGSPEAK_WRITE_API_KEY[] = "API_KEY";
constexpr char HERMES_ENDPOINT_URL[] = "https://example.com/api/webhook";
constexpr char HERMES_API_KEY[] = "API_KEY";
constexpr uint32_t TELEGRAM_THREAD_ID = 0;

constexpr char OTA_HOSTNAME[] = "ESP32-Device";
constexpr char OTA_PASSWORD[] = "";
constexpr uint16_t OTA_PORT = 3232;
}
```

### 2. Konfigurasi Sistem
Atur alamat slave Modbus, endpoint, dan interval di [`src/config.h`](src/config.h).

### 3. Kompilasi & Upload Firmware

**Upload via USB:**
```bash
pio run -e nodemcu-32s -t upload
```

**Upload via OTA (Wireless):**
```bash
pio run -e nodemcu-32s-ota -t upload
```

---

## 💻 Perintah Serial Monitor (115200 Baud)

Anda dapat mengetik perintah langsung di Serial Monitor:
- **`RW`** : Menghapus kredensial WiFi dari NVS flash dan me-restart ESP32 ke mode AP portal.
- **`TA`** : Mengirim uji coba alert manual ke Webhook / Telegram topic.
- **`TW`** : Mensimulasikan freeze CPU untuk menguji restart otomatis Task Watchdog Timer (WDT) dan verifikasi pelaporan alert reboot.

---

## 📄 Lisensi
Hak Cipta © 2026 Ahmad Rafa Khadafi. Seluruh hak cipta dilindungi undang-undang.
