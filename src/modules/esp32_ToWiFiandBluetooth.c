#include "esp32_ToWiFiandBluetooth.h"

/* Debug state until the real ESP32/WiFi bridge is connected. */
bool esp32_wifi_connected = false;

bool ESP32_WiFi_IsConnected(void)
{
    return esp32_wifi_connected;
}

bool ESP32_WiFi_Connect(void)
{
    return esp32_wifi_connected;
}

bool ESP32_NTP_Sync(void)
{
    return esp32_wifi_connected;
}
