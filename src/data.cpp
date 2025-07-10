#include <data.h>
#include <Sensor.h>
#include <server.h>
#include <deque>

Role currentRole;
MasterStatus masterStatus = MASTER_UNKNOWN;
uint8_t masterMac[6] = {0};
unsigned long lastMasterSeen = 0;
unsigned long heartbeatInterval = 5000; // 5 Sekunden
unsigned long lastHeartbeat = 0;
unsigned long syncSequenceNumber = 0;

std::vector<DeviceInfo> discoveredDevices;
std::vector<DeviceInfo> savedDevices;
std::deque<RaceEntry> raceQueue;

Preferences preferences;

Role getOwnRole()
{
    preferences.begin("lichtschranke", true);
    int role = preferences.getUInt("role", 1);
    Serial.printf("[ROLE_DEBUG] Lade eigene Rolle aus Preferences (%d)\n", role);
    preferences.end();
    Serial.printf("[ROLE_DEBUG] Geladene eigene Rolle: %s (int: %d)\n", roleToString(static_cast<Role>(role)).c_str(), role);
    return static_cast<Role>(role);
}

void saveOwnRole(Role role)
{
    Serial.printf("[ROLE_DEBUG] Speichere eigene Rolle: %s (int: %d)\n", roleToString(role).c_str(), static_cast<int>(role));
    preferences.begin("lichtschranke", false);
    preferences.putUInt("role", static_cast<int>(role));
    preferences.end();
    Serial.printf("[ROLE_DEBUG] Eigene Rolle geändert zu %s\n", roleToString(role).c_str());
}

void loadDeviceListFromPreferences()
{
    Serial.println("[ROLE_DEBUG] Lade gespeicherte Geräte aus Preferences.");
    preferences.begin("lichtschranke", true);
    String jsonStr = preferences.getString("devices", "[]");
    preferences.end();
    savedDevices.clear();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (!err)
    {
        JsonArray arr = doc.as<JsonArray>();
        for (JsonObject obj : arr)
        {
            DeviceInfo info;
            sscanf(obj["mac"], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &info.mac[0], &info.mac[1], &info.mac[2], &info.mac[3], &info.mac[4], &info.mac[5]);
            info.role = stringToRole(obj["role"].as<String>());
            info.timeOffset = 0;
            info.isOnline = false;
            info.lastSeen = 0;
            savedDevices.push_back(info);
            Serial.printf("[ROLE_DEBUG] Gerät aus Preferences geladen: MAC %s, Rolle %s\n", macToString(info.mac).c_str(), roleToString(info.role).c_str());

            addDeviceToPeer(info.mac);
        }
    }
    else
    {
        Serial.printf("[ROLE_DEBUG] Fehler beim Deserialisieren der Geräteliste: %s\n", err.c_str());
    }
}

void writeDeviceListToPreferences()
{
    Serial.println("[ROLE_DEBUG] Schreibe gespeicherte Geräteliste in Preferences.");
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto &dev : savedDevices)
    {
        if (memcmp(dev.mac, getMacAddress(), 6) == 0)
            continue;
        JsonObject obj = arr.add<JsonObject>();
        obj["mac"] = macToString(dev.mac);
        obj["role"] = roleToString(dev.role);
        Serial.printf("[ROLE_DEBUG] Gerät für Speicherung vorbereitet: MAC %s, Rolle %s\n", macToString(dev.mac).c_str(), roleToString(dev.role).c_str());
    }
    String jsonStr;
    serializeJson(arr, jsonStr);
    preferences.begin("lichtschranke", false);
    preferences.putString("devices", jsonStr);
    preferences.end();
    Serial.printf("[ROLE_DEBUG] Geräteliste erfolgreich gespeichert: %s\n", jsonStr.c_str());
}

void resetAll()
{
    Serial.println("[CRITICAL] Lösche alle Einstellungen.");
    preferences.begin("lichtschranke", false);
    preferences.clear();
    preferences.end();
}

