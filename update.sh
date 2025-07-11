#!/bin/bash

# Alle USB-Geräte finden, die wie ESP-Ports aussehen (z.B. ttyUSBx, ttyACMx)
# und nur eindeutige Pfade speichern
ESP_PORTS=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | sort -u)

# Prüfen, ob überhaupt ESP-Geräte gefunden wurden
if [ -z "$ESP_PORTS" ]; then
    echo "Keine ESP-Geräte (ttyUSBx oder ttyACMx) gefunden."
    exit 1
fi

echo "Folgende ESP-Geräte werden aktualisiert:"
for port in $ESP_PORTS; do
    echo "- $port"
done
echo "---"

# Zuerst einmalig den Build ausführen, ohne Upload
echo "Starte einmaligen Build-Prozess..."
platformio run
if [ $? -ne 0 ]; then
    echo "FEHLER: Build fehlgeschlagen. Das Skript wird beendet."
    exit 1 # Skript beenden, wenn der Build fehlschlägt
fi
echo "Build erfolgreich abgeschlossen."
echo "---"

# Iteriere über jeden gefundenen Port und führe die Upload-Befehle aus
for port in $ESP_PORTS; do
    echo "Starte Upload für Gerät an $port..."
    # Firmware-Upload
    echo "  Upload Firmware..."
    platformio run --target upload --upload-port "$port"
    if [ $? -ne 0 ]; then
        echo "Fehler beim Firmware-Upload für $port. Überspringe Filesystem-Upload für dieses Gerät."
        continue # Nächstes Gerät bearbeiten
    fi
    
    # Filesystem-Upload
    echo "  Upload Filesystem..."
    platformio run --target uploadfs --upload-port "$port"
    if [ $? -ne 0 ]; then
        echo "Fehler beim Filesystem-Upload für $port."
    fi
    echo "Upload für $port abgeschlossen."
    echo "---"
done

echo "Alle gefundenen ESP-Geräte wurden bearbeitet."