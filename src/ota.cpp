
#include "ota.h"
#include <Arduino.h>
#include "masterTask.h"

// OTA-Task-Funktion für FreeRTOS
static void otaTask(void *param)
{
    for (;;)
    {
        ArduinoOTA.handle();
        vTaskDelay(pdMS_TO_TICKS(10)); // alle 10ms
    }
}

void setupOTA()
{
    ArduinoOTA.setHostname("zeitnahme");
    ArduinoOTA.onStart([]()
                       {
        // Haupttask anhalten
        if (masterTaskHandle != NULL) {
            vTaskSuspend(masterTaskHandle);
        } });
    ArduinoOTA.onEnd([]()
                     {
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP.restart(); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { /* no log output */ });
    ArduinoOTA.onError([](ota_error_t error)
                       {
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP.restart(); });
    ArduinoOTA.begin();

    // OTA-Task für FreeRTOS anlegen (nach OTA-Initialisierung!)
    xTaskCreate(
        otaTask,    // Task-Funktion
        "OTA_Task", // Name
        4096,       // Stackgröße
        nullptr,    // Parameter
        1,          // Priorität
        nullptr     // Handle
    );
}
