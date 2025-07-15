#!/bin/bash
set -euo pipefail


# Update-Typ: fw (Firmware), fs (Filesystem), leer = beides
UPDATE_TYPE=""



# Farben definieren
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Parameter auswerten (robust gegen nicht gesetztes $1)
arg="${1:-}"
arg="${arg,,}"
if [ "$arg" = "-h" ] || [ "$arg" = "--help" ]; then
    echo "Usage: $0 [fw|fs]"
    echo "  fw   Only update firmware via OTA"
    echo "  fs   Only update filesystem via OTA"
    echo "  (no argument) Update both firmware and filesystem"
    exit 0
elif [ -z "$arg" ]; then
    UPDATE_TYPE=""
elif [ "$arg" = "fw" ] || [ "$arg" = "fs" ]; then
    UPDATE_TYPE="$arg"
else
    echo -e "${RED}❌ Invalid argument: '$1'${NC}"
    echo "Usage: $0 [fw|fs]"
    echo "  fw   Only update firmware via OTA"
    echo "  fs   Only update filesystem via OTA"
    echo "  (no argument) Update both firmware and filesystem"
    exit 1
fi

# WLAN Interface dynamisch bestimmen
WIFI_IFACE=$(nmcli -t -f DEVICE,TYPE,STATE dev | awk -F: '$2=="wifi" && $3=="connected"{print $1; exit}')
if [ -z "$WIFI_IFACE" ]; then
    WIFI_IFACE=$(nmcli -t -f DEVICE,TYPE dev | awk -F: '$2=="wifi"{print $1; exit}')
fi
WIFI_PASS=""


# Funktion: ESP-SSIDs finden
find_esp_ssids() {
    nmcli -t -f SSID dev wifi list | awk 'NF' | grep -F "⏱️" | while IFS= read -r ssid; do
        clean_ssid "$ssid"
    done
}

