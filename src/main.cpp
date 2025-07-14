
#include <ArduinoJson.h>
#include <deviceInfo.h>
#include <espnow.h>
#include <data.h>
#include <task.h>
#include <masterTask.h>
#include <Sensor.h>
#include <server.h>

#include <ArduinoOTA.h>
#include <Update.h>

char macStr[18] = {0};

void setup()
{
  Serial.begin(115200);
  initDeviceInfo();
  initWebpage();
  initEspNow();
  initSensor();
  loadDeviceListFromPreferences();
  initWebsocket();
  initLichtschrankeTask();

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

  Serial.printf("[MASTER_DEBUG] Setup abgeschlossen - Status: %s\n", masterStatusToString(getMasterStatus()).c_str());

  // OTA-Initialisierung
  ArduinoOTA.setHostname("zeitnahme");
  ArduinoOTA.onStart([]()
                     {
    String type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
    Serial.println("[OTA] Start updating " + type); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("[OTA] End"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
  ArduinoOTA.begin();
}

void loop()
{
  // Die Hauptlogik läuft jetzt in separaten Tasks
  // Minimale Verzögerung für bessere Performance
  ArduinoOTA.handle();
  delay(100);
}
