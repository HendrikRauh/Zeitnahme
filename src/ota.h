#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ArduinoOTA.h>
#include <Update.h>

// Initialisiert OTA und startet die OTA FreeRTOS-Task
void setupOTA();