std::vector<DeviceInfo> getDiscoveredDevices()
{
    return discoveredDevices;
}

std::vector<DeviceInfo> getSavedDevices()
{
    return savedDevices;
}

bool checkIfDeviceIsSaved(const uint8_t *mac)
{
    for (const auto &dev : getSavedDevices())
    {
        if (memcmp(dev.mac, mac, 6) == 0)
        {
            return true;
        }
    }
    return false;
}

bool checkIfDeviceIsDiscoveredList(const uint8_t *mac)
{
    for (const auto &dev : discoveredDevices)
    {
        if (memcmp(dev.mac, mac, 6) == 0)
        {
            return true;
        }
    }
    return false;
}

void addDiscoveredDevice(const uint8_t *mac, Role role)
{
    Serial.printf("[ROLE_DEBUG] Füge entdecktes Gerät hinzu: MAC %s, Rolle %s\n", macToString(mac).c_str(), roleToString(role).c_str());
    auto it = std::find_if(discoveredDevices.begin(), discoveredDevices.end(),
                           [&](const DeviceInfo &d)
                           { return memcmp(d.mac, mac, 6) == 0; });
    if (it != discoveredDevices.end())
    {
        it->role = role;
        it->isOnline = true;
        it->lastSeen = millis();
    }
    else
    {
        DeviceInfo info;
        memcpy(info.mac, mac, 6);
        info.role = role;
        info.timeOffset = 0;
        info.isOnline = true;
        info.lastSeen = millis();
        discoveredDevices.push_back(info);
    }
    printDeviceLists();
}

void addSavedDevice(const uint8_t *mac, Role role)
{
    Serial.printf("[ROLE_DEBUG] Füge gespeichertes Gerät hinzu: MAC %s, Rolle %s\n", macToString(mac).c_str(), roleToString(role).c_str());
    auto it = std::find_if(savedDevices.begin(), savedDevices.end(),
                           [&](const DeviceInfo &d)
                           { return memcmp(d.mac, mac, 6) == 0; });
    if (it != savedDevices.end())
    {
        it->role = role;
        it->isOnline = true;
        it->lastSeen = millis();
    }
    else
    {
        DeviceInfo info;
        memcpy(info.mac, mac, 6);
        info.role = role;
        info.timeOffset = 0;
        info.isOnline = true;
        info.lastSeen = millis();
        savedDevices.push_back(info);
    }
    addDeviceToPeer(mac);
    writeDeviceListToPreferences();
    printDeviceLists();

    // Master-Status neu bestimmen
    determineMaster();
}

void removeSavedDevice(const uint8_t *mac)
{
    Serial.printf("[ROLE_DEBUG] Entferne Gerät %s aus der List\n", macToString(mac).c_str());
    savedDevices.erase(
        std::remove_if(savedDevices.begin(), savedDevices.end(),
                       [&](const DeviceInfo &d)
                       { return memcmp(d.mac, mac, 6) == 0; }),
        savedDevices.end());
    removeDeviceFromPeer(mac);
    writeDeviceListToPreferences();
    printDeviceLists();

    // Master-Status neu bestimmen
    determineMaster();
}

