#include <Sensor.h>
#include <EasyUltrasonic.h>
#include <data.h>

constexpr uint8_t TRIG_PIN = 12;
constexpr uint8_t ECHO_PIN = 13;

EasyUltrasonic ultrasonic;

float baseDistance;
float cachedThreshold = 50.0f; // Cache für bessere Performance

void initSensor()
{
    ultrasonic.attach(TRIG_PIN, ECHO_PIN);

    // Lade Threshold-Wert in Cache für bessere Performance
    cachedThreshold = getSensorThreshold();

    calibrateSensor();
}

MeasureResult measure()
{
    MeasureResult res;
    res.time = millis();
    float dist = ultrasonic.getDistanceCM();
    if (dist == 0)
    {
        dist = 400;
    }

    // res.triggered = (fabs(ultrasonic.getDistanceCM() - baseDistance) >= THRESHOLD_CM);
    res.triggered = (fabs(dist - baseDistance) >= cachedThreshold);
    // Serial.printf("%s | Dist: %.0f\n", res.triggered ? "🔴" : "🟢", dist);
    return res;
}

float calibrateSensor()
{
    Serial.println("[SENSOR_DEBUG] Kalibriere Sensor...");
    baseDistance = ultrasonic.getDistanceCM();
    if (baseDistance == 0)
    {
        baseDistance = 400;
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
