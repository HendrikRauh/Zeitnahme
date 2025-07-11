#include <task.h>

unsigned long lastScream = 0;
LichtschrankeStatus status = STATUS_NORMAL;
unsigned long lastTrigger = 0;
unsigned long cooldownUntil = 0;

void lichtschrankeTask(void *pvParameters)
{
    for (;;)
    {
        MeasureResult res = measure();
        LichtschrankeStatus prevStatus = status;

        if (res.triggered && status == STATUS_NORMAL && res.time >= cooldownUntil)
        {
            if (getOwnRole() == ROLE_START)
            {
                if (isMaster())
                {
                    masterAddRaceStart(res.time, getMacAddress(), res.time);
                }
                else
                {
                    slaveHandleRaceStart(res.time, getMacAddress(), res.time);
                }
            }
            else if (getOwnRole() == ROLE_ZIEL)
            {
                if (isMaster())
                {
                    masterFinishRace(res.time, getMacAddress(), res.time);
                }
                else
                {
                    slaveHandleRaceFinish(res.time, getMacAddress(), res.time);
                }
            }
            lastTrigger = res.time;
            status = STATUS_TRIGGERED;
            // Serial.printf("🔴 %lu\n", res.time);
        }
        else if (!res.triggered && (status == STATUS_TRIGGERED || status == STATUS_TRIGGERED_IN_COOLDOWN))
        {
            status = STATUS_COOLDOWN;
            cooldownUntil = res.time + COOLDOWN_MS;
            // Serial.printf("🟡 %lu\n", res.time);
        }
        else if (res.triggered && (status == STATUS_COOLDOWN || status == STATUS_TRIGGERED_IN_COOLDOWN))
        {
            status = STATUS_TRIGGERED_IN_COOLDOWN;
            cooldownUntil = res.time + COOLDOWN_MS;
            // Serial.printf("🟠 %lu\n", res.time);
        }
        else if (!res.triggered && (status == STATUS_COOLDOWN || status == STATUS_TRIGGERED_IN_COOLDOWN) && res.time >= cooldownUntil)
        {
            status = STATUS_NORMAL;
            // Serial.printf("🟢 %lu\n", res.time);
        }
        else if (status == STATUS_TRIGGERED && (res.time - lastTrigger > 500))
        {
            status = STATUS_COOLDOWN;
            cooldownUntil = res.time + COOLDOWN_MS;
            // Serial.printf("🟡* %lu\n", res.time);
        }

        if (status != prevStatus)
        {
            broadcastLichtschrankeStatus(status);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void initLichtschrankeTask()
{
    xTaskCreatePinnedToCore(
        lichtschrankeTask,
        "LichtschrankeTask",
        4096,
        NULL,
        1,
        NULL,
        1);
}

LichtschrankeStatus getStatus()
{
    return status;
}