#include <espnow.h>

void handleSaveDeviceMessage(const uint8_t *incomingData)
{
    SaveDeviceMessage msg;

    memcpy(&msg, incomingData, sizeof(msg));

    Serial.printf("[ROLE_DEBUG] SaveDeviceMessage empfangen. Sender: %s (Rolle: %s), Ziel: %s (Rolle: %s)\n",
                  macToString(msg.senderMac).c_str(), roleToString(msg.senderRole),
                  macToString(msg.targetMac).c_str(), roleToString(msg.targetRole));

    // WebSocket-Updates senden
    String json = "{\"mac\":\"" + macToString(msg.targetMac) + "\",\"role\":\"" + roleToString(msg.targetRole) + "\"}";
    wsBrodcastMessage("{\"type\":\"device\",\"data\":" + json + "}");
    // ! If no ROLE_IGNORE: save own role given and save other device
    if (msg.targetRole != ROLE_IGNORE)
    {
        addSavedDevice(msg.senderMac, msg.senderRole);
        changeOwnRole(msg.targetRole);
    }
    else
    {
        removeSavedDevice(msg.senderMac);
    }
}

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    Serial.printf("[ROLE_DEBUG] Daten empfangen von MAC %s, Länge %d.\n", macToString(mac).c_str(), len);
    if (len == 9 && memcmp(incomingData, "WHOAREYOU", 9) == 0)
    {
        Serial.printf("[ROLE_DEBUG] 'WHOAREYOU' Scream empfangen von %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        sendIdentity(mac);
    }
    else if (len == sizeof(DeviceInfo))
    {
        DeviceInfo msg;
        memcpy(&msg, incomingData, sizeof(msg));
        Serial.printf("[ROLE_DEBUG] Identity empfangen von MAC %s, Rolle %s\n", macToString(msg.mac).c_str(), roleToString(msg.role).c_str());
        handleIdentityMessage(msg.mac, msg.role);
    }
    else if (len == sizeof(SaveDeviceMessage))
    {
        handleSaveDeviceMessage(incomingData);
    }
}

void onDataSend(const uint8_t *mac, esp_now_send_status_t status)
{
    Serial.printf("[ROLE_DEBUG] ESP-NOW Sendestatus für MAC %s: %s\n", macToString(mac).c_str(), status == ESP_NOW_SEND_SUCCESS ? "success" : "fail");
}

void sendIdentity(const uint8_t *dest)
{
    Serial.printf("[ROLE_DEBUG] Sende eigene Identität an MAC %s.\n", macToString(dest).c_str());
    DeviceInfo msg;
    memcpy(msg.mac, getMacAddress(), 6);
    msg.role = getOwnRole();

    esp_now_peer_info_t peerInfo = {}; // Ziel ist meine eigene MAC (da ich meine Identität sende)
    memcpy(peerInfo.peer_addr, dest, 6);
    peerInfo.channel = ESP_NOW_CHANNEL;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(dest))
    {
        esp_now_add_peer(&peerInfo);
    }
    esp_err_t result = esp_now_send(dest, (uint8_t *)&msg, sizeof(msg));
    if (result == ESP_OK)
    {
        Serial.println("[ROLE_DEBUG] Identität erfolgreich gesendet.");
    }
    else
    {
        Serial.printf("[ROLE_DEBUG] Fehler beim Senden der Identität, Fehlercode: %d\n", result);
    }
    printDeviceLists();
}

void sendDiscoveryMessage()
{
    Serial.println("[ESP_NOW_DEBUG] Sende 'WHOAREYOU' Broadcast.");
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    addDeviceToPeer(broadcastMac);
    esp_now_send(broadcastMac, (uint8_t *)"WHOAREYOU", 9);
    removeDeviceFromPeer(broadcastMac);
    Serial.println("[ESP_NOW_DEBUG] 'WHOAREYOU' Broadcast gesendet und Peer entfernt.\n");
}

bool tellOtherDeviceToChangeHisRole(const uint8_t *targetMac, Role newRole)
{
    Serial.printf("[ROLE_DEBUG] Sende Anforderung zur Rollenänderung an MAC %s, neue Rolle: %s\n", macToString(targetMac).c_str(), roleToString(newRole).c_str());
    SaveDeviceMessage msg;
    memcpy(msg.targetMac, targetMac, 6);
    msg.targetRole = newRole;
    memcpy(msg.senderMac, getMacAddress(), 6);
    msg.senderRole = getOwnRole();

    esp_now_peer_info_t peerInfo = {};

    memcpy(peerInfo.peer_addr, targetMac, 6);
    peerInfo.channel = ESP_NOW_CHANNEL;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(targetMac))
    {
        esp_now_add_peer(&peerInfo);
    }

    esp_err_t result = esp_now_send(targetMac, (uint8_t *)&msg, sizeof(msg));
    if (result == ESP_OK)
    {
        Serial.println("[ROLE_DEBUG] ESP-NOW Nachricht erfolgreich gesendet.");
        return true;
    }
    else
    {
        Serial.printf("[ROLE_DEBUG] ESP-NOW Nachricht fehlgeschlagen, Fehlercode: %d\n", result);
        return false;
    }
}

void tellOthersMyRoleChanged()
{
    loadDeviceListFromPreferences();

    for (const auto &dev : getSavedDevices())
    {
        sendIdentity(dev.mac);
    }
}

void sendGoodBye(const uint8_t *mac)
{
    // send deviceinfo with role_ignore

    Serial.printf("[ROLE_DEBUG] Sende Goodbye an MAC %s.\n", macToString(mac));
    DeviceInfo msg;
    WiFi.macAddress(msg.mac);
    msg.role = ROLE_IGNORE;

    esp_now_peer_info_t peerInfo = {}; // Ziel ist meine eigene MAC (da ich meine Identität sende)
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = ESP_NOW_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    esp_err_t result = esp_now_send(mac, (uint8_t *)&msg, sizeof(msg));
    if (result == ESP_OK)
    {
        Serial.println("[ROLE_DEBUG] Goodbye erfolgreich gesendet.");
    }
    else
    {
        Serial.printf("[ROLE_DEBUG] Fehler beim Senden von Goodbye, Fehlercode: %d\n", result);
    }
}

void initEspNow()
{
    esp_now_init();
    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSend);
}

void addDeviceToPeer(const uint8_t *mac)
{
    esp_now_peer_info_t peerInfo = {};

    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = ESP_NOW_CHANNEL;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(mac))
    {
        esp_now_add_peer(&peerInfo);
    }
}

void removeDeviceFromPeer(const uint8_t *mac)
{
    esp_now_del_peer(mac);
}

void searchForDevices()
{
    clearDiscoveredDevices();
    sendDiscoveryMessage();
}