#ifndef DEVICEINFO_H
#define DEVICEINFO_H
#include <WiFi.h>
#include <Utility.h>

const uint8_t *getMacAddress();
void initDeviceInfo();
#endif