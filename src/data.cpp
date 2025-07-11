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

// Performance-Optimierung: Caching für häufig verwendete Werte
static Role cachedOwnRole = ROLE_IGNORE; // Verwende ROLE_IGNORE als Default
static bool roleLoaded = false;
static float cachedMinDistanceCache = 2.0f;
static float cachedMaxDistanceCache = 100.0f;
static bool distanceCacheLoaded = false;

// Cache-Invalidierung für Distanzen
void invalidateDistanceCache()
{
    distanceCacheLoaded = false;
    Serial.println("[CACHE_DEBUG] Distanz-Cache invalidiert");
}

Role getOwnRole()
{
    // Cache für bessere Performance
    if (roleLoaded)
    {
        return cachedOwnRole;
    }

    preferences.begin("lichtschranke", true);
    int role = preferences.getUInt("role", 1);
    preferences.end();
    cachedOwnRole = static_cast<Role>(role);
    roleLoaded = true;
    return cachedOwnRole;
}

void saveOwnRole(Role role)
{
    preferences.begin("lichtschranke", false);
    preferences.putUInt("role", static_cast<int>(role));
    preferences.end();
    // Cache aktualisieren
    cachedOwnRole = role;
    roleLoaded = true;
}

void loadDeviceListFromPreferences()
{
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
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto &dev : savedDevices)
    {
        if (memcmp(dev.mac, getMacAddress(), 6) == 0)
            continue;
        JsonObject obj = arr.add<JsonObject>();
        obj["mac"] = macToString(dev.mac);
        obj["role"] = roleToString(dev.role);
    }
    String jsonStr;
    serializeJson(arr, jsonStr);
    preferences.begin("lichtschranke", false);
    preferences.putString("devices", jsonStr);
    preferences.end();
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
    auto it = std::find_if(savedDevices.begin(), savedDevices.end(),
                           [&](const DeviceInfo &d)
                           { return memcmp(d.mac, mac, 6) == 0; });
    if (it != savedDevices.end())
    {
        it->role = role;
        // Onlinestatus wird nur durch empfangene Nachrichten gesetzt
    }
    else
    {
        DeviceInfo info;
        memcpy(info.mac, mac, 6);
        info.role = role;
        info.timeOffset = 0;
        info.isOnline = false;
        info.lastSeen = 0;
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
        // Onlinestatus wird nur durch empfangene Nachrichten gesetzt
    }
    else
    {
        Serial.printf("[ROLE_DEBUG] Gerät %s nicht gefunden, wird hinzugefügt mit Rolle %s\n", macToString(mac).c_str(), roleToString(role).c_str());
        DeviceInfo info;
        memcpy(info.mac, mac, 6);
        info.role = role;
        info.timeOffset = 0;
        info.isOnline = false;
        info.lastSeen = 0;
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

#ifndef DEFAULT_MIN_DISTANCE_CM
#define DEFAULT_MIN_DISTANCE_CM 2
#endif
#ifndef DEFAULT_MAX_DISTANCE_CM
#define DEFAULT_MAX_DISTANCE_CM 100
#endif

int getMinDistance()
{
    preferences.begin("lichtschranke", true);
    int minDistance = preferences.getInt("minDistance", DEFAULT_MIN_DISTANCE_CM);
    preferences.end();
    Serial.printf("[DISTANCE_DEBUG] Min-Distanz geladen: %d cm\n", minDistance);
    return minDistance;
}

int getMaxDistance()
{
    preferences.begin("lichtschranke", true);
    int maxDistance = preferences.getInt("maxDistance", DEFAULT_MAX_DISTANCE_CM);
    preferences.end();
    Serial.printf("[DISTANCE_DEBUG] Max-Distanz geladen: %d cm\n", maxDistance);
    return maxDistance;
}

void setMinDistance(int minDistance)
{
    if (minDistance < 2 || minDistance > 200)
    {
        Serial.printf("[DISTANCE_DEBUG] Ungültiger Min-Distanz-Wert: %d cm. Verwende Default (%d cm).\n", minDistance, DEFAULT_MIN_DISTANCE_CM);
        minDistance = DEFAULT_MIN_DISTANCE_CM;
    }

    preferences.begin("lichtschranke", false);
    preferences.putInt("minDistance", minDistance);
    preferences.end();

    Serial.printf("[DISTANCE_DEBUG] Min-Distanz gesetzt auf: %d cm\n", minDistance);

    // Sensor-Cache aktualisieren
    updateDistanceCache();
}

void setMaxDistance(int maxDistance)
{
    if (maxDistance < 2 || maxDistance > 200)
    {
        Serial.printf("[DISTANCE_DEBUG] Ungültiger Max-Distanz-Wert: %d cm. Verwende Default (%d cm).\n", maxDistance, DEFAULT_MAX_DISTANCE_CM);
        maxDistance = DEFAULT_MAX_DISTANCE_CM;
    }

    preferences.begin("lichtschranke", false);
    preferences.putInt("maxDistance", maxDistance);
    preferences.end();

    Serial.printf("[DISTANCE_DEBUG] Max-Distanz gesetzt auf: %d cm\n", maxDistance);

    // Sensor-Cache aktualisieren
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
    int runningRaces = 0;
    for (const auto &race : raceQueue)
    {
        if (!race.isFinished)
            runningRaces++;
    }
    return runningRaces;
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

    // Debug: Aktuelle MAC-Adresse ausgeben
    Serial.printf("[MASTER_DEBUG] Eigene MAC-Adresse: %s\n", macToString(getMacAddress()).c_str());

    uint8_t lowestMac[6];
    memcpy(lowestMac, getMacAddress(), 6);
    bool foundLower = false;

    Serial.printf("[MASTER_DEBUG] Starte mit eigener MAC als niedrigste: %s\n", macToString(lowestMac).c_str());

    // Prüfe alle gespeicherten Geräte die online sind
    Serial.printf("[MASTER_DEBUG] Prüfe %d gespeicherte Geräte...\n", savedDevices.size());
    for (const auto &dev : savedDevices)
    {
        Serial.printf("[MASTER_DEBUG] Gerät %s: Online=%d, Vergleich mit aktuell niedrigster MAC\n",
                      macToString(dev.mac).c_str(), dev.isOnline);

        if (dev.isOnline && memcmp(dev.mac, lowestMac, 6) < 0)
        {
            Serial.printf("[MASTER_DEBUG] Gefunden niedrigere MAC: %s < %s\n",
                          macToString(dev.mac).c_str(), macToString(lowestMac).c_str());
            memcpy(lowestMac, dev.mac, 6);
            foundLower = true;
        }
    }

    // Prüfe auch alle entdeckten Geräte
    Serial.printf("[MASTER_DEBUG] Prüfe %d entdeckte Geräte...\n", discoveredDevices.size());
    for (const auto &dev : discoveredDevices)
    {
        Serial.printf("[MASTER_DEBUG] Entdecktes Gerät %s: Online=%d, Vergleich mit aktuell niedrigster MAC\n",
                      macToString(dev.mac).c_str(), dev.isOnline);

        if (dev.isOnline && memcmp(dev.mac, lowestMac, 6) < 0)
        {
            Serial.printf("[MASTER_DEBUG] Gefunden niedrigere MAC: %s < %s\n",
                          macToString(dev.mac).c_str(), macToString(lowestMac).c_str());
            memcpy(lowestMac, dev.mac, 6);
            foundLower = true;
        }
    }

    Serial.printf("[MASTER_DEBUG] Endgültige niedrigste MAC: %s (foundLower=%d)\n",
                  macToString(lowestMac).c_str(), foundLower);

    if (!foundLower)
    {
        // Dieses Gerät hat die niedrigste MAC
        if (!isMaster())
        {
            Serial.printf("[MASTER_DEBUG] Dieses Gerät wird Master (niedrigste MAC): %s\n", macToString(getMacAddress()).c_str());
            setMasterStatus(MASTER_MASTER);
            memcpy(masterMac, getMacAddress(), 6);
        }
        else
        {
            Serial.printf("[MASTER_DEBUG] Dieses Gerät bleibt Master: %s\n", macToString(getMacAddress()).c_str());
        }
    }
    else
    {
        // Ein anderes Gerät ist Master
        if (!isSlave() || memcmp(masterMac, lowestMac, 6) != 0)
        {
            Serial.printf("[MASTER_DEBUG] Anderes Gerät wird Master: %s (aktueller Status: %s)\n",
                          macToString(lowestMac).c_str(), isMaster() ? "Master" : (isSlave() ? "Slave" : "Unknown"));
            setMasterStatus(MASTER_SLAVE);
            memcpy(masterMac, lowestMac, 6);
        }
        else
        {
            Serial.printf("[MASTER_DEBUG] Anderes Gerät bleibt Master: %s\n", macToString(lowestMac).c_str());
        }
    }

    lastMasterSeen = millis();
    Serial.printf("[MASTER_DEBUG] Master-Bestimmung abgeschlossen. Master: %s, Eigener Status: %s\n",
                  macToString(masterMac).c_str(), isMaster() ? "Master" : (isSlave() ? "Slave" : "Unknown"));
}

void syncTimeWithMaster()
{
    if (isMaster())
        return;

    sendTimeSyncRequest();
}

void handleMasterHeartbeat(const uint8_t *incomingMasterMac, unsigned long masterTime)
{
    Serial.printf("[MASTER_DEBUG] Heartbeat empfangen von: %s (aktueller Master: %s)\n",
                  macToString(incomingMasterMac).c_str(), macToString(masterMac).c_str());

    if (memcmp(masterMac, incomingMasterMac, 6) == 0)
    {
        lastMasterSeen = millis();
        Serial.printf("[MASTER_DEBUG] Heartbeat vom bekannten Master empfangen: %s\n", macToString(incomingMasterMac).c_str());
    }
    else
    {
        Serial.printf("[MASTER_DEBUG] Heartbeat von anderem Master. Vergleiche MACs:\n");
        Serial.printf("[MASTER_DEBUG] Incoming Master: %s\n", macToString(incomingMasterMac).c_str());
        Serial.printf("[MASTER_DEBUG] Eigene MAC: %s\n", macToString(getMacAddress()).c_str());

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
                Serial.printf("[MASTER_DEBUG] Führe Master-Bestimmung durch, da wir nicht Master sind\n");
                determineMaster();
            }
            else
            {
                Serial.printf("[MASTER_DEBUG] Ignoriere Heartbeat, da wir bereits Master sind\n");
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
    // Prüfe erst die gespeicherten Geräte
    for (const auto &dev : savedDevices)
    {
        if (memcmp(dev.mac, deviceMac, 6) == 0)
        {
            Serial.printf("[SYNC_DEBUG] Zeit-Offset für %s aus savedDevices: %ld ms\n",
                          macToString(deviceMac).c_str(), dev.timeOffset);
            return dev.timeOffset;
        }
    }

    // Prüfe auch die entdeckten Geräte
    for (const auto &dev : discoveredDevices)
    {
        if (memcmp(dev.mac, deviceMac, 6) == 0)
        {
            Serial.printf("[SYNC_DEBUG] Zeit-Offset für %s aus discoveredDevices: %ld ms\n",
                          macToString(deviceMac).c_str(), dev.timeOffset);
            return dev.timeOffset;
        }
    }

    Serial.printf("[SYNC_DEBUG] Kein Zeit-Offset für %s gefunden, verwende 0\n",
                  macToString(deviceMac).c_str());
    return 0;
}

void updateTimeOffset(const uint8_t *deviceMac, long offset)
{
    bool updated = false;

    // Aktualisiere in savedDevices
    for (auto &dev : savedDevices)
    {
        if (memcmp(dev.mac, deviceMac, 6) == 0)
        {
            dev.timeOffset = offset;
            dev.lastSeen = millis();
            updated = true;
            Serial.printf("[SYNC_DEBUG] Zeit-Offset für %s in savedDevices aktualisiert: %ld ms\n",
                          macToString(deviceMac).c_str(), offset);
            break;
        }
    }

    // Aktualisiere auch in discoveredDevices
    for (auto &dev : discoveredDevices)
    {
        if (memcmp(dev.mac, deviceMac, 6) == 0)
        {
            dev.timeOffset = offset;
            dev.lastSeen = millis();
            updated = true;
            Serial.printf("[SYNC_DEBUG] Zeit-Offset für %s in discoveredDevices aktualisiert: %ld ms\n",
                          macToString(deviceMac).c_str(), offset);
            break;
        }
    }

    if (!updated)
    {
        Serial.printf("[SYNC_DEBUG] Gerät %s nicht gefunden für Zeit-Offset-Update\n",
                      macToString(deviceMac).c_str());
    }
}

// Race-Management (nur Master)
void masterAddRaceStart(unsigned long startTime, const uint8_t *startDevice, unsigned long localTime)
{
    if (!isMaster())
    {
        Serial.printf("[MASTER_DEBUG] masterAddRaceStart aufgerufen, aber dieses Gerät ist nicht Master (Status: %s)\n",
                      isMaster() ? "Master" : (isSlave() ? "Slave" : "Unknown"));
        return;
    }

    // Berechne Zeit-Offset zwischen Master und Start-Gerät
    if (memcmp(startDevice, getMacAddress(), 6) != 0)
    {
        // Zeit-Offset: (unsere Zeit) - (lokale Zeit des anderen Geräts)
        // Dieser Offset wird später zu der anderen Zeit addiert, um sie zu korrigieren
        long estimatedOffset = (long)millis() - (long)startTime;
        updateTimeOffset(startDevice, estimatedOffset);
        Serial.printf("[MASTER_DEBUG] Zeit-Offset für Start-Gerät %s geschätzt: %ld ms\n",
                      macToString(startDevice).c_str(), estimatedOffset);
    }

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

    Serial.printf("[MASTER_DEBUG] Rennen gestartet von %s, Zeit: %lu (Queue-Größe: %d)\n",
                  macToString(startDevice).c_str(), startTime, raceQueue.size());

    broadcastRaceUpdate();

    // Zähle nur laufende Rennen für die Anzeige
    int runningRaces = 0;
    for (const auto &race : raceQueue)
    {
        if (!race.isFinished)
            runningRaces++;
    }
    wsBrodcastMessage("{\"type\":\"laufCount\",\"value\":" + String(runningRaces) + "}");
}

void masterFinishRace(unsigned long finishTime, const uint8_t *finishDevice, unsigned long localTime)
{
    if (!isMaster())
    {
        Serial.printf("[MASTER_DEBUG] masterFinishRace aufgerufen, aber dieses Gerät ist nicht Master (Status: %s)\n",
                      isMaster() ? "Master" : (isSlave() ? "Slave" : "Unknown"));
        return;
    }

    if (raceQueue.empty())
    {
        Serial.printf("[MASTER_DEBUG] masterFinishRace: Keine offenen Rennen in der Queue\n");
        return;
    }

    // Berechne Zeit-Offset zwischen Master und Ziel-Gerät
    if (memcmp(finishDevice, getMacAddress(), 6) != 0)
    {
        // Zeit-Offset: (unsere Zeit) - (lokale Zeit des anderen Geräts)
        // Dieser Offset wird später zu der anderen Zeit addiert, um sie zu korrigieren
        long estimatedOffset = (long)millis() - (long)finishTime;
        updateTimeOffset(finishDevice, estimatedOffset);
        Serial.printf("[MASTER_DEBUG] Zeit-Offset für Ziel-Gerät %s geschätzt: %ld ms\n",
                      macToString(finishDevice).c_str(), estimatedOffset);
    }

    Serial.printf("[MASTER_DEBUG] masterFinishRace von %s, Zeit: %lu, Queue-Größe: %d\n",
                  macToString(finishDevice).c_str(), finishTime, raceQueue.size());

    // Debug: Zeige Status aller Rennen in der Queue
    Serial.printf("[MASTER_DEBUG] Aktuelle Queue-Inhalte:\n");
    for (size_t i = 0; i < raceQueue.size(); i++)
    {
        const auto &race = raceQueue[i];
        Serial.printf("[MASTER_DEBUG] Rennen %d: Start=%s, Beendet=%s, Zeit=%lu\n",
                      i, macToString(race.startDevice).c_str(),
                      race.isFinished ? "Ja" : "Nein", race.startTime);
    }

    // Finde das älteste unbeendete Rennen
    bool foundRace = false;
    for (auto &race : raceQueue)
    {
        if (!race.isFinished)
        {
            foundRace = true;
            race.isFinished = true;
            race.finishTime = finishTime;
            race.finishTimeLocal = localTime;
            memcpy(race.finishDevice, finishDevice, 6);

            // Berechne Dauer mit Zeit-Offset-Korrektur
            long startOffset = getTimeOffset(race.startDevice);
            long finishOffset = getTimeOffset(finishDevice);

            Serial.printf("[MASTER_DEBUG] Rohe Zeiten: Start=%lu, Ziel=%lu\n", race.startTime, finishTime);
            Serial.printf("[MASTER_DEBUG] Zeit-Offsets: Start=%ld ms, Ziel=%ld ms\n", startOffset, finishOffset);

            // Korrigiere die Zeiten mit den Offsets
            long correctedStartTime = (long)race.startTime + startOffset;
            long correctedFinishTime = (long)finishTime + finishOffset;

            Serial.printf("[MASTER_DEBUG] Korrigierte Zeiten: Start=%ld, Ziel=%ld\n", correctedStartTime, correctedFinishTime);

            // Berechne Dauer und verhindere negative Werte
            long duration = correctedFinishTime - correctedStartTime;
            if (duration < 0)
            {
                Serial.printf("[MASTER_DEBUG] WARNUNG: Negative Dauer erkannt! Start: %ld, Ziel: %ld, Dauer: %ld\n",
                              correctedStartTime, correctedFinishTime, duration);
                duration = 0; // Setze auf 0 wenn negativ
            }
            race.duration = (unsigned long)duration;

            Serial.printf("[MASTER_DEBUG] Rennen beendet: Start %s (%ld), Ziel %s (%ld), Dauer: %lu ms\n",
                          macToString(race.startDevice).c_str(), correctedStartTime,
                          macToString(finishDevice).c_str(), correctedFinishTime,
                          race.duration);

            broadcastRaceUpdate();
            wsBrodcastMessage("{\"type\":\"lastTime\",\"value\":" + String(race.duration) + "}");

            // Aktualisiere Laufzähler nach dem Beenden des Rennens
            int runningRaces = 0;
            for (const auto &raceEntry : raceQueue)
            {
                if (!raceEntry.isFinished)
                    runningRaces++;
            }
            wsBrodcastMessage("{\"type\":\"laufCount\",\"value\":" + String(runningRaces) + "}");

            break;
        }
    }

    if (!foundRace)
    {
        Serial.printf("[MASTER_DEBUG] masterFinishRace: Kein offenes Rennen gefunden zum Beenden\n");
        // Sende trotzdem Laufzähler-Update, falls alle Rennen beendet sind
        int runningRaces = 0;
        for (const auto &raceEntry : raceQueue)
        {
            if (!raceEntry.isFinished)
                runningRaces++;
        }
        wsBrodcastMessage("{\"type\":\"laufCount\",\"value\":" + String(runningRaces) + "}");
    }
}

void cleanupFinishedRaces()
{
    if (!isMaster())
        return;

    // Entferne beendete Rennen, die älter als 15 Sekunden sind (reduziert von 30)
    // ABER: Behalte Rennen mit 0ms Dauer länger, da sie Probleme anzeigen können
    auto now = millis();
    auto it = raceQueue.begin();
    bool removedAny = false;

    while (it != raceQueue.end())
    {
        if (it->isFinished && (now - it->finishTime > 15000)) // 15 Sekunden (reduziert)
        {
            // Spezialbehandlung für Rennen mit 0ms Dauer - behalte sie länger
            if (it->duration == 0 && (now - it->finishTime < 30000)) // 30 Sekunden für 0ms-Rennen
            {
                ++it;
                continue;
            }

            Serial.printf("[MASTER_DEBUG] Entferne altes Rennen: Start %s, Ziel %s, Dauer: %lu ms\n",
                          macToString(it->startDevice).c_str(),
                          macToString(it->finishDevice).c_str(),
                          it->duration);
            it = raceQueue.erase(it);
            removedAny = true;
        }
        else
        {
            ++it;
        }
    }

    // Sende Update nur wenn sich etwas geändert hat
    if (removedAny)
    {
        Serial.printf("[MASTER_DEBUG] Cleanup abgeschlossen. Neue Queue-Größe: %d\n", raceQueue.size());
        broadcastRaceUpdate();

        // Aktualisiere WebSocket-Clients mit neuem Laufzähler
        int runningRaces = 0;
        for (const auto &race : raceQueue)
        {
            if (!race.isFinished)
                runningRaces++;
        }
        wsBrodcastMessage("{\"type\":\"laufCount\",\"value\":" + String(runningRaces) + "}");
    }
}

void broadcastRaceUpdate()
{
    if (isMaster())
    {
        sendFullSync(); // Verwende Full-Sync statt nur Race-Update
    }
}

void handleRaceUpdate(const uint8_t *data, int len)
{
    if (isSlave())
    {
        Serial.printf("[SLAVE_DEBUG] Race-Update vom Master empfangen (Länge: %d)\n", len);

        if (len >= sizeof(RaceUpdateMessage))
        {
            RaceUpdateMessage msg;
            memcpy(&msg, data, sizeof(msg));

            // Prüfe, ob die Nachricht vom bekannten Master kommt
            if (memcmp(msg.masterMac, getMasterMac(), 6) == 0)
            {
                // Synchronisiere die Race-Queue komplett mit den Master-Daten
                raceQueue.clear();

                for (int i = 0; i < msg.raceCount; i++)
                {
                    raceQueue.push_back(msg.races[i]);
                }

                Serial.printf("[SLAVE_DEBUG] Race-Queue synchronisiert: %d Rennen vom Master\n", msg.raceCount);

                // Aktualisiere WebSocket-Clients mit aktuellen Daten
                updateWebSocketClients();
            }
            else
            {
                Serial.printf("[SLAVE_DEBUG] Race-Update von unbekanntem Master ignoriert: %s\n", macToString(msg.masterMac).c_str());
            }
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

void updateWebSocketClients()
{
    // Sende Master-Status
    String masterStatus = isMaster() ? "Master" : "Slave";
    wsBrodcastMessage("{\"type\":\"masterStatus\",\"value\":\"" + masterStatus + "\"}");

    // Sende Master-MAC
    wsBrodcastMessage("{\"type\":\"masterMac\",\"value\":\"" + macToString(getMasterMac()) + "\"}");

    // Sende Zeit-Offset (nur bei Slaves)
    if (isSlave())
    {
        long offset = getTimeOffset(getMasterMac());
        wsBrodcastMessage("{\"type\":\"timeOffset\",\"value\":" + String(offset) + "}");
    }

    // Sende Laufzähler (nur laufende Rennen)
    int runningRaces = 0;
    for (const auto &race : raceQueue)
    {
        if (!race.isFinished)
            runningRaces++;
    }
    wsBrodcastMessage("{\"type\":\"laufCount\",\"value\":" + String(runningRaces) + "}");

    // Sende letzte Zeit (letztes beendetes Rennen)
    for (auto it = raceQueue.rbegin(); it != raceQueue.rend(); ++it)
    {
        if (it->isFinished)
        {
            wsBrodcastMessage("{\"type\":\"lastTime\",\"value\":" + String(it->duration) + "}");
            break;
        }
    }

    // Sende vollständige Race-Liste als JSON
    JsonDocument doc;
    JsonArray races = doc.to<JsonArray>();

    for (const auto &race : raceQueue)
    {
        JsonObject raceObj = races.add<JsonObject>();
        raceObj["startTime"] = race.startTime;
        raceObj["startDevice"] = macToString(race.startDevice);
        raceObj["isFinished"] = race.isFinished;

        if (race.isFinished)
        {
            raceObj["finishTime"] = race.finishTime;
            raceObj["finishDevice"] = macToString(race.finishDevice);
            raceObj["duration"] = race.duration;
        }
    }

    String raceListJson;
    serializeJson(doc, raceListJson);
    wsBrodcastMessage("{\"type\":\"raceList\",\"data\":" + raceListJson + "}");
}

void handleFullSync(const uint8_t *data, int len)
{
    if (isSlave())
    {
        Serial.printf("[SLAVE_DEBUG] Full-Sync vom Master empfangen (Länge: %d)\n", len);

        if (len >= sizeof(FullSyncMessage))
        {
            FullSyncMessage msg;
            memcpy(&msg, data, sizeof(msg));

            // Prüfe, ob die Nachricht vom bekannten Master kommt
            if (memcmp(msg.masterMac, getMasterMac(), 6) == 0)
            {
                // Synchronisiere die Race-Queue komplett mit den Master-Daten
                raceQueue.clear();

                for (int i = 0; i < msg.raceCount; i++)
                {
                    raceQueue.push_back(msg.races[i]);
                }

                // Berechne Zeit-Offset zum Master
                long timeOffset = msg.masterTime - millis();
                updateTimeOffset(msg.masterMac, timeOffset);

                Serial.printf("[SLAVE_DEBUG] Full-Sync verarbeitet: %d Rennen, Zeit-Offset: %ld ms, letzte Zeit: %lu ms\n",
                              msg.raceCount, timeOffset, msg.lastFinishedTime);

                // Aktualisiere WebSocket-Clients mit allen Daten
                updateWebSocketClients();

                // Sende auch die letzte Zeit, falls vorhanden
                if (msg.lastFinishedTime > 0)
                {
                    wsBrodcastMessage("{\"type\":\"lastTime\",\"value\":" + String(msg.lastFinishedTime) + "}");
                }
            }
            else
            {
                Serial.printf("[SLAVE_DEBUG] Full-Sync von unbekanntem Master ignoriert: %s\n", macToString(msg.masterMac).c_str());
            }
        }
    }
}

void updateDiscoveredDeviceRole(const uint8_t *mac, Role newRole)
{
    Serial.printf("[ROLE_DEBUG] Aktualisiere Rolle in entdeckten Geräten: MAC %s, neue Rolle %s\n", macToString(mac).c_str(), roleToString(newRole).c_str());
    for (auto &dev : discoveredDevices)
    {
        if (memcmp(dev.mac, mac, 6) == 0)
        {
            dev.role = newRole;
            dev.isOnline = true;
            dev.lastSeen = millis();
            Serial.printf("[ROLE_DEBUG] Rolle in entdeckten Geräten erfolgreich aktualisiert\n");
            return;
        }
    }
    Serial.printf("[ROLE_DEBUG] Gerät %s nicht in entdeckten Geräten gefunden\n", macToString(mac).c_str());
}