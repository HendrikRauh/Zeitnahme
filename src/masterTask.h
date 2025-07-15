#ifndef MASTER_TASK_H
#define MASTER_TASK_H

#include <Arduino.h>
#include <data.h>
#include <espnow.h>
#include <Utility.h>

extern TaskHandle_t masterTaskHandle;
void initMasterTask();
void cleanupFinishedRaces();

#endif
