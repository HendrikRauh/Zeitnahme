#ifndef SENSOR_H
#define SENSOR_H

constexpr unsigned long COOLDOWN_MS = 3000;

struct MeasureResult
{
    unsigned long time;
    bool triggered;
};

enum LichtschrankeStatus
{
    STATUS_NORMAL,
    STATUS_TRIGGERED,
    STATUS_COOLDOWN,
    STATUS_TRIGGERED_IN_COOLDOWN
};

float calibrateSensor();

void initSensor();

MeasureResult measure();

void updateThresholdCache();

float getCurrentThreshold();

float getBaseDistance();

#endif