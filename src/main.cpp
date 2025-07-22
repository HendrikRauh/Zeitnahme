

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ota.h"

#include <ArduinoJson.h>
#include <deviceInfo.h>
#include <espnow.h>
#include <data.h>
#include <task.h>
#include <masterTask.h>
#include <Sensor.h>
#include <server.h>
#include <anzeige.h>

char macStr[18] = {0};

void setup()
{

  Serial.begin(115200);
  initDeviceInfo();
  initWebpage();
  initEspNow();
  initWebsocket();
  loadDeviceListFromPreferences();
  // TODO: Nur das verwenden was auch gebraucht wird (Anzeige oder Sensor + Tasks)
  initSensor();
  initLichtschrankeTask();
  // ----
  initMatrix();

  // Gestaffelte Initialisierung basierend auf MAC-Adresse (reduziert für bessere Performance)
  // Geräte mit niedrigerer MAC warten weniger
  const uint8_t *myMac = getMacAddress();
  uint32_t macSum = myMac[2] + myMac[3] + myMac[4] + myMac[5];
  uint32_t delayMs = (macSum % 3) * 500; // 0-1 Sekunden Verzögerung (reduziert)

  Serial.printf("[MASTER_DEBUG] Warte %lu ms vor Master-Bestimmung (MAC-basiert)\n", delayMs);
  delay(delayMs);

  // Master-System initialisieren
  determineMaster();

  // Master-Task starten
  initMasterTask();

  // Reduzierte Verzögerung für Netzwerk-Stabilisierung
  delay(1000); // Reduziert von 3000ms auf 1000ms

  // Zeit-Synchronisation starten (falls Slave)
  if (isSlave())
  {
    syncTimeWithMaster();
  }

  setupOTA();

  Serial.printf("[MASTER_DEBUG] Setup abgeschlossen - Status: %s\n", masterStatusToString(getMasterStatus()).c_str());
}

void loop()
{
  // Verhindert Busy-Waiting, gibt CPU-Zeit frei, verbessert Timing für Sensoren
  vTaskDelay(pdMS_TO_TICKS(1)); // 1 ms Pause, tickratenunabhängig
  matrixShowTime(millis());
}
