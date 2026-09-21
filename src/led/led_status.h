#pragma once

void ledStatusBegin();
void ledStatusSetWifiConnected(bool connected);
void ledStatusSetModbusHealthy(bool healthy);
void ledStatusSetMqttConnected(bool connected);
void ledStatusSetThingSpeakHealthy(bool healthy);
void ledStatusSetUploading(bool uploading);
void ledStatusNotifyUpload();
void ledStatusNotifyUploadSuccess();
