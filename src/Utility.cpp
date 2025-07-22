#include <Utility.h>
#include <server.h>

String roleToString(Role role)
{
    switch (role)
    {
    case ROLE_START:
        return "Start";
    case ROLE_ZIEL:
        return "Ziel";
    case ROLE_IGNORE:
        return "Ignorieren";
    case ROLE_DISPLAY:
        return "Anzeige";
    default:
        return "unknown";
    }
}

String masterStatusToString(MasterStatus status)
{
    switch (status)
    {
    case MASTER_UNKNOWN:
        return "Unknown";
    case MASTER_SLAVE:
        return "Slave";
    case MASTER_MASTER:
        return "Master";
    default:
        return "unknown";
    }
}

Role stringToRole(const String &text)
{
    if (text == "Start")
        return ROLE_START;
    if (text == "Ziel")
        return ROLE_ZIEL;
    if (text == "Anzeige")
        return ROLE_DISPLAY;
    return ROLE_IGNORE;
}

String macToString(const uint8_t *mac)
{
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

String macToShortString(const uint8_t *mac)
{
    char buf[9];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X",
             mac[3], mac[4], mac[5]);
    return String(buf);
}

bool findFullMacFromShortMac(const String &shortMac, uint8_t fullMac[6])
{
    // Suche in gespeicherten Geräten
    for (const auto &dev : getSavedDevices())
    {
        if (macToShortString(dev.mac) == shortMac)
        {
            memcpy(fullMac, dev.mac, 6);
            return true;
        }
    }

    // Suche in entdeckten Geräten
    for (const auto &dev : getDiscoveredDevices())
    {
        if (macToShortString(dev.mac) == shortMac)
        {
            memcpy(fullMac, dev.mac, 6);
            return true;
        }
    }

    // Prüfe eigene MAC-Adresse
    if (macToShortString(getMacAddress()) == shortMac)
    {
        memcpy(fullMac, getMacAddress(), 6);
        return true;
    }

    return false;
}

String statusToString(LichtschrankeStatus status)
{
    switch (status)
    {
    case STATUS_NORMAL:
        return "normal";
    case STATUS_TRIGGERED:
        return "triggered";
    case STATUS_COOLDOWN:
        return "cooldown";
    case STATUS_TRIGGERED_IN_COOLDOWN:
        return "triggered_in_cooldown";
    default:
        return "unknown";
    }
}

