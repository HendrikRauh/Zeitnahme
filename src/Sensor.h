#ifndef SENSOR_H
#define SENSOR_H

#ifndef COOLDOWN_MS
#define COOLDOWN_MS 3000UL
#endif

#ifndef MAX_DISTANCE_CM
#define MAX_DISTANCE_CM 400.0f
#endif

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