#include <espnow.h>
#include <data.h>
#include <server.h>
#include <algorithm>
#include <set>

void handleSaveDeviceMessage(const uint8_t *incomingData)
{
    SaveDeviceMessage msg;

    memcpy(&msg, incomingData, sizeof(msg));

    // WebSocket-Updates senden
    String json = "{\"mac\":\"" + macToString(msg.targetMac) + "\",\"role\":\"" + roleToString(msg.targetRole) + "\"}";
    wsBrodcastMessage("{\"type\":\"device\",\"data\":" + json + "}");

    // Prüfe, ob die Nachricht für dieses Gerät bestimmt ist
    if (memcmp(msg.targetMac, getMacAddress(), 6) == 0)
    {
        // Die Nachricht ist für uns bestimmt
        if (msg.targetRole != ROLE_IGNORE)
        {
            Serial.printf("[ROLE_DEBUG] Rollenänderungsanfrage empfangen: neue Rolle %s\n", roleToString(msg.targetRole).c_str());
            addSavedDevice(msg.senderMac, msg.senderRole);
            changeOwnRole(msg.targetRole);
        }
        else
        {
            Serial.println("[ROLE_DEBUG] Entfernungsanfrage empfangen - ignoriere Sender");
            removeSavedDevice(msg.senderMac);
        }
    }
    else
    {
        Serial.printf("[ROLE_DEBUG] SaveDeviceMessage für anderes Gerät empfangen: %s (ignoriert)\n", macToString(msg.targetMac).c_str());
    }
}

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    if (len == 9 && memcmp(incomingData, "WHOAREYOU", 9) == 0)
    {
        sendIdentity(mac);
    }
    else if (len == sizeof(DeviceInfo) || len == sizeof(SaveDeviceMessage))
    {
        // Da beide Messages die gleiche Größe haben, prüfen wir das messageType Feld
        SaveDeviceMessage *saveMsg = (SaveDeviceMessage *)incomingData;
        if (saveMsg->messageType == MSG_TYPE_SAVE_DEVICE)
        {
            handleSaveDeviceMessage(incomingData);
        }
        else
        {
            // Es ist eine DeviceInfo-Nachricht
            DeviceInfo msg;
            memcpy(&msg, incomingData, sizeof(msg));
            handleIdentityMessage(msg.mac, msg.role);
        }
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
        }
    }
    else if (len == sizeof(MasterHeartbeatMessage) || len == sizeof(TimeSyncRequestMessage))
    {
        // Prüfe Message-Typ erst, dann handle entsprechend
        uint8_t messageType = incomingData[0];

        if (messageType == MSG_TYPE_HEARTBEAT && len == sizeof(MasterHeartbeatMessage))
        {
            // Serial.printf("[ESP_NOW_DEBUG] MasterHeartbeatMessage empfangen\n");
            MasterHeartbeatMessage msg;
            memcpy(&msg, incomingData, sizeof(msg));
            handleMasterHeartbeat(msg.masterMac, msg.masterTime);
        }
        else if (messageType == MSG_TYPE_TIME_SYNC_REQUEST && len == sizeof(TimeSyncRequestMessage))
        {
            TimeSyncRequestMessage msg;
            memcpy(&msg, incomingData, sizeof(msg));
            handleTimeSyncRequest(msg.requesterMac, msg.requestTime);
        }
        else
        {
            Serial.printf("[ESP_NOW_DEBUG] Unbekannter Message-Typ: %d, Länge: %d\n", messageType, len);
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
            // Unbekannter Message-Typ
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
            Serial.printf("[ESP_NOW_DEBUG] Unbekannter TimeSyncResponse Message-Typ: %d\n", messageType);
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
            Serial.printf("[ESP_NOW_DEBUG] Unbekannter FullSync Message-Typ: %d\n", messageType);
        }
    }
    else
    {
        Serial.printf("[ESP_NOW_DEBUG] Unbekannte Nachrichtenlänge: %d bytes\n", len);
    }
}

