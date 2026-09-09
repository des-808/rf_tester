#ifndef ESP32_TO_WIFI_AND_BLUETOOTH_H
#define ESP32_TO_WIFI_AND_BLUETOOTH_H

#include <stdbool.h>

extern bool esp32_wifi_connected;

bool ESP32_WiFi_IsConnected(void);
bool ESP32_WiFi_Connect(void);
bool ESP32_NTP_Sync(void);

#endif /* ESP32_TO_WIFI_AND_BLUETOOTH_H */