# Funktion: SSID bereinigen (ANSI-Codes und Escape-Sequenzen entfernen)
clean_ssid() {
    local ssid="$1"
    
    # ANSI Escape-Sequenzen entfernen
    ssid=$(echo "$ssid" | sed 's/\x1b\[[0-9;]*m//g')
    
    # Andere Escape-Sequenzen normalisieren
    ssid="${ssid//\\:/:}"
    ssid="${ssid//\\\\/\\}"
    
    # Trailing/Leading Whitespace entfernen
    ssid=$(echo "$ssid" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
    
    echo "$ssid"
}

# Funktion: Aktuelle WLAN-SSID sicher ermitteln
get_current_ssid() {
    local ssid=""
    
    # Methode 1: Über aktive WiFi-Verbindung (robuster)
    ssid=$(nmcli -t -f active,ssid dev wifi | awk -F: '$1 ~ /^(yes|ja|si|oui|sí|да|はい|是|예)$/ {print $2}' | head -1)
    
    # Methode 2: Über Geräte-Status (Fallback)
    if [ -z "$ssid" ]; then
        ssid=$(nmcli -t -f DEVICE,TYPE,STATE,CONNECTION dev | awk -F: '$3=="connected" && $2=="wifi" {print $4}' | head -1)
    fi
    
    # Methode 3: Über connection show (zweiter Fallback)
    if [ -z "$ssid" ]; then
        ssid=$(nmcli -t -f NAME,TYPE,DEVICE con show --active | awk -F: '$2=="802-11-wireless" && $3!="" {print $1}' | head -1)
    fi
    
    # SSID bereinigen
    ssid=$(clean_ssid "$ssid")
    
    # Prüfen ob es sich um ein ESP-Gerät handelt (diese nicht als Original-SSID verwenden)
    if [[ "$ssid" =~ ⏱️ ]]; then
        echo ""  # Leere SSID zurückgeben wenn es ein ESP-Gerät ist
        return
    fi
    
    echo "$ssid"
}

# Funktion: Mit WLAN verbinden
connect_wifi() {
    local ssid="$1"
    if [ -n "$WIFI_PASS" ]; then
        nmcli dev wifi connect "$ssid" password "$WIFI_PASS" ifname "$WIFI_IFACE" >/dev/null 2>&1
    else
        nmcli dev wifi connect "$ssid" ifname "$WIFI_IFACE" >/dev/null 2>&1
    fi
}

# Funktion: WLAN trennen
disconnect_wifi() {
    nmcli con down id "$1" >/dev/null 2>&1 || true
}


# Status-Arrays für Übersicht



declare -A STATUS_FW
declare -A STATUS_FS
declare -A DURATION_FW
declare -A DURATION_FS

# Funktion: OTA-Update durchführen (je nach UPDATE_TYPE)
run_ota_update() {
    local ip="$1"
    local ssid="$2"
    local fw_status=0
    local fs_status=0
    local clean_ssid_name=$(clean_ssid "$ssid")
    # Hash für Array-Key erzeugen (nur Kleinbuchstaben und Zahlen)
    local ssid_hash
    ssid_hash=$(echo -n "$clean_ssid_name" | md5sum | awk '{print $1}')
    local start_fw end_fw start_fs end_fs
    if [ -z "$UPDATE_TYPE" ] || [ "$UPDATE_TYPE" = "fw" ]; then
        echo -e "${YELLOW}⚙️  OTA Firmware-Update für $clean_ssid_name...${NC}"
        start_fw=$(date +%s.%N)
        set +e  # Temporär Fehler-Abbruch deaktivieren
        PLATFORMIO_UPLOAD_PROTOCOL=espota platformio run --target upload --upload-port "$ip"
        fw_status=$?
        set -e  # Fehler-Abbruch wieder aktivieren
        end_fw=$(date +%s.%N)
        STATUS_FW["$ssid_hash"]=$([ $fw_status -eq 0 ] && echo "🟢" || echo "🔴")
        DURATION_FW["$ssid_hash"]=$(awk "BEGIN {print sprintf(\"%.2f\", $end_fw - $start_fw)}")
    fi

    if [ -z "$UPDATE_TYPE" ] || [ "$UPDATE_TYPE" = "fs" ]; then
        echo -e "${YELLOW}⚙️  OTA Filesystem-Update für $clean_ssid_name...${NC}"
        start_fs=$(date +%s.%N)
        set +e  # Temporär Fehler-Abbruch deaktivieren
        PLATFORMIO_UPLOAD_PROTOCOL=espota platformio run --target uploadfs --upload-port "$ip"
        fs_status=$?
        set -e  # Fehler-Abbruch wieder aktivieren
        end_fs=$(date +%s.%N)
        STATUS_FS["$ssid_hash"]=$([ $fs_status -eq 0 ] && echo "🟢" || echo "🔴")
        DURATION_FS["$ssid_hash"]=$(awk "BEGIN {print sprintf(\"%.2f\", $end_fs - $start_fs)}")
    fi

    # Erfolgsmeldung je nach Update-Typ
    if { [ "$UPDATE_TYPE" = "fw" ] && [ $fw_status -eq 0 ]; } || \
    { [ "$UPDATE_TYPE" = "fs" ] && [ $fs_status -eq 0 ]; } || \
    { [ -z "$UPDATE_TYPE" ] && [ $fw_status -eq 0 ] && [ $fs_status -eq 0 ]; }
    then
        echo -e "${GREEN}✅ OTA-Update für $clean_ssid_name erfolgreich!${NC}"
    else
        echo -e "${RED}❌ OTA-Update für $clean_ssid_name fehlgeschlagen!${NC}"
    fi
}


# Hauptlogik


# Cleanup-Funktion für Lastwill (WLAN wiederherstellen und Nachricht ausgeben)
__CLEANUP_CALLED=0
cleanup() {
    if [ "$__CLEANUP_CALLED" -eq 1 ]; then return; fi
    __CLEANUP_CALLED=1
    
    # Status-Übersicht anzeigen (falls ESP_SSIDS bereits gesetzt)
    if [ "${#ESP_SSIDS[@]}" -gt 0 ]; then
        echo -e "\n${BLUE}=== OTA-Update-Prozess abgeschlossen ===${NC}"
        OTA_FAILED=0
        for ssid in "${ESP_SSIDS[@]}"; do
            clean_ssid_name=$(clean_ssid "$ssid")
            ssid_hash=$(echo -n "$clean_ssid_name" | md5sum | awk '{print $1}')
            echo -e "${YELLOW}Gerät: $clean_ssid_name${NC}"
            if [ -z "$UPDATE_TYPE" ] || [ "$UPDATE_TYPE" = "fw" ]; then
                fw="${STATUS_FW[$ssid_hash]:-⚪}"
                dur_fw="${DURATION_FW[$ssid_hash]:-n/a}"
                echo -e "  Firmware: $fw  (${dur_fw}s)"
                if [ "$fw" = "🔴" ]; then OTA_FAILED=1; fi
            fi
            if [ -z "$UPDATE_TYPE" ] || [ "$UPDATE_TYPE" = "fs" ]; then
                fs="${STATUS_FS[$ssid_hash]:-⚪}"
                dur_fs="${DURATION_FS[$ssid_hash]:-n/a}"
                echo -e "  Filesystem: $fs  (${dur_fs}s)"
                if [ "$fs" = "🔴" ]; then OTA_FAILED=1; fi
            fi
            echo
        done
    fi
    
    # WLAN-Wiederverbindung
    if [ -n "$PREV_SSID" ]; then
        echo -e "${YELLOW}🔄 Versuche Rückverbindung zu: $PREV_SSID${NC}"
        
        # Erst versuchen, über gespeichertes Profil zu verbinden
        if nmcli con up id "$PREV_SSID" >/dev/null 2>&1; then
            sleep 3
            if nmcli -t -f active,ssid dev wifi | awk -F: -v ssid="$PREV_SSID" '$1 ~ /^(yes|ja|si|oui|sí|да|はい|是|예)$/ && $2 == ssid { found=1 } END { exit !found }'; then
                echo -e "${GREEN}✅ Wieder verbunden mit $PREV_SSID${NC}"
            else
                echo -e "${RED}❌ Rückverbindung zu $PREV_SSID fehlgeschlagen!${NC}"
            fi
        else
            # Falls Profil-Verbindung fehlschlägt, versuche direkte Verbindung
            echo -e "${YELLOW}⚠️  Gespeichertes Profil nicht verfügbar, versuche direkte Verbindung...${NC}"
            if nmcli dev wifi connect "$PREV_SSID" ifname "$WIFI_IFACE" >/dev/null 2>&1; then
                sleep 3
                if nmcli -t -f active,ssid dev wifi | awk -F: -v ssid="$PREV_SSID" '$1 ~ /^(yes|ja|si|oui|sí|да|はい|是|예)$/ && $2 == ssid { found=1 } END { exit !found }'; then
                    echo -e "${GREEN}✅ Wieder verbunden mit $PREV_SSID${NC}"
                else
                    echo -e "${RED}❌ Rückverbindung zu $PREV_SSID fehlgeschlagen!${NC}"
                fi
            else
                echo -e "${RED}❌ Konnte keine Verbindung zu $PREV_SSID herstellen!${NC}"
            fi
        fi
    fi
    
    # Nur bei unerwarteten Stops (nicht bei normalem Ende)
    if [ "${SCRIPT_INTERRUPTED:-0}" -eq 1 ]; then
        echo -e "${RED}⚠️  Script wurde unerwartet gestoppt!${NC}"
    fi
    
    # Exit mit Fehlercode wenn Updates fehlgeschlagen sind
    if [ "${OTA_FAILED:-0}" -ne 0 ]; then
        exit 1
    fi
}

main() {
    # Aktuell verbundenes WLAN merken (robuste Methode)
    PREV_SSID=$(get_current_ssid)
    
    # Trap für Lastwill setzen (SIGINT, SIGTERM, EXIT)
    trap 'SCRIPT_INTERRUPTED=1' SIGINT SIGTERM
    trap cleanup EXIT
    
    mapfile -t ESP_SSIDS < <(find_esp_ssids)
    if [ ${#ESP_SSIDS[@]} -eq 0 ]; then
        echo -e "${RED}❌ Keine passenden ESP-WLANs gefunden!${NC}"
        exit 1
    fi
    
    echo -e "\n${YELLOW}🚀 Starte OTA-Update für folgende Geräte:${NC}"
    for ssid in "${ESP_SSIDS[@]}"; do
        clean_ssid_name=$(clean_ssid "$ssid")
        echo -e "  ${BLUE}- $clean_ssid_name${NC}"
    done
    echo
    
    for ssid in "${ESP_SSIDS[@]}"; do
        clean_ssid_name=$(clean_ssid "$ssid")
        echo -e "${BLUE}=== Verbinde mit >$clean_ssid_name< ===${NC}"
        connect_wifi "$clean_ssid_name"
        sleep 3
        if ip addr show "$WIFI_IFACE" | grep -q '192\.168\.4\.'; then
            echo -e "${GREEN}✅ Verbunden mit $clean_ssid_name${NC}"
            ESP_IP="192.168.4.1"
            run_ota_update "$ESP_IP" "$clean_ssid_name"
        else
            echo -e "${RED}❌ Verbindung zu $clean_ssid_name fehlgeschlagen!${NC}"
        fi
        disconnect_wifi "$clean_ssid_name"
        sleep 2
        echo -e "${BLUE}---${NC}"
    done
}

main "$@"


