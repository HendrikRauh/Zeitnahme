#include <data.h>
#include <Sensor.h>
#include <deque>

Role currentRole;

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

    DynamicJsonDocument doc(1024);
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
    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.to<JsonArray>();
    for (const auto &dev : savedDevices)
    {
        if (memcmp(dev.mac, getMacAddress(), 6) == 0)
            continue;
        JsonObject obj = arr.createNestedObject();
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
    }
    else
    {
        DeviceInfo info;
        memcpy(info.mac, mac, 6);
        info.role = role;
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
    }
    else
    {
        DeviceInfo info;
        memcpy(info.mac, mac, 6);
        info.role = role;
        savedDevices.push_back(info);
    }
    addDeviceToPeer(mac);
    writeDeviceListToPreferences();
    printDeviceLists();
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
    }
    else
    {
        Serial.printf("[ROLE_DEBUG] Gerät %s nicht gefunden, wird hinzugefügt mit Rolle %s\n", macToString(mac).c_str(), roleToString(role).c_str());
        DeviceInfo info;
        memcpy(info.mac, mac, 6);
        info.role = role;
        savedDevices.push_back(info);
    }
    writeDeviceListToPreferences();
    printDeviceLists();
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
    raceQueue.push_back({startTime});
}

bool finishRace(unsigned long finishTime, unsigned long &startTime, unsigned long &duration)
{
    if (raceQueue.empty())
        return false;
    startTime = raceQueue.front().startTime;
    duration = finishTime - startTime;
    raceQueue.pop_front();
    return true;
}