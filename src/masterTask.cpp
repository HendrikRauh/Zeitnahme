#include <task.h>

void masterTask(void *pvParameters)
{
    unsigned long lastHeartbeat = 0;
    unsigned long lastMasterCheck = 0;
    unsigned long lastTimeSync = 0;
    unsigned long lastRaceCleanup = 0;
    unsigned long lastFullSync = 0;

    for (;;)
    {
        unsigned long now = millis();

        // Heartbeat senden (Master)
        if (isMaster() && (now - lastHeartbeat > 5000))
        {
            sendHeartbeat();
            lastHeartbeat = now;
        }

        // Full-Sync senden (Master) - alle 10 Sekunden
        if (isMaster() && (now - lastFullSync > 10000))
        {
            sendFullSync();
            lastFullSync = now;
        }

        // Master-Online-Check (Slave)
        if (now - lastMasterCheck > 10000)
        {
            checkMasterOnline();
            lastMasterCheck = now;
        }

        // Zeit-Synchronisation (Slave)
        if (isSlave() && (now - lastTimeSync > 30000))
        {
            syncTimeWithMaster();
            lastTimeSync = now;
        }

        // Race-Cleanup (Master) - entferne alte beendete Rennen
        if (isMaster() && (now - lastRaceCleanup > 60000))
        {
            cleanupFinishedRaces();
            lastRaceCleanup = now;
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 Sekunde warten
    }
}

void initMasterTask()
{
    xTaskCreatePinnedToCore(
        masterTask,
        "MasterTask",
        4096,
        NULL,
        1,
        NULL,
        0); // Auf Core 0 laufen lassen
}