void changeSavedDevice(const uint8_t *mac, Role role)
{
    Serial.printf("[ROLE_DEBUG] Versuche Gerät in Liste zu ändern/hinzuzufügen: MAC %s, Rolle %s\n", macToString(mac).c_str(), roleToString(role).c_str());
    if (memcmp(mac, getMacAddress(), 6) == 0)
    {
        Serial.println("[ROLE_DEBUG] Versucht, eigenes Gerät über changeKnownDevice zu ändern. Ignoriert.");
        return; // Eigenes Gerät nicht hinzufügen!
    }

    auto it = std::find_if(savedDevices.begin(), savedDevices.end(),
                           [&](const DeviceInfo &d)
                           { return memcmp(d.mac, mac, 6) == 0; });
    if (it != savedDevices.end())
    {
        Serial.printf("[ROLE_DEBUG] Rolle für %s wird aktualisiert zu %s\n", macToString(mac).c_str(), roleToString(role));
        it->role = role;
        it->isOnline = true;
        it->lastSeen = millis();
    }
    else
    {
        Serial.printf("[ROLE_DEBUG] Gerät %s nicht gefunden, wird hinzugefügt mit Rolle %s\n", macToString(mac).c_str(), roleToString(role).c_str());
        DeviceInfo info;
        memcpy(info.mac, mac, 6);
        info.role = role;
        info.timeOffset = 0;
        info.isOnline = true;
        info.lastSeen = millis();
        savedDevices.push_back(info);
    }
    writeDeviceListToPreferences();
    printDeviceLists();

    // Master-Status neu bestimmen
    determineMaster();
}

bool changeOtherDevice(const uint8_t *mac, Role role)
{
    Serial.printf("[ROLE_DEBUG] changeOtherDevice aufgerufen für MAC %s, Rolle %s\n", macToString(mac).c_str(), roleToString(role).c_str());
    return tellOtherDeviceToChangeHisRole(mac, role);
}

void changeOwnRole(Role newRole)
{
    saveOwnRole(newRole);
    tellOthersMyRoleChanged();
}

void clearDiscoveredDevices()
{
    discoveredDevices.clear();
}

// Sensor Distance Settings Funktionen
constexpr float DEFAULT_MIN_DISTANCE_CM = 2.0f;
constexpr float DEFAULT_MAX_DISTANCE_CM = 100.0f;

float getMinDistance()
{
    preferences.begin("lichtschranke", true);
    float minDistance = preferences.getFloat("minDistance", DEFAULT_MIN_DISTANCE_CM);
    preferences.end();
    Serial.printf("[DISTANCE_DEBUG] Min-Distanz geladen: %.2f cm\n", minDistance);
    return minDistance;
}

float getMaxDistance()
{
    preferences.begin("lichtschranke", true);
    float maxDistance = preferences.getFloat("maxDistance", DEFAULT_MAX_DISTANCE_CM);
    preferences.end();
    Serial.printf("[DISTANCE_DEBUG] Max-Distanz geladen: %.2f cm\n", maxDistance);
    return maxDistance;
}

void setMinDistance(float minDistance)
{
    if (minDistance < 2 || minDistance > 200)
    {
        Serial.printf("[DISTANCE_DEBUG] Ungültiger Min-Distanz-Wert: %.2f cm. Verwende Default (%.2f cm).\n", minDistance, DEFAULT_MIN_DISTANCE_CM);
        minDistance = DEFAULT_MIN_DISTANCE_CM;
    }

    preferences.begin("lichtschranke", false);
    preferences.putFloat("minDistance", minDistance);
    preferences.end();

    Serial.printf("[DISTANCE_DEBUG] Min-Distanz gesetzt auf: %.2f cm\n", minDistance);

    // Aktualisiere den Cache im Sensor-Modul für bessere Performance
    updateDistanceCache();
}

void setMaxDistance(float maxDistance)
{
    if (maxDistance < 2 || maxDistance > 200)
    {
        Serial.printf("[DISTANCE_DEBUG] Ungültiger Max-Distanz-Wert: %.2f cm. Verwende Default (%.2f cm).\n", maxDistance, DEFAULT_MAX_DISTANCE_CM);
        maxDistance = DEFAULT_MAX_DISTANCE_CM;
    }

    preferences.begin("lichtschranke", false);
    preferences.putFloat("maxDistance", maxDistance);
    preferences.end();

    Serial.printf("[DISTANCE_DEBUG] Max-Distanz gesetzt auf: %.2f cm\n", maxDistance);

    // Aktualisiere den Cache im Sensor-Modul für bessere Performance
    updateDistanceCache();
}

