
#include <ArduinoJson.h>
#include <deviceInfo.h>
#include <espnow.h>
#include <data.h>
#include <task.h>
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
}

void loop()
{
  // leer lassen! Die Hauptlogik läuft in lichtschrankeTask
}