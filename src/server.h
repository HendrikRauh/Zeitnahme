#ifndef SERVER_H
#define SERVER_H
#include "AsyncWebSocket.h"
#include "WiFi.h"
#include <Arduino.h>
#include <AsyncTCP.h>
#include <data.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Sensor.h>
#include <data.h>
#include <task.h>
#include <timeLogic.h>
#include <Utility.h>
#include <espnow.h>

void wsBrodcastMessage(String message);

void broadcastLichtschrankeStatus(LichtschrankeStatus status);

void broadcastMasterStatus();

void broadcastLastTime(unsigned long lastTime);

void broadcastSavedDevices();

void broadcastDiscoveredDevices();

void initWebpage();

void initWebsocket();

#endif