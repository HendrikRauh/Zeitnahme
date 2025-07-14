#!/bin/bash

# OTA/USB Umschaltung: "espota" für OTA, "esptool" für USB, oder leer für Standard aus platformio.ini
UPLOAD_PROTOCOL="espota"

# Update-Typ: fw (Firmware), fs (Filesystem), leer = beides
UPDATE_TYPE=""

# Parameter auswerten
if [ "$1" = "fw" ] || [ "$1" = "FS" ] || [ "$1" = "fs" ] || [ "$1" = "FW" ]; then
    UPDATE_TYPE="${1,,}"
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
declare -A STATUS_MAC



# Funktion: OTA-Update durchführen (je nach UPDATE_TYPE)
run_ota_update() {
    local ip="$1"
    local ssid="$2"
    local fw_status=0
    local fs_status=0
    
    if [ -z "$UPDATE_TYPE" ] || [ "$UPDATE_TYPE" = "fw" ]; then
        echo -e "${YELLOW}⚙️  OTA Firmware-Update für $ssid...${NC}"
        PLATFORMIO_UPLOAD_PROTOCOL=espota platformio run --target upload --upload-port "$ip"
        fw_status=$?
        STATUS_FW["$ssid"]=$([ $fw_status -eq 0 ] && echo "🟢" || echo "🔴")
    fi
    
    if [ -z "$UPDATE_TYPE" ] || [ "$UPDATE_TYPE" = "fs" ]; then
        echo -e "${YELLOW}⚙️  OTA Filesystem-Update für $ssid...${NC}"
        PLATFORMIO_UPLOAD_PROTOCOL=espota platformio run --target uploadfs --upload-port "$ip"
        fs_status=$?
        STATUS_FS["$ssid"]=$([ $fs_status -eq 0 ] && echo "🟢" || echo "🔴")
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
    
    # Dynamische Übersichtstabelle
    echo -e "${BLUE}=== OTA-Update-Prozess abgeschlossen ===${NC}"
    # Feste Display-Width für Gerätenamen: Emoji + Leer + 8 Zeichen = 11
    maxlen=11
    # Tabellen-Borders dynamisch generieren
    border_top="┌$(printf '─%.0s' $(seq 1 $((maxlen+1))))┬────┬────┐"
    border_mid="├$(printf '─%.0s' $(seq 1 $((maxlen+1))))┼────┼────┤"
    border_bot="└$(printf '─%.0s' $(seq 1 $((maxlen+1))))┴────┴────┘"
    # Tabelle ausgeben
    printf '\n%s\n' "$border_top"
    printf "│ %-${maxlen}s │ %-2s │ %-2s │\n" "Gerät" "F" "S"
    printf '%s\n' "$border_mid"
    count=${#ESP_SSIDS[@]}
    for i in "${!ESP_SSIDS[@]}"; do
        ssid="${ESP_SSIDS[$i]}"
        clean_ssid="${ssid//\\:/:}"
        fw="${STATUS_FW[$clean_ssid]:-⚪}"
        fs="${STATUS_FS[$clean_ssid]:-⚪}"
        printf "│ %-${maxlen}s │ %-2s │ %-2s │\n" "$clean_ssid" "$fw" "$fs"
        # Horizontale Linie außer nach dem letzten Gerät
        if [ $i -lt $((count-1)) ]; then
            printf '%s\n' "$border_mid"
        fi
    done
    printf '%s\n' "$border_bot"
}

main "$@"