void onDataSend(const uint8_t *mac, esp_now_send_status_t status)
{
    // Nur Fehler loggen, Erfolg stumm
    if (status != ESP_NOW_SEND_SUCCESS)
    {
        Serial.printf("[ESP_NOW_ERROR] Senden fehlgeschlagen an %s\n", macToString(mac).c_str());
    }
}

void sendIdentity(const uint8_t *dest)
{
    DeviceInfo msg;
    memcpy(msg.mac, getMacAddress(), 6);
    msg.role = getOwnRole();

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, dest, 6);
    peerInfo.channel = ESP_NOW_CHANNEL;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(dest))
    {
        esp_now_add_peer(&peerInfo);
    }
    esp_err_t result = esp_now_send(dest, (uint8_t *)&msg, sizeof(msg));
    if (result != ESP_OK)
    {
        Serial.printf("[ESP_NOW_ERROR] Identität senden fehlgeschlagen: %d\n", result);
    }
    printDeviceLists();
}

void sendDiscoveryMessage()
{
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    addDeviceToPeer(broadcastMac);
    esp_now_send(broadcastMac, (uint8_t *)"WHOAREYOU", 9);
    removeDeviceFromPeer(broadcastMac);
}

bool tellOtherDeviceToChangeHisRole(const uint8_t *targetMac, Role newRole)
{
    SaveDeviceMessage msg;
    msg.messageType = MSG_TYPE_SAVE_DEVICE;
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
        return true;
    }
    else
    {
        Serial.printf("[ESP_NOW_ERROR] Rollenänderung fehlgeschlagen: %d\n", result);
        return false;
    }
}

void tellOthersMyRoleChanged()
{
    loadDeviceListFromPreferences();

    // Sammle alle bekannten MAC-Adressen (sowohl gespeicherte als auch entdeckte)
    std::set<String> sentToMacs;

    // Sende an alle gespeicherten Geräte
    for (const auto &dev : getSavedDevices())
    {
        sendIdentity(dev.mac);
        sentToMacs.insert(macToString(dev.mac));
        Serial.printf("[ROLE_DEBUG] Identity an gespeichertes Gerät gesendet: %s\n", macToString(dev.mac).c_str());
    }

    // Sende auch an alle entdeckten Geräte (die noch nicht erreicht wurden)
    for (const auto &dev : getDiscoveredDevices())
    {
        String macStr = macToString(dev.mac);
        if (sentToMacs.find(macStr) == sentToMacs.end())
        {
            sendIdentity(dev.mac);
            Serial.printf("[ROLE_DEBUG] Identity an entdecktes Gerät gesendet: %s\n", macStr.c_str());
        }
    }
}

void sendGoodBye(const uint8_t *mac)
{
    DeviceInfo msg;
    WiFi.macAddress(msg.mac);
    msg.role = ROLE_IGNORE;

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = ESP_NOW_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    esp_err_t result = esp_now_send(mac, (uint8_t *)&msg, sizeof(msg));
    if (result != ESP_OK)
    {
        Serial.printf("[ESP_NOW_ERROR] Goodbye fehlgeschlagen: %d\n", result);
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
    if (!esp_now_is_peer_exist(mac))
    {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, mac, 6);
        peerInfo.channel = ESP_NOW_CHANNEL;
        peerInfo.encrypt = false;
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

    // Sofortiges Senden ohne Verzögerung für kritische Zeiterfassung
    if (isSlave())
    {
        // Slaves senden nur an Master - sofort
        esp_now_send(getMasterMac(), (uint8_t *)&msg, sizeof(msg));
    }
    else if (isMaster())
    {
        // Master sendet an alle Slaves - sofort
        for (const auto &dev : getSavedDevices())
        {
            if (memcmp(dev.mac, getMacAddress(), 6) != 0)
            {
                esp_now_send(dev.mac, (uint8_t *)&msg, sizeof(msg));
            }
        }
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