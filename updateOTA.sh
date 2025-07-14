# OTA/USB Umschaltung: "espota" für OTA, "esptool" für USB, oder leer für Standard aus platformio.ini
UPLOAD_PROTOCOL="espota"
#!/bin/bash

# Farben definieren
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m'





WIFI_IFACE="wlp0s20f3"
WIFI_PASS=""

find_esp_ssids() {
    nmcli -t -f SSID dev wifi list | awk 'NF' | grep -F "⏱️"
}
connect_wifi() {
    local ssid="$1"
    nmcli dev wifi connect "$ssid" password "$WIFI_PASS" ifname "$WIFI_IFACE"
}

# Funktion: WLAN trennen
disconnect_wifi() {
    nmcli con down "$1" 2>/dev/null || true
}

# Status-Arrays für Übersicht
declare -A STATUS_FW
declare -A STATUS_FS
declare -A STATUS_MAC

# Funktion: OTA-Update durchführen
run_ota_update() {
    local ip="$1"
    local ssid="$2"
    echo -e "${YELLOW}⚙️  OTA Firmware-Update für $ssid...${NC}"
    PLATFORMIO_UPLOAD_PROTOCOL=espota platformio run --target upload --upload-port "$ip"
    fw_status=$?
    STATUS_FW["$ssid"]=$([ $fw_status -eq 0 ] && echo "🟢" || echo "🔴")
    
    echo -e "${YELLOW}⚙️  OTA Filesystem-Update für $ssid...${NC}"
    PLATFORMIO_UPLOAD_PROTOCOL=espota platformio run --target uploadfs --upload-port "$ip"
    fs_status=$?
    STATUS_FS["$ssid"]=$([ $fs_status -eq 0 ] && echo "🟢" || echo "🔴")
    
    if [ $fw_status -eq 0 ] && [ $fs_status -eq 0 ]; then
        echo -e "${GREEN}✅ OTA-Update für $ssid erfolgreich!${NC}"
    else
        echo -e "${RED}❌ OTA-Update für $ssid fehlgeschlagen!${NC}"
    fi
}

# Hauptlogik
## Debug-Ausgaben entfernt
mapfile -t ESP_SSIDS < <(find_esp_ssids)
if [ ${#ESP_SSIDS[@]} -eq 0 ]; then
    echo -e "${RED}❌ Keine passenden ESP-WLANs gefunden!${NC}"
    exit 1
fi

for ssid in "${ESP_SSIDS[@]}"; do
    clean_ssid="${ssid//\\:/:}"
    echo -e "${BLUE}=== Verbinde mit >$clean_ssid< ===${NC}"
    connect_wifi "$clean_ssid"
    sleep 3
    # Prüfe, ob Interface eine IP im ESP-AP-Netz hat
    STATUS_MAC["$clean_ssid"]="$clean_ssid"
    if ip addr show "$WIFI_IFACE" | grep -q '192.168.4.'; then
        echo -e "${GREEN}✅ Verbunden mit $clean_ssid${NC}"
        ESP_IP="192.168.4.1"
        run_ota_update "$ESP_IP" "$clean_ssid"
    else
        echo -e "${RED}❌ Verbindung zu $clean_ssid fehlgeschlagen!${NC}"
    fi
    disconnect_wifi "$clean_ssid"
    sleep 2
    echo -e "${BLUE}---${NC}"
done


# Übersichtstabelle
echo -e "${BLUE}=== OTA-Update-Prozess abgeschlossen ===${NC}"
printf '\n┌────────────────────┬──────────┬──────────┐\n'
printf '│ %-18s │ %-8s │ %-8s │\n' "Gerät" "FW" "FS"
printf '├────────────────────┼──────────┼──────────┤\n'
for ssid in "${ESP_SSIDS[@]}"; do
    clean_ssid="${ssid//\\:/:}"
    fw="${STATUS_FW[$clean_ssid]:-⚪}"
    fs="${STATUS_FS[$clean_ssid]:-⚪}"
    printf '│ %-18s │ %-8s │ %-8s │\n' "$clean_ssid" "$fw" "$fs"
done
printf '└────────────────────┴──────────┴──────────┘\n'