void addRaceStart(unsigned long startTime)
{
    if (isMaster())
    {
        masterAddRaceStart(startTime, getMacAddress(), millis());
    }
    else
    {
        slaveHandleRaceStart(startTime, getMacAddress(), millis());
    }
}

bool finishRace(unsigned long finishTime, unsigned long &startTime, unsigned long &duration)
{
    if (isMaster())
    {
        masterFinishRace(finishTime, getMacAddress(), millis());

        // Gebe die Daten des letzten beendeten Rennens zurück
        for (auto it = raceQueue.rbegin(); it != raceQueue.rend(); ++it)
        {
            if (it->isFinished)
            {
                startTime = it->startTime;
                duration = it->duration;
                return true;
            }
        }
        return false;
    }
    else
    {
        slaveHandleRaceFinish(finishTime, getMacAddress(), millis());

        // Bei Slaves: Warte auf Update vom Master
        // Diese Funktion wird hauptsächlich für Kompatibilität beibehalten
        if (raceQueue.empty())
            return false;

        // Verwende die Daten vom Master
        for (auto it = raceQueue.rbegin(); it != raceQueue.rend(); ++it)
        {
            if (it->isFinished)
            {
                startTime = it->startTime;
                duration = it->duration;
                return true;
            }
        }
        return false;
    }
}

int getLaufCount()
{
    return raceQueue.size();
}

// Master-System Funktionen
MasterStatus getMasterStatus()
{
    return masterStatus;
}

void setMasterStatus(MasterStatus status)
{
    if (masterStatus != status)
    {
        Serial.printf("[MASTER_DEBUG] Master-Status ändert sich von %d zu %d\n", masterStatus, status);
        masterStatus = status;
        if (status == MASTER_MASTER)
        {
            memcpy(masterMac, getMacAddress(), 6);
            Serial.printf("[MASTER_DEBUG] Dieses Gerät ist jetzt Master: %s\n", macToString(masterMac).c_str());
        }
        // Broadcast Master-Status-Änderung
        broadcastMasterStatus();
    }
}

bool isMaster()
{
    return masterStatus == MASTER_MASTER;
}

bool isSlave()
{
    return masterStatus == MASTER_SLAVE;
}

uint8_t *getMasterMac()
{
    return masterMac;
}

void determineMaster()
{
    Serial.println("[MASTER_DEBUG] Bestimme Master-Gerät...");

    uint8_t lowestMac[6];
    memcpy(lowestMac, getMacAddress(), 6);
    bool foundLower = false;

    // Prüfe alle gespeicherten Geräte die online sind
    for (const auto &dev : savedDevices)
    {
        if (dev.isOnline && memcmp(dev.mac, lowestMac, 6) < 0)
        {
            memcpy(lowestMac, dev.mac, 6);
            foundLower = true;
        }
    }

    // Prüfe auch alle entdeckten Geräte
    for (const auto &dev : discoveredDevices)
    {
        if (dev.isOnline && memcmp(dev.mac, lowestMac, 6) < 0)
        {
            memcpy(lowestMac, dev.mac, 6);
            foundLower = true;
        }
    }

    if (!foundLower)
    {
        // Dieses Gerät hat die niedrigste MAC
        if (!isMaster())
        {
            setMasterStatus(MASTER_MASTER);
            memcpy(masterMac, getMacAddress(), 6);
            Serial.printf("[MASTER_DEBUG] Dieses Gerät wird Master: %s\n", macToString(masterMac).c_str());
        }
    }
    else
    {
        // Ein anderes Gerät ist Master
        if (!isSlave() || memcmp(masterMac, lowestMac, 6) != 0)
        {
            setMasterStatus(MASTER_SLAVE);
            memcpy(masterMac, lowestMac, 6);
            Serial.printf("[MASTER_DEBUG] Anderes Gerät ist Master: %s\n", macToString(masterMac).c_str());
        }
    }

    lastMasterSeen = millis();
}

