#include <Utility.h>

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
    Serial.printf("[ROLE_DEBUG] Identity empfangen von MAC %s, Rolle %s\n", macToString(senderMac).c_str(), roleToString(senderRole));

    bool hasChanges = false;

    if (checkIfDeviceIsSaved(senderMac))
    {
        // Prüfe, ob sich die Rolle geändert hat
        for (const auto &dev : getSavedDevices())
        {
            if (memcmp(dev.mac, senderMac, 6) == 0)
            {
                if (dev.role != senderRole)
                {
                    Serial.printf("[ROLE_DEBUG] Rolle von Gerät %s hat sich geändert: %s -> %s\n",
                                  macToString(senderMac).c_str(), roleToString(dev.role).c_str(), roleToString(senderRole));
                    changeSavedDevice(senderMac, senderRole);
                    hasChanges = true;
                }
                else
                {
                    Serial.printf("[ROLE_DEBUG] Gerät %s ist bereits bekannt mit korrekter Rolle %s\n",
                                  macToString(senderMac).c_str(), roleToString(senderRole));

                    // Aktualisiere lastSeen für bekannte Geräte OHNE Zeit-Offset zu ändern
                    // Markiere Gerät als online in savedDevices
                    for (auto &device : savedDevices)
                    {
                        if (memcmp(device.mac, senderMac, 6) == 0)
                        {
                            device.isOnline = true;
                            device.lastSeen = millis();
                            Serial.printf("[ROLE_DEBUG] Gerät %s als online markiert (Zeit-Offset beibehalten: %ld ms)\n",
                                          macToString(senderMac).c_str(), device.timeOffset);
                            break;
                        }
                    }
                    hasChanges = true; // Trigger Master-Neubestimmung
                }
                break;
            }
        }
    }

    if (!checkIfDeviceIsDiscoveredList(senderMac))
    {
        Serial.printf("[ROLE_DEBUG] Gerät %s wird zu gefunden Geräten hinzugefügt\n", macToString(senderMac).c_str());
        addDiscoveredDevice(senderMac, senderRole);
        hasChanges = true;
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