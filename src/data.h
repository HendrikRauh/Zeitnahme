#ifndef DATA_H
#define DATA_H

#include "Preferences.h"
#include <Utility.h>
#include <data.h>
#include <deviceInfo.h>
#include <ArduinoJson.h>

Role getOwnRole();

void saveOwnRole(Role role);

void loadDeviceListFromPreferences();

void writeDeviceListToPreferences();

void resetAll();

#include <Arduino.h>
#include <vector>
#include <algorithm>
#include <role.h>
#include <Utility.h>
#include <espnow.h>
#include <deque>

struct DeviceInfo
{
    uint8_t mac[6];
    Role role;
};

std::vector<DeviceInfo> getDiscoveredDevices();

std::vector<DeviceInfo> getSavedDevices();

bool checkIfDeviceIsSaved(const uint8_t *mac);

bool checkIfDeviceIsDiscoveredList(const uint8_t *mac);

void addDiscoveredDevice(const uint8_t *mac, Role role);

void clearDiscoveredDevices();

void addSavedDevice(const uint8_t *mac, Role role);

void removeSavedDevice(const uint8_t *mac);

void changeSavedDevice(const uint8_t *mac, Role role);

bool changeOtherDevice(const uint8_t *mac, Role role);

void changeOwnRole(Role newRole);

// Sensor Distance Settings Funktionen
float getMinDistance();
float getMaxDistance();
void setMinDistance(float minDistance);
void setMaxDistance(float maxDistance);

struct RaceEntry
{
    unsigned long startTime;
};

extern std::deque<RaceEntry> raceQueue;

void addRaceStart(unsigned long startTime);
bool finishRace(unsigned long finishTime, unsigned long &startTime, unsigned long &duration);

#endif