#pragma once

#include <Arduino.h>

void hermesAlertBegin();
void hermesSendAlert(const char *alert, const char *detail);