void syncTimeWithMaster()
{
    if (isMaster())
        return;

    sendTimeSyncRequest();
}

void handleMasterHeartbeat(const uint8_t *incomingMasterMac, unsigned long masterTime)
{
    if (memcmp(masterMac, incomingMasterMac, 6) == 0)
    {
        lastMasterSeen = millis();
        Serial.printf("[MASTER_DEBUG] Heartbeat vom bekannten Master empfangen: %s\n", macToString(incomingMasterMac).c_str());
    }
    else
    {
        // Prüfe, ob der sendende Master eine niedrigere MAC-Adresse hat
        if (memcmp(incomingMasterMac, getMacAddress(), 6) < 0)
        {
            // Der andere Master hat eine niedrigere MAC - er sollte Master sein
            Serial.printf("[MASTER_DEBUG] Heartbeat von berechtigtem Master empfangen: %s (niedrigere MAC)\n", macToString(incomingMasterMac).c_str());
            setMasterStatus(MASTER_SLAVE);
            memcpy(masterMac, incomingMasterMac, 6);
            lastMasterSeen = millis();
        }
        else
        {
            // Der andere Master hat eine höhere MAC - wir sollten Master sein
            Serial.printf("[MASTER_DEBUG] Heartbeat von unberechtigtem Master empfangen: %s (höhere MAC)\n", macToString(incomingMasterMac).c_str());
            if (!isMaster())
            {
                determineMaster();
            }
        }
    }
}

void sendHeartbeat()
{
    if (isMaster())
    {
        sendMasterHeartbeat();
        lastHeartbeat = millis();
    }
}

void checkMasterOnline()
{
    if (isSlave() && (millis() - lastMasterSeen > heartbeatInterval * 3))
    {
        Serial.println("[MASTER_DEBUG] Master ist offline, bestimme neuen Master");
        // Master als offline markieren
        for (auto &dev : savedDevices)
        {
            if (memcmp(dev.mac, masterMac, 6) == 0)
            {
                dev.isOnline = false;
                break;
            }
        }
        determineMaster();
    }
}

// Zeit-Synchronisation
void requestTimeSync()
{
    if (isSlave())
    {
        sendTimeSyncRequest();
    }
}

void handleTimeSyncRequest(const uint8_t *requesterMac, unsigned long requesterTime)
{
    if (isMaster())
    {
        sendTimeSyncResponse(requesterMac, requesterTime, syncSequenceNumber++);
    }
}

void handleTimeSyncResponse(const uint8_t *incomingMasterMac, unsigned long masterTime, unsigned long roundTripTime)
{
    if (isSlave() && memcmp(masterMac, incomingMasterMac, 6) == 0)
    {
        long offset = masterTime - millis() + (roundTripTime / 2);
        updateTimeOffset(incomingMasterMac, offset);
        Serial.printf("[SYNC_DEBUG] Zeit-Offset zum Master: %ld ms\n", offset);
    }
}

long getTimeOffset(const uint8_t *deviceMac)
{
    for (const auto &dev : savedDevices)
    {
        if (memcmp(dev.mac, deviceMac, 6) == 0)
        {
            return dev.timeOffset;
        }
    }
    return 0;
}

void updateTimeOffset(const uint8_t *deviceMac, long offset)
{
    for (auto &dev : savedDevices)
    {
        if (memcmp(dev.mac, deviceMac, 6) == 0)
        {
            dev.timeOffset = offset;
            dev.lastSeen = millis();
            Serial.printf("[SYNC_DEBUG] Zeit-Offset für %s aktualisiert: %ld ms\n", macToString(deviceMac).c_str(), offset);
            return;
        }
    }
}

