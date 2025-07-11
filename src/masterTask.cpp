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

        // Heartbeat senden (Master) - reduziert für weniger Störungen
        if (isMaster() && (now - lastHeartbeat > 10000)) // 10 Sekunden statt 5
        {
            sendHeartbeat();
            lastHeartbeat = now;
        }

        // Full-Sync senden (Master) - reduziert für weniger Störungen
        if (isMaster() && (now - lastFullSync > 15000)) // 15 Sekunden statt 10
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

        // Zeit-Synchronisation (Slave) - reduziert für weniger Störungen
        if (isSlave() && (now - lastTimeSync > 60000)) // 60 Sekunden statt 30
        {
            syncTimeWithMaster();
            lastTimeSync = now;
        }

        // Race-Cleanup (Master) - entferne alte beendete Rennen
        if (isMaster() && (now - lastRaceCleanup > 10000)) // Reduziert von 60000 auf 10000 (10 Sekunden)
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
