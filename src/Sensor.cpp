#include <Sensor.h>
#include <Arduino.h>
#include <data.h>

constexpr uint8_t TRIG_PIN = 12;
constexpr uint8_t ECHO_PIN = 13;

float cachedMinDistance = 2.0f;   // Cache für bessere Performance
float cachedMaxDistance = 100.0f; // Cache für bessere Performance

// Einfache Ultraschall-Messung ohne externe Bibliothek
float getDistanceCM()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH);
    float distance = (duration * 0.034) / 2; // Schallgeschwindigkeit: 340 m/s

    return distance > 0 ? distance : MAX_DISTANCE_CM;
}

void initSensor()
{
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // Lade Distanz-Werte in Cache für bessere Performance
    updateDistanceCache();
}

MeasureResult measure()
{
    MeasureResult res;
    res.time = millis();
    float dist = getDistanceCM();
    if (dist == 0)
    {
        dist = MAX_DISTANCE_CM;
    }

    // Auslösung wenn Distanz zwischen Min und Max liegt
    res.triggered = (dist >= cachedMinDistance && dist <= cachedMaxDistance);
    // Serial.printf("%s | Dist: %.0f (Min: %.0f, Max: %.0f)\n", res.triggered ? "🔴" : "🟢", dist, cachedMinDistance, cachedMaxDistance);
    return res;
}

void updateDistanceCache()
{
    cachedMinDistance = getMinDistance();
    cachedMaxDistance = getMaxDistance();
    Serial.printf("[SENSOR_DEBUG] Distanz-Cache aktualisiert - Min: %.2f cm, Max: %.2f cm\n", cachedMinDistance, cachedMaxDistance);
}

float getCurrentMinDistance()
{
    return cachedMinDistance;
}

float getCurrentMaxDistance()
{
    return cachedMaxDistance;
}
