#include <Sensor.h>
#include <EasyUltrasonic.h>

constexpr uint8_t TRIG_PIN = 12;
constexpr uint8_t ECHO_PIN = 13;
constexpr float THRESHOLD_CM = 50.0f;

EasyUltrasonic ultrasonic;

float baseDistance;

void initSensor()
{
    ultrasonic.attach(TRIG_PIN, ECHO_PIN);
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
    res.triggered = (fabs(dist - baseDistance) >= THRESHOLD_CM);
    Serial.printf("%s | Dist: %.0f\n", res.triggered ? "🔴" : "🟢", dist);
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
