#include <espnow.h>
#include <data.h>
#include <server.h>
#include <algorithm>

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
    else if (len == sizeof(RaceEventMessage))
    {
        RaceEventMessage msg;
        memcpy(&msg, incomingData, sizeof(msg));

        if (isMaster())
        {
            // Master verarbeitet Race-Events
            if (msg.senderRole == ROLE_START)
            {
                masterAddRaceStart(msg.eventTime, msg.senderMac, msg.localTime);
            }
            else if (msg.senderRole == ROLE_ZIEL)
            {
                masterFinishRace(msg.eventTime, msg.senderMac, msg.localTime);
            }
        }
        else
        {
            // Slaves leiten Race-Events an Master weiter (falls nötig)
            Serial.printf("[SLAVE_DEBUG] Race-Event von %s empfangen, Rolle: %s\n",
                          macToString(msg.senderMac).c_str(), roleToString(msg.senderRole).c_str());
        }
    }
    else if (len == sizeof(MasterHeartbeatMessage))
    {
        // Prüfe Message-Typ
        uint8_t messageType = incomingData[0];
        if (messageType == MSG_TYPE_HEARTBEAT)
        {
            MasterHeartbeatMessage msg;
            memcpy(&msg, incomingData, sizeof(msg));
            handleMasterHeartbeat(msg.masterMac, msg.masterTime);
        }
        else
        {
            Serial.printf("[ESP_NOW_DEBUG] Unbekannter Message-Typ %d für Heartbeat-Größe\n", messageType);
        }
    }
    else if (len == sizeof(TimeSyncRequestMessage))
    {
        // Prüfe Message-Typ
        uint8_t messageType = incomingData[0];
        if (messageType == MSG_TYPE_TIME_SYNC_REQUEST)
        {
            TimeSyncRequestMessage msg;
            memcpy(&msg, incomingData, sizeof(msg));
            handleTimeSyncRequest(msg.requesterMac, msg.requestTime);
        }
        else
        {
            Serial.printf("[ESP_NOW_DEBUG] Unbekannter Message-Typ %d für TimeSyncRequest-Größe\n", messageType);
        }
    }
    else if (len == sizeof(TimeSyncResponseMessage))
    {
        // Prüfe Message-Typ
        uint8_t messageType = incomingData[0];
        if (messageType == MSG_TYPE_TIME_SYNC_RESPONSE)
        {
            TimeSyncResponseMessage msg;
            memcpy(&msg, incomingData, sizeof(msg));
            unsigned long roundTripTime = millis() - msg.originalRequestTime;
            handleTimeSyncResponse(msg.masterMac, msg.masterTime, roundTripTime);
        }
        else
        {
            Serial.printf("[ESP_NOW_DEBUG] Unbekannter Message-Typ %d für TimeSyncResponse-Größe\n", messageType);
        }
    }
    else if (len == sizeof(RaceUpdateMessage))
    {
        // Prüfe Message-Typ
        uint8_t messageType = incomingData[0];
        if (messageType == MSG_TYPE_RACE_UPDATE)
        {
            RaceUpdateMessage msg;
            memcpy(&msg, incomingData, sizeof(msg));
            handleRaceUpdate((uint8_t *)&msg, len);
        }
        else
        {
            Serial.printf("[ESP_NOW_DEBUG] Unbekannter Message-Typ %d für RaceUpdate-Größe\n", messageType);
        }
    }
    else if (len == sizeof(FullSyncMessage))
    {
        // Prüfe Message-Typ
        uint8_t messageType = incomingData[0];
        if (messageType == MSG_TYPE_FULL_SYNC)
        {
            FullSyncMessage msg;
            memcpy(&msg, incomingData, sizeof(msg));
            handleFullSync((uint8_t *)&msg, len);
        }
        else
        {
            Serial.printf("[ESP_NOW_DEBUG] Unbekannter Message-Typ %d für FullSync-Größe\n", messageType);
        }
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

void broadcastRaceEvent(Role senderRole, unsigned long eventTime)
{
    RaceEventMessage msg;
    msg.senderRole = senderRole;
    msg.eventTime = eventTime;
    msg.localTime = millis();
    memcpy(msg.senderMac, getMacAddress(), 6);

    if (isSlave())
    {
        // Slaves senden nur an Master
        esp_now_send(getMasterMac(), (uint8_t *)&msg, sizeof(msg));
        Serial.printf("[SLAVE_DEBUG] Race-Event an Master gesendet: %s\n", macToString(getMasterMac()).c_str());
    }
    else if (isMaster())
    {
        // Master sendet an alle Slaves
        for (const auto &dev : getSavedDevices())
        {
            if (memcmp(dev.mac, getMacAddress(), 6) != 0) // Nicht an sich selbst senden
            {
                esp_now_send(dev.mac, (uint8_t *)&msg, sizeof(msg));
            }
        }
        Serial.println("[MASTER_DEBUG] Race-Event an alle Slaves gesendet");
    }
}

// Master-System Funktionen
void sendMasterHeartbeat()
{
    if (!isMaster())
        return;

    MasterHeartbeatMessage msg;
    msg.messageType = MSG_TYPE_HEARTBEAT;
    memcpy(msg.masterMac, getMacAddress(), 6);
    msg.masterTime = millis();
    msg.sequenceNumber = millis() / 1000; // Einfache Sequenznummer

    for (const auto &dev : getSavedDevices())
    {
        if (memcmp(dev.mac, getMacAddress(), 6) != 0) // Nicht an sich selbst senden
        {
            esp_now_send(dev.mac, (uint8_t *)&msg, sizeof(msg));
        }
    }
    Serial.println("[MASTER_DEBUG] Heartbeat an alle Slaves gesendet");
}

void sendTimeSyncRequest()
{
    if (!isSlave())
        return;

    TimeSyncRequestMessage msg;
    msg.messageType = MSG_TYPE_TIME_SYNC_REQUEST;
    memcpy(msg.requesterMac, getMacAddress(), 6);
    msg.requestTime = millis();
    msg.sequenceNumber = millis(); // Einfache Sequenznummer

    esp_now_send(getMasterMac(), (uint8_t *)&msg, sizeof(msg));
    Serial.printf("[SYNC_DEBUG] Zeit-Sync-Anfrage an Master gesendet: %s\n", macToString(getMasterMac()).c_str());
}

void sendTimeSyncResponse(const uint8_t *requesterMac, unsigned long originalRequestTime, unsigned long sequenceNumber)
{
    if (!isMaster())
        return;

    TimeSyncResponseMessage msg;
    msg.messageType = MSG_TYPE_TIME_SYNC_RESPONSE;
    memcpy(msg.masterMac, getMacAddress(), 6);
    msg.masterTime = millis();
    msg.originalRequestTime = originalRequestTime;
    msg.sequenceNumber = sequenceNumber;

    esp_now_send(requesterMac, (uint8_t *)&msg, sizeof(msg));
    Serial.printf("[SYNC_DEBUG] Zeit-Sync-Antwort an %s gesendet\n", macToString(requesterMac).c_str());
}

void sendRaceUpdate()
{
    if (!isMaster())
        return;

    RaceUpdateMessage msg;
    msg.messageType = MSG_TYPE_RACE_UPDATE;
    memcpy(msg.masterMac, getMacAddress(), 6);
    msg.raceCount = raceQueue.size() > 5 ? 5 : raceQueue.size(); // Maximal 5 Rennen
    msg.timestamp = millis();

    // Kopiere die aktuellen Race-Daten
    int i = 0;
    for (const auto &race : raceQueue)
    {
        if (i >= 5)
            break; // Maximal 5 Rennen
        msg.races[i] = race;
        i++;
    }

    // Sende die vollständigen Race-Daten an alle Slaves
    for (const auto &dev : getSavedDevices())
    {
        if (memcmp(dev.mac, getMacAddress(), 6) != 0) // Nicht an sich selbst senden
        {
            esp_now_send(dev.mac, (uint8_t *)&msg, sizeof(msg));
        }
    }
    Serial.printf("[MASTER_DEBUG] Vollständige Race-Daten an alle Slaves gesendet: %d Rennen\n", msg.raceCount);
}

void sendFullSync()
{
    if (!isMaster())
        return;

    FullSyncMessage msg;
    msg.messageType = MSG_TYPE_FULL_SYNC;
    memcpy(msg.masterMac, getMacAddress(), 6);
    msg.masterTime = millis();
    msg.timestamp = millis();

    // Kopiere die aktuellen Race-Daten
    int i = 0;
    for (const auto &race : raceQueue)
    {
        if (i >= 5)
            break; // Maximal 5 Rennen
        msg.races[i] = race;
        i++;
    }
    msg.raceCount = i;

    // Finde die letzte beendete Zeit
    msg.lastFinishedTime = 0;
    for (auto it = raceQueue.rbegin(); it != raceQueue.rend(); ++it)
    {
        if (it->isFinished)
        {
            msg.lastFinishedTime = it->duration;
            break;
        }
    }

    // Sende vollständige Sync-Daten an alle Slaves
    for (const auto &dev : getSavedDevices())
    {
        if (memcmp(dev.mac, getMacAddress(), 6) != 0) // Nicht an sich selbst senden
        {
            esp_now_send(dev.mac, (uint8_t *)&msg, sizeof(msg));
        }
    }
    Serial.printf("[MASTER_DEBUG] Full-Sync an alle Slaves gesendet: %d Rennen, letzte Zeit: %lu ms\n",
                  msg.raceCount, msg.lastFinishedTime);
}