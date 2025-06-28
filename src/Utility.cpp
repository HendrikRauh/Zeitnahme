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
        broadcastDiscoveredDevices();
        printDeviceLists();
    }
}

void printDeviceLists()
{
    Serial.println("\n=== Geräteübersicht ===");
    Serial.printf("Eigene MAC: %s\n", macToString(getMacAddress()).c_str());

    Serial.println("\nEntdeckte Geräte:");
    for (const auto &dev : getDiscoveredDevices())
    {
        Serial.printf("%s - Rolle: %s\n",
                      macToString(dev.mac).c_str(),
                      roleToString(dev.role));
    }

    Serial.println("\nGespeicherte Geräte:");
    for (const auto &dev : getSavedDevices())
    {
        Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X - Rolle: %s\n",
                      dev.mac[0], dev.mac[1], dev.mac[2], dev.mac[3], dev.mac[4], dev.mac[5],
                      roleToString(dev.role).c_str());
    }
    Serial.println("=====================\n");
}

String getSavedDevicesJson()
{
    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.to<JsonArray>();

    for (const auto &dev : getSavedDevices())
    {
        JsonObject obj = arr.createNestedObject();

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

    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.to<JsonArray>();

    for (const auto &dev : getDiscoveredDevices())
    {
        JsonObject obj = arr.createNestedObject();

        obj["mac"] = macToShortString(dev.mac);
        obj["role"] = roleToString(dev.role);
    }

    String jsonStr;
    serializeJson(arr, jsonStr);
    return jsonStr;
}