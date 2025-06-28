#include <Sensor.h>
#include <Arduino.h>
#include <data.h>

constexpr uint8_t TRIG_PIN = 12;
constexpr uint8_t ECHO_PIN = 13;

float baseDistance;
float cachedThreshold = 50.0f; // Cache für bessere Performance

// Einfache Ultraschall-Messung ohne externe Bibliothek
float getDistanceCM() {
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

    // Lade Threshold-Wert in Cache für bessere Performance
    cachedThreshold = getSensorThreshold();

    calibrateSensor();
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

    // res.triggered = (fabs(getDistanceCM() - baseDistance) >= THRESHOLD_CM);
    res.triggered = (fabs(dist - baseDistance) >= cachedThreshold);
    // Serial.printf("%s | Dist: %.0f\n", res.triggered ? "🔴" : "🟢", dist);
    return res;
}

float calibrateSensor()
{
    Serial.println("[SENSOR_DEBUG] Kalibriere Sensor...");
    baseDistance = getDistanceCM();
    if (baseDistance == 0)
    {
        baseDistance = MAX_DISTANCE_CM;
    }
    Serial.printf("[SENSOR_DEBUG] Kalibrierung abgeschlossen. Basisdistanz: %.2f cm\n", baseDistance);
    return baseDistance;
}

void updateThresholdCache()
{
    cachedThreshold = getSensorThreshold();
    Serial.printf("[SENSOR_DEBUG] Threshold-Cache aktualisiert auf: %.2f cm\n", cachedThreshold);
}

float getCurrentThreshold()
{
    return cachedThreshold;
}

float getBaseDistance()
{
    return baseDistance;
}

bool isBaseDistanceMaxRange()
{
    return baseDistance >= MAX_DISTANCE_CM;
}
