#ifndef ESPNOW_H
#define ESPNOW_H
#include <data.h>
#include <Arduino.h>
#include <Utility.h>
#include <deviceInfo.h>
#include <esp_now.h>
#include <data.h>
#include <server.h>

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

#endif