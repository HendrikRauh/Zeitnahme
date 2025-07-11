#include <task.h>

unsigned long lastScream = 0;
LichtschrankeStatus status = STATUS_NORMAL;
unsigned long lastTrigger = 0;
unsigned long cooldownUntil = 0;

void lichtschrankeTask(void *pvParameters)
{
    // Cache für bessere Performance
    Role cachedRole = getOwnRole();
    bool isMasterCached = isMaster();

    for (;;)
    {
        MeasureResult res = measure();
        LichtschrankeStatus prevStatus = status;
        if (res.triggered && status == STATUS_NORMAL && res.time >= cooldownUntil)
        {
            // Sofortige Zeiterfassung für maximale Genauigkeit
            unsigned long triggerTime = micros() / 1000; // Mikrosekunden-Genauigkeit

            Serial.printf("*** TRIGGER ERKANNT! Zeit: %lu, Rolle: %d, Master: %s ***\n",
                          triggerTime, cachedRole, isMasterCached ? "JA" : "NEIN");

            // Optimierte Rolle-Abfrage ohne Funktionsaufruf
            if (cachedRole == ROLE_START)
            {
                Serial.println("-> START-Sensor ausgelöst");
                if (isMasterCached)
                {
                    masterAddRaceStart(triggerTime, getMacAddress(), triggerTime);
                }
                else
                {
                    slaveHandleRaceStart(triggerTime, getMacAddress(), triggerTime);
                }
            }
            else if (cachedRole == ROLE_ZIEL)
            {
                Serial.println("-> ZIEL-Sensor ausgelöst");
                if (isMasterCached)
                {
                    masterFinishRace(triggerTime, getMacAddress(), triggerTime);
                }
                else
                {
                    slaveHandleRaceFinish(triggerTime, getMacAddress(), triggerTime);
                }
            }
            else
            {
                Serial.printf("-> IGNORIERT - Rolle ist %d\n", cachedRole);
            }
            lastTrigger = triggerTime;
            status = STATUS_TRIGGERED;
        }
        // Optimierte Status-Maschine mit weniger Vergleichen
        else if (!res.triggered)
        {
            if (status == STATUS_TRIGGERED || status == STATUS_TRIGGERED_IN_COOLDOWN)
            {
                status = STATUS_COOLDOWN;
                cooldownUntil = res.time + COOLDOWN_MS;
            }
            else if ((status == STATUS_COOLDOWN || status == STATUS_TRIGGERED_IN_COOLDOWN) && res.time >= cooldownUntil)
            {
                status = STATUS_NORMAL;
            }
        }
        else if (res.triggered)
        {
            if (status == STATUS_COOLDOWN || status == STATUS_TRIGGERED_IN_COOLDOWN)
            {
                status = STATUS_TRIGGERED_IN_COOLDOWN;
                cooldownUntil = res.time + COOLDOWN_MS;
            }
            else if (status == STATUS_TRIGGERED && (res.time - lastTrigger > 500))
            {
                status = STATUS_COOLDOWN;
                cooldownUntil = res.time + COOLDOWN_MS;
            }
        }

        // WebSocket-Broadcast bei allen wichtigen Status-Änderungen
        if (status != prevStatus)
        {
            Serial.printf("[STATUS] Status-Wechsel: %d -> %d\n", prevStatus, status);
            broadcastLichtschrankeStatus(status);
        }

        // Micro-Optimierung: Minimale Verzögerung bei kritischen Zuständen
        if (status == STATUS_TRIGGERED)
        {
            vTaskDelay(pdMS_TO_TICKS(1)); // Maximale Frequenz für Trigger-Erkennung
        }
        else if (status == STATUS_TRIGGERED_IN_COOLDOWN)
        {
            vTaskDelay(pdMS_TO_TICKS(2)); // Hohe Frequenz während Cooldown
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(5)); // Normal wenn inaktiv
        }

        // Cache periodisch aktualisieren
        static unsigned long lastCacheUpdate = 0;
        if (res.time - lastCacheUpdate > 10000) // Alle 10 Sekunden
        {
            Serial.println("[CACHE] Aktualisiere Role/Master-Cache...");
            cachedRole = getOwnRole();
            isMasterCached = isMaster();
            Serial.printf("[CACHE] Neue Werte: Rolle=%d, Master=%s\n", cachedRole, isMasterCached ? "JA" : "NEIN");
            lastCacheUpdate = res.time;
        }
    }
}

void initLichtschrankeTask()
{
    xTaskCreatePinnedToCore(
        lichtschrankeTask,
        "LichtschrankeTask",
        8192, // Mehr Stack für Optimierungen
        NULL,
        configMAX_PRIORITIES - 1, // Absolut höchste Priorität
        NULL,
        1); // Core 1 für beste Performance
}

LichtschrankeStatus getStatus()
{
    return status;
}