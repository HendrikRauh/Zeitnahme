#ifndef ESPNOW_H
#define ESPNOW_H
#include <data.h>
#include <Arduino.h>
#include <Utility.h>
#include <deviceInfo.h>
#include <esp_now.h>
#include <data.h>
#include <server.h>

struct SaveDeviceMessage
{
    uint8_t targetMac[6];
    Role targetRole;
    uint8_t senderMac[6];
    Role senderRole;
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

#endif