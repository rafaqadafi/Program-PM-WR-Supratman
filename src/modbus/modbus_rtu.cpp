#include "modbus_rtu.h"

#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "led/led_status.h"

namespace {
HardwareSerial modbusSerial(2);
SemaphoreHandle_t meterMutex = nullptr;
MeterData latestMeter{};
uint32_t latestSequence = 0;

uint16_t modbusCrc(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t position = 0; position < length; position++) {
    crc ^= data[position];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  return crc;
}

bool readHoldingRegisters(uint16_t startAddress, uint16_t count,
                          uint16_t *words) {
  if (count == 0 || count > 64 || words == nullptr) return false;

  uint8_t request[8] = {
      Config::Modbus::SLAVE_ID,
      0x03,
      static_cast<uint8_t>(startAddress >> 8),
      static_cast<uint8_t>(startAddress & 0xFF),
      static_cast<uint8_t>(count >> 8),
      static_cast<uint8_t>(count & 0xFF),
      0,
      0,
  };
  const uint16_t requestCrc = modbusCrc(request, 6);
  request[6] = requestCrc & 0xFF;
  request[7] = requestCrc >> 8;

  while (modbusSerial.available()) modbusSerial.read();
  modbusSerial.write(request, sizeof(request));
  modbusSerial.flush();

  uint8_t response[140];
  size_t length = 0;
  const size_t expectedLength = 5 + (count * 2);
  const uint32_t startedAt = millis();
  uint32_t lastByteAt = startedAt;

  while (millis() - startedAt < Config::Modbus::RESPONSE_TIMEOUT_MS) {
    while (modbusSerial.available() && length < sizeof(response)) {
      response[length++] = modbusSerial.read();
      lastByteAt = millis();
    }
    if (length >= expectedLength && millis() - lastByteAt >= 5) break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  if (length != expectedLength || response[0] != Config::Modbus::SLAVE_ID ||
      response[1] != 0x03 || response[2] != count * 2) {
    return false;
  }

  const uint16_t receivedCrc =
      response[length - 2] | (static_cast<uint16_t>(response[length - 1]) << 8);
  if (modbusCrc(response, length - 2) != receivedCrc) return false;

  for (uint16_t index = 0; index < count; index++) {
    words[index] = (static_cast<uint16_t>(response[3 + index * 2]) << 8) |
                   response[4 + index * 2];
  }
  return true;
}

uint32_t combineU32(uint16_t highWord, uint16_t lowWord) {
  return (static_cast<uint32_t>(highWord) << 16) | lowWord;
}

int32_t combineS32(uint16_t highWord, uint16_t lowWord) {
  return static_cast<int32_t>(combineU32(highWord, lowWord));
}

float combineFloat32(uint16_t highWord, uint16_t lowWord) {
  const uint32_t raw = combineU32(highWord, lowWord);
  float value;
  memcpy(&value, &raw, sizeof(value));
  return value;
}

uint16_t blockOffset(uint16_t address) {
  return address - Config::Register::ELECTRICAL_BLOCK_START;
}

bool readMeter(MeterData &data) {
  data = {};
  data.slave = Config::Modbus::SLAVE_ID;

  uint16_t electrical[Config::Register::ELECTRICAL_BLOCK_COUNT];
  uint16_t energyWords[2];

  if (!readHoldingRegisters(Config::Register::ELECTRICAL_BLOCK_START,
                            Config::Register::ELECTRICAL_BLOCK_COUNT,
                            electrical) ||
      !readHoldingRegisters(Config::Register::CONSUMED_ENERGY, 2,
                            energyWords)) {
    data.online = false;
    data.dataReady = false;
    return false;
  }

  const float frequency =
      electrical[blockOffset(Config::Register::FREQUENCY)] * 0.01f;

  for (uint8_t phase = 0; phase < METER_PHASE_COUNT; phase++) {
    uint16_t offset = blockOffset(Config::Register::VOLTAGE[phase]);
    data.phase[phase].voltage =
        combineU32(electrical[offset], electrical[offset + 1]) * 0.0001f;

    offset = blockOffset(Config::Register::CURRENT[phase]);
    data.phase[phase].current =
        combineU32(electrical[offset], electrical[offset + 1]) * 0.0001f;

    data.phase[phase].frequency = frequency;

    offset = blockOffset(Config::Register::ACTIVE_POWER[phase]);
    data.phase[phase].activePower =
        combineS32(electrical[offset], electrical[offset + 1]) * 0.1f;

    offset = blockOffset(Config::Register::REACTIVE_POWER[phase]);
    data.phase[phase].reactivePower =
        combineS32(electrical[offset], electrical[offset + 1]) * 0.1f;

    offset = blockOffset(Config::Register::APPARENT_POWER[phase]);
    data.phase[phase].apparentPower =
        combineS32(electrical[offset], electrical[offset + 1]) * 0.1f;
  }

  uint16_t offset = blockOffset(Config::Register::TOTAL_ACTIVE_POWER);
  data.totalActive =
      combineS32(electrical[offset], electrical[offset + 1]) * 0.1f;

  offset = blockOffset(Config::Register::TOTAL_REACTIVE_POWER);
  data.totalReactive =
      combineS32(electrical[offset], electrical[offset + 1]) * 0.1f;

  offset = blockOffset(Config::Register::TOTAL_APPARENT_POWER);
  data.totalApparent =
      combineS32(electrical[offset], electrical[offset + 1]) * 0.1f;

  data.powerFactor =
      static_cast<int16_t>(electrical[blockOffset(
          Config::Register::TOTAL_POWER_FACTOR)]) * 0.001f;
  const float rawEnergy = Config::Register::SWAP_ENERGY_WORDS
                              ? combineFloat32(energyWords[1], energyWords[0])
                              : combineFloat32(energyWords[0], energyWords[1]);
  data.totalEnergy = rawEnergy / Config::Register::ENERGY_DIVISOR;

  const bool valid = isfinite(data.totalEnergy) && data.totalEnergy >= 0.0f &&
                     data.totalEnergy < 1.0e9f && frequency >= 40.0f &&
                     frequency <= 70.0f;
  data.online = true;
  data.dataReady = valid;
  return valid;
}

void saveLatest(const MeterData &data) {
  if (xSemaphoreTake(meterMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    latestMeter = data;
    latestSequence++;
    xSemaphoreGive(meterMutex);
  }
}

void printMeterSummary(const MeterData &data) {
  if (!data.dataReady) {
    Serial.printf("[Modbus] Pembacaan slave ID %u gagal/tidak valid\n",
                  Config::Modbus::SLAVE_ID);
    return;
  }

  Serial.printf(
      "[Modbus] V=%.2f/%.2f/%.2f V, I=%.3f/%.3f/%.3f A, "
      "F=%.2f/%.2f/%.2f Hz\n",
      data.phase[0].voltage, data.phase[1].voltage, data.phase[2].voltage,
      data.phase[0].current, data.phase[1].current, data.phase[2].current,
      data.phase[0].frequency, data.phase[1].frequency,
      data.phase[2].frequency);
  Serial.printf(
      "[Modbus] P: L1=%.3f L2=%.3f L3=%.3f | Q: L1=%.3f L2=%.3f L3=%.3f | "
      "S: L1=%.3f L2=%.3f L3=%.3f (kW/kVAR/kVA)\n",
      data.phase[0].activePower / Config::Mqtt::POWER_DIVISOR,
      data.phase[1].activePower / Config::Mqtt::POWER_DIVISOR,
      data.phase[2].activePower / Config::Mqtt::POWER_DIVISOR,
      data.phase[0].reactivePower / Config::Mqtt::POWER_DIVISOR,
      data.phase[1].reactivePower / Config::Mqtt::POWER_DIVISOR,
      data.phase[2].reactivePower / Config::Mqtt::POWER_DIVISOR,
      data.phase[0].apparentPower / Config::Mqtt::POWER_DIVISOR,
      data.phase[1].apparentPower / Config::Mqtt::POWER_DIVISOR,
      data.phase[2].apparentPower / Config::Mqtt::POWER_DIVISOR);
  Serial.printf(
      "[Modbus] Total: P=%.3f kW, Q=%.3f kVAR, S=%.3f kVA, "
      "PF=%.2f, energy consumed=%.3f kWh\n",
      data.totalActive / Config::Mqtt::POWER_DIVISOR,
      data.totalReactive / Config::Mqtt::POWER_DIVISOR,
      data.totalApparent / Config::Mqtt::POWER_DIVISOR,
      data.powerFactor, data.totalEnergy);
}

void modbusTask(void *) {
  modbusSerial.begin(Config::Modbus::BAUD, Config::Modbus::SERIAL_FORMAT,
                     Config::Pins::MODBUS_RX, Config::Pins::MODBUS_TX);
  Serial.printf("[Modbus] Slave ID %u, FC03, %lu baud, 8N1, RX=%u, TX=%u\n",
                Config::Modbus::SLAVE_ID, Config::Modbus::BAUD,
                Config::Pins::MODBUS_RX, Config::Pins::MODBUS_TX);

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    MeterData data{};
    readMeter(data);
    ledStatusSetModbusHealthy(data.online && data.dataReady);
    saveLatest(data);
    printMeterSummary(data);
    vTaskDelay(pdMS_TO_TICKS(Config::Modbus::POLLING_INTERVAL_MS));
  }
}
}  // namespace

void modbusRtuBegin() {
  meterMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(modbusTask, "Modbus RTU", 5120, nullptr, 2, nullptr, 1);
}

bool modbusRtuGetLatest(MeterData &data, uint32_t &sequence) {
  if (meterMutex == nullptr ||
      xSemaphoreTake(meterMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }

  data = latestMeter;
  sequence = latestSequence;
  xSemaphoreGive(meterMutex);
  return sequence > 0;
}
