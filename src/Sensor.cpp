#include <Sensor.h>
#include <Arduino.h>
#include <data.h>

constexpr uint8_t TRIG_PIN = 12;
constexpr uint8_t ECHO_PIN = 13;

int cachedMinDistance = 2;   // Cache für bessere Performance
int cachedMaxDistance = 100; // Cache für bessere Performance

// Einfache Ultraschall-Messung ohne externe Bibliothek - optimiert für Geschwindigkeit
float getDistanceCM()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Noch kürzerer Timeout für maximale Reaktionszeit
    long duration = pulseIn(ECHO_PIN, HIGH, 20000); // 20ms Timeout
    if (duration == 0)
        return MAX_DISTANCE_CM;

    // Optimierte Berechnung
    return (duration * 0.017); // Direkte Berechnung: duration * 0.034 / 2
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
    // Mikrosekunden-Genauigkeit für höchste Zeitpräzision
    unsigned long startMicros = micros();
    float dist = getDistanceCM();
    res.time = startMicros / 1000; // Konvertierung zu Millisekunden

    if (dist == 0)
    {
        dist = MAX_DISTANCE_CM;
    }

    // Optimierte Trigger-Erkennung
    res.triggered = (dist >= cachedMinDistance && dist <= cachedMaxDistance);

    // Debug nur bei Trigger-Ereignissen
    if (res.triggered)
    {
        Serial.printf("Sensor TRIGGER: %.1fcm (Min:%d Max:%d)\n", dist, cachedMinDistance, cachedMaxDistance);
    }

    return res;
}

void updateDistanceCache()
{
    // Verwende die Funktionen aus data.h für bessere Cache-Synchronisation
    cachedMinDistance = getMinDistance();
    cachedMaxDistance = getMaxDistance();
    Serial.printf("[SENSOR_CACHE] Cache aktualisiert: Min:%d Max:%d\n", cachedMinDistance, cachedMaxDistance);
}

int getCurrentMinDistance()
{
    return cachedMinDistance;
}

int getCurrentMaxDistance()
{
    return cachedMaxDistance;
}
