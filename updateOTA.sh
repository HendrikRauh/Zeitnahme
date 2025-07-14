#!/bin/bash

# OTA/USB Umschaltung: "espota" für OTA, "esptool" für USB, oder leer für Standard aus platformio.ini
UPLOAD_PROTOCOL="espota"

# Update-Typ: fw (Firmware), fs (Filesystem), leer = beides
UPDATE_TYPE=""

# Parameter auswerten
arg="${1,,}"
if [ "$arg" = "fw" ] || [ "$arg" = "fs" ]; then
    UPDATE_TYPE="$arg"
fi


# Farben definieren
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# WLAN Interface und Passwort (ggf. anpassen)
WIFI_IFACE="wlp0s20f3"
WIFI_PASS=""


# Funktion: ESP-SSIDs finden
find_esp_ssids() {
    nmcli -t -f SSID dev wifi list | awk 'NF' | grep -F "⏱️"
}

# Funktion: Mit WLAN verbinden
connect_wifi() {
    local ssid="$1"
    nmcli dev wifi connect "$ssid" password "$WIFI_PASS" ifname "$WIFI_IFACE" >/dev/null 2>&1
}

# Funktion: WLAN trennen
disconnect_wifi() {
    nmcli con down "$1" >/dev/null 2>&1 || true
}


# Status-Arrays für Übersicht
declare -A STATUS_FW
declare -A STATUS_FS



declare -A DURATION_FS

# Funktion: OTA-Update durchführen (je nach UPDATE_TYPE)
run_ota_update() {
    local ip="$1"
    local ssid="$2"
    local fw_status=0
    local fs_status=0
    local clean_ssid="${ssid//\\:/:}"
    # Hash für Array-Key erzeugen (nur Kleinbuchstaben und Zahlen)
    local ssid_hash
    ssid_hash=$(echo -n "$clean_ssid" | md5sum | awk '{print $1}')
    local start_fw end_fw start_fs end_fs
    if [ -z "$UPDATE_TYPE" ] || [ "$UPDATE_TYPE" = "fw" ]; then
        echo -e "${YELLOW}⚙️  OTA Firmware-Update für $ssid...${NC}"
        start_fw=$(date +%s.%N)
        PLATFORMIO_UPLOAD_PROTOCOL=espota platformio run --target upload --upload-port "$ip"
        fw_status=$?
        end_fw=$(date +%s.%N)
        STATUS_FW["$ssid_hash"]=$([ $fw_status -eq 0 ] && echo "🟢" || echo "🔴")
        DURATION_FW["$ssid_hash"]=$(awk "BEGIN {print sprintf(\"%.2f\", $end_fw - $start_fw)}")
    fi

    if [ -z "$UPDATE_TYPE" ] || [ "$UPDATE_TYPE" = "fs" ]; then
        echo -e "${YELLOW}⚙️  OTA Filesystem-Update für $ssid...${NC}"
        start_fs=$(date +%s.%N)
        PLATFORMIO_UPLOAD_PROTOCOL=espota platformio run --target uploadfs --upload-port "$ip"
        fs_status=$?
        end_fs=$(date +%s.%N)
        STATUS_FS["$ssid_hash"]=$([ $fs_status -eq 0 ] && echo "🟢" || echo "🔴")
        DURATION_FS["$ssid_hash"]=$(awk "BEGIN {print sprintf(\"%.2f\", $end_fs - $start_fs)}")
    fi

    # Erfolgsmeldung je nach Update-Typ
    if { [ "$UPDATE_TYPE" = "fw" ] && [ $fw_status -eq 0 ]; } || \
    { [ "$UPDATE_TYPE" = "fs" ] && [ $fs_status -eq 0 ]; } || \
    { [ -z "$UPDATE_TYPE" ] && [ $fw_status -eq 0 ] && [ $fs_status -eq 0 ]; }
    then
        echo -e "${GREEN}✅ OTA-Update für $ssid erfolgreich!${NC}"
    else
        echo -e "${RED}❌ OTA-Update für $ssid fehlgeschlagen!${NC}"
    fi
}


# Hauptlogik

main() {
    # Aktuell verbundenes WLAN merken (nur mit nmcli, sprachunabhängig)
    PREV_SSID=$(nmcli -t -f active,ssid dev wifi | grep -E '^(yes|ja):' | cut -d: -f2-)
    
    mapfile -t ESP_SSIDS < <(find_esp_ssids)
    if [ ${#ESP_SSIDS[@]} -eq 0 ]; then
        echo -e "${RED}❌ Keine passenden ESP-WLANs gefunden!${NC}"
        exit 1
    fi
    
    echo -e "\n${YELLOW}🚀 Starte OTA-Update für folgende Geräte:${NC}"
    for ssid in "${ESP_SSIDS[@]}"; do
        clean_ssid="${ssid//\\:/:}"
        echo -e "  ${BLUE}- $clean_ssid${NC}"
    done
    echo
    
    for ssid in "${ESP_SSIDS[@]}"; do
        clean_ssid="${ssid//\\:/:}"
        echo -e "${BLUE}=== Verbinde mit >$clean_ssid< ===${NC}"
        connect_wifi "$clean_ssid"
        sleep 3
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
    
    # Nach dem Update wieder mit vorherigem WLAN verbinden
    if [ -n "$PREV_SSID" ]; then
        # Rückverbindung mit vorherigem WLAN
        # Explizit das alte WLAN aktivieren (Profil bevorzugt)
        if nmcli con up id "$PREV_SSID" >/dev/null 2>&1; then
            sleep 3
            if nmcli -t -f active,ssid dev wifi | grep -E '^(yes|ja):' | grep -q ":$PREV_SSID$"; then
                echo -e "${GREEN}✅ Wieder verbunden mit $PREV_SSID${NC}"
            else
                echo -e "${RED}❌ Rückverbindung zu $PREV_SSID fehlgeschlagen!${NC}"
            fi
        else
            echo -e "${RED}❌ Konnte gespeichertes Profil für $PREV_SSID nicht aktivieren!${NC}"
        fi
    fi
    
    # Übersicht als Liste mit Dauer
    echo -e "\n${BLUE}=== OTA-Update-Prozess abgeschlossen ===${NC}"
    for ssid in "${ESP_SSIDS[@]}"; do
        clean_ssid="${ssid//\\:/:}"
        ssid_hash=$(echo -n "$clean_ssid" | md5sum | awk '{print $1}')
        echo -e "${YELLOW}Gerät: $clean_ssid${NC}"
        if [ -z "$UPDATE_TYPE" ] || [ "$UPDATE_TYPE" = "fw" ]; then
            fw="${STATUS_FW[$ssid_hash]:-⚪}"
            dur_fw="${DURATION_FW[$ssid_hash]:-n/a}"
            echo -e "  Firmware: $fw  (${dur_fw}s)"
        fi
        if [ -z "$UPDATE_TYPE" ] || [ "$UPDATE_TYPE" = "fs" ]; then
            fs="${STATUS_FS[$ssid_hash]:-⚪}"
            dur_fs="${DURATION_FS[$ssid_hash]:-n/a}"
            echo -e "  Filesystem: $fs  (${dur_fs}s)"
        fi
        echo
    done
}

main "$@"