void handleIdentityMessage(const uint8_t *senderMac, Role senderRole)
{
    bool hasChanges = false;
    bool roleChanged = false;

    if (checkIfDeviceIsSaved(senderMac))
    {
        for (auto &dev : savedDevices) // Iterate directly over the original vector
        {
            if (memcmp(dev.mac, senderMac, 6) == 0)
            {
                // Onlinestatus immer aktualisieren
                dev.isOnline = true;
                dev.lastSeen = millis();
                hasChanges = true;
                if (dev.role != senderRole)
                {
                    changeSavedDevice(senderMac, senderRole);
                    roleChanged = true;
                    // WebSocket-Update für Rollenbestätigung senden (jetzt mit ArduinoJson)
                    JsonDocument doc;
                    doc["type"] = "device_role_changed";
                    JsonObject data = doc["data"].to<JsonObject>();
                    data["mac"] = macToShortString(senderMac);
                    data["role"] = roleToString(senderRole);
                    String json;
                    serializeJson(doc, json);
                    wsBrodcastMessage(json);
                }
                break;
            }
        }
    }
    else
    {
        // Prüfe, ob wir kürzlich eine Rollenänderungsanfrage an dieses Gerät gesendet haben
        // Falls ja, füge es jetzt zu unserer gespeicherten Liste hinzu
        if (checkIfDeviceIsDiscoveredList(senderMac) && senderRole != ROLE_IGNORE)
        {
            Serial.printf("[ROLE_DEBUG] Gerät %s nicht in gespeicherter Liste, aber in entdeckter Liste gefunden. Füge zur gespeicherten Liste hinzu.\n", macToString(senderMac).c_str());
            changeSavedDevice(senderMac, senderRole);
            roleChanged = true;
            // WebSocket-Update für Rollenbestätigung senden
            JsonDocument doc;
            doc["type"] = "device_role_changed";
            JsonObject data = doc["data"].to<JsonObject>();
            data["mac"] = macToShortString(senderMac);
            data["role"] = roleToString(senderRole);
            String json;
            serializeJson(doc, json);
            wsBrodcastMessage(json);
        }
    }
    // Aktualisiere entdeckte Geräte Liste
    if (!checkIfDeviceIsDiscoveredList(senderMac))
    {
        addDiscoveredDevice(senderMac, senderRole);
        hasChanges = true;
    }
    else
    {
        // Prüfe, ob sich die Rolle in den entdeckten Geräten geändert hat
        bool discoveredRoleChanged = false;
        for (const auto &dev : getDiscoveredDevices())
        {
            if (memcmp(dev.mac, senderMac, 6) == 0)
            {
                if (dev.role != senderRole)
                {
                    updateDiscoveredDeviceRole(senderMac, senderRole);
                    hasChanges = true;
                    if (!roleChanged) // Nur senden wenn nicht bereits von savedDevices gesendet
                    {
                        // WebSocket-Update für Rollenbestätigung senden (jetzt mit ArduinoJson)
                        JsonDocument doc;
                        doc["type"] = "device_role_changed";
                        JsonObject data = doc["data"].to<JsonObject>();
                        data["mac"] = macToShortString(senderMac);
                        data["role"] = roleToString(senderRole);
                        String json;
                        serializeJson(doc, json);
                        wsBrodcastMessage(json);
                    }
                }
                discoveredRoleChanged = true;
                break;
            }
        }
        if (!discoveredRoleChanged)
        {
            addDiscoveredDevice(senderMac, senderRole);
            hasChanges = true;
        }
    }

    // Nur broadcasten wenn es tatsächlich Änderungen gab
    if (hasChanges)
    {
        printDeviceLists();
        // Master-Status neu bestimmen wenn sich etwas geändert hat
        determineMaster();
    }
}

void printDeviceLists()
{
    Serial.println("\n=== Geräteübersicht ===");
    Serial.printf("Eigene MAC: %s\n", macToString(getMacAddress()).c_str());
    Serial.printf("Master-Status: %s\n", masterStatusToString(getMasterStatus()).c_str());
    if (isSlave())
    {
        Serial.printf("Master-MAC: %s\n", macToString(getMasterMac()).c_str());
    }

    Serial.println("\nEntdeckte Geräte:");
    for (const auto &dev : getDiscoveredDevices())
    {
        Serial.printf("%s - Rolle: %s, Online: %s, Offset: %ld ms\n",
                      macToString(dev.mac).c_str(),
                      roleToString(dev.role).c_str(),
                      dev.isOnline ? "Ja" : "Nein",
                      dev.timeOffset);
    }

    Serial.println("\nGespeicherte Geräte:");
    for (const auto &dev : getSavedDevices())
    {
        Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X - Rolle: %s, Online: %s, Offset: %ld ms\n",
                      dev.mac[0], dev.mac[1], dev.mac[2], dev.mac[3], dev.mac[4], dev.mac[5],
                      roleToString(dev.role).c_str(),
                      dev.isOnline ? "Ja" : "Nein",
                      dev.timeOffset);
    }
    Serial.println("=====================\n");
}

String getSavedDevicesJson()
{
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (const auto &dev : getSavedDevices())
    {
        JsonObject obj = arr.add<JsonObject>();

        obj["mac"] = macToShortString(dev.mac);
        obj["role"] = roleToString(dev.role);
    }

    String jsonStr;
    serializeJson(arr, jsonStr);
    return jsonStr;
}

String getDiscoveredDevicesJson()
{
    sendDiscoveryMessage();

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (const auto &dev : getDiscoveredDevices())
    {
        JsonObject obj = arr.add<JsonObject>();

        obj["mac"] = macToShortString(dev.mac);
        obj["role"] = roleToString(dev.role);
    }

    String jsonStr;
    serializeJson(arr, jsonStr);
    return jsonStr;
}