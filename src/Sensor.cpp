#include <Sensor.h>
#include <EasyUltrasonic.h>

constexpr uint8_t TRIG_PIN = 12;
constexpr uint8_t ECHO_PIN = 13;
constexpr float THRESHOLD_CM = 10.0f;

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
    res.triggered = (fabs(ultrasonic.getDistanceCM() - baseDistance) >= THRESHOLD_CM);
    return res;
}

float calibrateSensor()
{
    Serial.println("[SENSOR_DEBUG] Kalibriere Sensor...");
    delay(1000);
    baseDistance = ultrasonic.getDistanceCM();
    Serial.printf("[SENSOR_DEBUG] Kalibrierung abgeschlossen. Basisdistanz: %.2f cm\n", baseDistance);
    return baseDistance;
}
