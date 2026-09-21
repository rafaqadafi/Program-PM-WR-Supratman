#pragma once

#include <Arduino.h>

constexpr uint8_t METER_PHASE_COUNT = 3;

struct PhaseData {
  float voltage;
  float current;
  float frequency;
  float activePower;
  float reactivePower;
  float apparentPower;
};

struct MeterData {
  uint8_t slave;
  bool online;
  bool dataReady;
  PhaseData phase[METER_PHASE_COUNT];
  float totalActive;
  float totalReactive;
  float totalApparent;
  float powerFactor;
  // Total positive active energy sisi primer (energi consumed/import).
  float totalEnergy;
};

void modbusRtuBegin();
bool modbusRtuGetLatest(MeterData &data, uint32_t &sequence);