// Race-Management (nur Master)
void masterAddRaceStart(unsigned long startTime, const uint8_t *startDevice, unsigned long localTime)
{
    if (!isMaster())
        return;

    RaceEntry entry;
    entry.startTime = startTime;
    entry.startTimeLocal = localTime;
    memcpy(entry.startDevice, startDevice, 6);
    entry.isFinished = false;
    entry.finishTime = 0;
    entry.finishTimeLocal = 0;
    memset(entry.finishDevice, 0, 6);
    entry.duration = 0;

    raceQueue.push_back(entry);

    Serial.printf("[MASTER_DEBUG] Rennen gestartet von %s, Zeit: %lu\n", macToString(startDevice).c_str(), startTime);

    broadcastRaceUpdate();
    wsBrodcastMessage("{\"type\":\"laufCount\",\"value\":" + String(raceQueue.size()) + "}");
}

void masterFinishRace(unsigned long finishTime, const uint8_t *finishDevice, unsigned long localTime)
{
    if (!isMaster() || raceQueue.empty())
        return;

    // Finde das älteste unbeendete Rennen
    for (auto &race : raceQueue)
    {
        if (!race.isFinished)
        {
            race.isFinished = true;
            race.finishTime = finishTime;
            race.finishTimeLocal = localTime;
            memcpy(race.finishDevice, finishDevice, 6);

            // Berechne Dauer mit Zeit-Offset-Korrektur
            long startOffset = getTimeOffset(race.startDevice);
            long finishOffset = getTimeOffset(finishDevice);

            unsigned long correctedStartTime = race.startTime + startOffset;
            unsigned long correctedFinishTime = finishTime + finishOffset;

            race.duration = correctedFinishTime - correctedStartTime;

            Serial.printf("[MASTER_DEBUG] Rennen beendet: Start %s (%lu), Ziel %s (%lu), Dauer: %lu ms\n",
                          macToString(race.startDevice).c_str(), correctedStartTime,
                          macToString(finishDevice).c_str(), correctedFinishTime,
                          race.duration);

            broadcastRaceUpdate();
            wsBrodcastMessage("{\"type\":\"lastTime\",\"value\":" + String(race.duration) + "}");

            // Entferne beendete Rennen nach kurzer Zeit
            // (wird in einem separaten Cleanup-Prozess gemacht)
            break;
        }
    }
}

void broadcastRaceUpdate()
{
    if (isMaster())
    {
        sendRaceUpdate();
    }
}

void handleRaceUpdate(const uint8_t *data, int len)
{
    if (isSlave())
    {
        // Für jetzt implementieren wir eine einfache Lösung
        // In einer vollständigen Implementierung würden wir ein serialisiertes Format verwenden
        Serial.printf("[SLAVE_DEBUG] Race-Update vom Master empfangen (Länge: %d)\n", len);

        // Für die einfache Implementierung nehmen wir an, dass die Daten
        // direkt die Anzahl der laufenden Rennen enthalten
        if (len >= sizeof(RaceUpdateMessage))
        {
            RaceUpdateMessage msg;
            memcpy(&msg, data, sizeof(msg));

            // Synchronisiere die Anzahl der laufenden Rennen
            // (Eine vollständige Implementierung würde hier die tatsächlichen Race-Daten übertragen)
            Serial.printf("[SLAVE_DEBUG] Master hat %d laufende Rennen\n", msg.raceCount);
            wsBrodcastMessage("{\"type\":\"laufCount\",\"value\":" + String(msg.raceCount) + "}");
        }
    }
}

// Slave-Funktionen
void slaveHandleRaceStart(unsigned long startTime, const uint8_t *startDevice, unsigned long localTime)
{
    if (isSlave())
    {
        // Sende Event an Master
        broadcastRaceEvent(ROLE_START, startTime);
    }
}

void slaveHandleRaceFinish(unsigned long finishTime, const uint8_t *finishDevice, unsigned long localTime)
{
    if (isSlave())
    {
        // Sende Event an Master
        broadcastRaceEvent(ROLE_ZIEL, finishTime);
    }
}