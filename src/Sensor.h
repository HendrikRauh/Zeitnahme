#ifndef SENSOR_H
#define SENSOR_H

constexpr unsigned long COOLDOWN_MS = 3000;
constexpr float MAX_DISTANCE_CM = 400.0f; // Maximalwert des Ultraschallsensors

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

void initSensor();

MeasureResult measure();

void updateDistanceCache();

int getCurrentMinDistance();

int getCurrentMaxDistance();

#endif