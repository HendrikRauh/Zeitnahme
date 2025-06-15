#ifndef TASK_H
#define TASK_H

#include <server.h>
#include <Arduino.h>
#include <Sensor.h>
#include <timeLogic.h>

void initLichtschrankeTask();
LichtschrankeStatus getStatus();

#endif