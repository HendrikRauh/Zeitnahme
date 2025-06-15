#ifndef SERVER_H
#define SERVER_H
#include "AsyncWebSocket.h"
#include "WiFi.h"
#include <Arduino.h>
#include <AsyncTCP.h>
#include <data.h>
#include <ESPAsyncWebServer.h>
#include <Sensor.h>
#include <data.h>
#include <task.h>
#include <timeLogic.h>
#include <Utility.h>
#include <espnow.h>

void wsBrodcastMessage(String message);

void broadcastLichtschrankeStatus(LichtschrankeStatus status);

void broadcastTimeToClients(unsigned long time);

void initWebpage();

void initWebsocket();

String generateConfigPage();

String generateMainPage(unsigned long lastTime);

#endif