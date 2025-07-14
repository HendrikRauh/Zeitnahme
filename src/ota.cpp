
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
        String typ = ArduinoOTA.getCommand() == U_FLASH ? "Programm" : "Dateisystem";
        Serial.println("[OTA] Update gestartet: " + typ);
        // Haupttask anhalten
        if (masterTaskHandle != NULL) {
            vTaskSuspend(masterTaskHandle);
            Serial.println("[OTA] MasterTask wurde pausiert.");
        } });
    ArduinoOTA.onEnd([]()
                     {
        Serial.println("[OTA] Update abgeschlossen");
        Serial.println("[OTA] Update erfolgreich - Neustart wird ausgeführt");
        delay(500);
        ESP.restart(); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          { if (total > 0) Serial.printf("[OTA] Fortschritt: %u%%\r", (progress * 100 / total)); });
    ArduinoOTA.onError([](ota_error_t error)
                       {
        Serial.printf("[OTA] Fehler[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Authentifizierung fehlgeschlagen");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Start fehlgeschlagen");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Verbindung fehlgeschlagen");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Empfang fehlgeschlagen");
        else if (error == OTA_END_ERROR) Serial.println("Abschluss fehlgeschlagen");
        Serial.println("[OTA] Fehler - Neustart wird ausgeführt");
        delay(500);
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
