
#include <ArduinoJson.h>
#include <deviceInfo.h>
#include <espnow.h>
#include <data.h>
#include <task.h>
#include <masterTask.h>
#include <Sensor.h>
#include <server.h>

char macStr[18] = {0};

void setup()
{
  Serial.begin(9600);
  initDeviceInfo();
  initWebpage();
  initEspNow();
  initSensor();
  loadDeviceListFromPreferences();
  initWebsocket();
  initLichtschrankeTask();

  // Gestaffelte Initialisierung basierend auf MAC-Adresse
  // Geräte mit niedrigerer MAC warten weniger
  const uint8_t *myMac = getMacAddress();
  uint32_t macSum = myMac[2] + myMac[3] + myMac[4] + myMac[5];
  uint32_t delayMs = (macSum % 5) * 1000; // 0-4 Sekunden Verzögerung

  Serial.printf("[MASTER_DEBUG] Warte %lu ms vor Master-Bestimmung (MAC-basiert)\n", delayMs);
  delay(delayMs);

  // Master-System initialisieren
  determineMaster();

  // Master-Task starten
  initMasterTask();

  // Weitere Verzögerung für Netzwerk-Stabilisierung
  delay(3000);

  // Zeit-Synchronisation starten (falls Slave)
  if (isSlave())
  {
    syncTimeWithMaster();
  }

  Serial.printf("[MASTER_DEBUG] Setup abgeschlossen - Status: %s\n", masterStatusToString(getMasterStatus()).c_str());
}

void loop()
{
  // Die Hauptlogik läuft jetzt in separaten Tasks
  // Hier können andere Wartungsaufgaben ausgeführt werden
  delay(1000);
}