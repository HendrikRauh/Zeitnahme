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

// Forward declarations
void broadcastMasterStatus();

struct DeviceInfo
{
    uint8_t mac[6];
    Role role;
    long timeOffset;        // Zeit-Offset relativ zum Master in ms
    bool isOnline;          // Ist das Gerät aktuell erreichbar?
    unsigned long lastSeen; // Letzter Kontakt (millis())
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
    unsigned long startTimeLocal;  // Lokale Zeit des Start-Geräts
    uint8_t startDevice[6];        // MAC des Start-Geräts
    bool isFinished;               // Wurde das Rennen beendet?
    unsigned long finishTime;      // Ziel-Zeit (nur wenn isFinished=true)
    unsigned long finishTimeLocal; // Lokale Zeit des Ziel-Geräts
    uint8_t finishDevice[6];       // MAC des Ziel-Geräts
    unsigned long duration;        // Berechnete Dauer in ms
};

extern std::deque<RaceEntry> raceQueue;
extern std::vector<DeviceInfo> savedDevices;

void addRaceStart(unsigned long startTime);
bool finishRace(unsigned long finishTime, unsigned long &startTime, unsigned long &duration);

// Gibt die aktuelle Anzahl laufender Läufe zurück
int getLaufCount();

// Master-System Funktionen
MasterStatus getMasterStatus();
void setMasterStatus(MasterStatus status);
bool isMaster();
bool isSlave();
uint8_t *getMasterMac();
void determineMaster();
void syncTimeWithMaster();
void handleMasterHeartbeat(const uint8_t *masterMac, unsigned long masterTime);
void sendHeartbeat();
void checkMasterOnline();

// Zeit-Synchronisation
void requestTimeSync();
void handleTimeSyncRequest(const uint8_t *requesterMac, unsigned long requesterTime);
void handleTimeSyncResponse(const uint8_t *masterMac, unsigned long masterTime, unsigned long roundTripTime);
long getTimeOffset(const uint8_t *deviceMac);
void updateTimeOffset(const uint8_t *deviceMac, long offset);

// Race-Management (nur Master)
void masterAddRaceStart(unsigned long startTime, const uint8_t *startDevice, unsigned long localTime);
void masterFinishRace(unsigned long finishTime, const uint8_t *finishDevice, unsigned long localTime);
void broadcastRaceUpdate();
void handleRaceUpdate(const uint8_t *data, int len);
void cleanupFinishedRaces();

// Slave-Funktionen
void slaveHandleRaceStart(unsigned long startTime, const uint8_t *startDevice, unsigned long localTime);
void slaveHandleRaceFinish(unsigned long finishTime, const uint8_t *finishDevice, unsigned long localTime);

#endif