#ifndef ESPNOW_H
#define ESPNOW_H
#include <Arduino.h>
#include <Utility.h>
#include <deviceInfo.h>
#include <esp_now.h>
#include <role.h>

// Forward declarations
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

// Message-Typen
#define MSG_TYPE_HEARTBEAT 1
#define MSG_TYPE_TIME_SYNC_REQUEST 2
#define MSG_TYPE_TIME_SYNC_RESPONSE 3
#define MSG_TYPE_RACE_UPDATE 4
#define MSG_TYPE_FULL_SYNC 5

#define ESP_NOW_CHANNEL 8

struct SaveDeviceMessage
{
    uint8_t targetMac[6];
    Role targetRole;
    uint8_t senderMac[6];
    Role senderRole;
};

struct RaceEventMessage
{
    Role senderRole;         // ROLE_START oder ROLE_ZIEL
    unsigned long eventTime; // millis() beim Auslösen
    unsigned long localTime; // Lokale Zeit des sendenden Geräts
    uint8_t senderMac[6];    // MAC des sendenden Geräts
};

struct MasterHeartbeatMessage
{
    uint8_t messageType; // 1 = Heartbeat
    uint8_t masterMac[6];
    unsigned long masterTime;
    unsigned long sequenceNumber;
};

struct TimeSyncRequestMessage
{
    uint8_t messageType; // 2 = TimeSyncRequest
    uint8_t requesterMac[6];
    unsigned long requestTime;
    unsigned long sequenceNumber;
};

struct TimeSyncResponseMessage
{
    uint8_t messageType; // 3 = TimeSyncResponse
    uint8_t masterMac[6];
    unsigned long masterTime;
    unsigned long originalRequestTime;
    unsigned long sequenceNumber;
};

struct RaceUpdateMessage
{
    uint8_t messageType; // 4 = RaceUpdate
    uint8_t masterMac[6];
    int raceCount;
    RaceEntry races[5]; // Maximal 5 aktive Rennen gleichzeitig
    unsigned long timestamp;
};

struct FullSyncMessage
{
    uint8_t messageType; // 5 = FullSync
    uint8_t masterMac[6];
    unsigned long masterTime;
    int raceCount;
    RaceEntry races[5];             // Aktuelle Rennen
    unsigned long lastFinishedTime; // Letzte beendete Zeit
    unsigned long timestamp;
};

void initEspNow();

void sendIdentity(const uint8_t *dest);

void sendDiscoveryMessage();

void tellOthersMyRoleChanged();

void sendGoodBye(const uint8_t *mac);

bool tellOtherDeviceToChangeHisRole(const uint8_t *targetMac, Role newRole);

void searchForDevices();

void addDeviceToPeer(const uint8_t *mac);

void removeDeviceFromPeer(const uint8_t *mac);

// Sende RaceEventMessage an alle bekannten Geräte
void broadcastRaceEvent(Role senderRole, unsigned long eventTime);

// Master-System Funktionen
void sendMasterHeartbeat();
void sendTimeSyncRequest();
void sendTimeSyncResponse(const uint8_t *requesterMac, unsigned long originalRequestTime, unsigned long sequenceNumber);
void sendRaceUpdate();
void sendFullSync();

#endif