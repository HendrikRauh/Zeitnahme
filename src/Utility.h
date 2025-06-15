#ifndef UTILITY_H
#define UTILITY_H

#include <role.h>
#include <Arduino.h>
#include <Sensor.h>
#include <data.h>

String roleToString(Role role);
Role stringToRole(const String &text);
String statusToString(LichtschrankeStatus status);
String macToString(const uint8_t *mac);
void handleIdentityMessage(const uint8_t *senderMac, Role senderRole);
void printDeviceLists();

String getDiscoveredDevicesJson();
String getSavedDevicesJson();

#endif