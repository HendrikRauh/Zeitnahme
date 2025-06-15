#include <deviceInfo.h>

static uint8_t s_macAddress[6];

void initDeviceInfo()
{
    WiFi.macAddress(s_macAddress);
    Serial.printf("Eigene Mac: %s\n", macToString(s_macAddress).c_str());
}

const uint8_t *getMacAddress()
{
    return s_macAddress;
}