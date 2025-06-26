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
    // res.triggered = (fabs(ultrasonic.getDistanceCM() - baseDistance) >= THRESHOLD_CM);
    res.triggered = (fabs(dist - baseDistance) >= THRESHOLD_CM);
    Serial.printf("Dist: %f, %d\n", dist, res.triggered);
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
