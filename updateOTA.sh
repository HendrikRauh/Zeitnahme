
#!/usr/bin/env bash
set -euo pipefail

# Farben
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Status-Arrays
declare -gA STATUS_FW
declare -gA STATUS_FS
declare -gA DURATION_FW
declare -gA DURATION_FS
declare -gA RETRIES_FW
declare -gA RETRIES_FS
declare -ga ESP_SSIDS

# OTA-Retry für ein Gerät und Typ
retry_ota_update() {
    local ssid="$1"
    local type="$2" # "fw" oder "fs"
    local max_retries=2
    local try=1
    local success=0
    local clean_ssid_name=$(clean_ssid "$ssid")
    local ssid_hash=$(echo -n "$clean_ssid_name" | md5sum | awk '{print $1}')
    while [ $try -le $max_retries ]; do
        # Der erste Retry ist Versuch 2
        echo -e "${YELLOW}🔁 Wiederhole OTA-$type-Update für $clean_ssid_name (Versuch $((try+1))/$((max_retries+1)))...${NC}"
        connect_wifi "$clean_ssid_name"
        sleep 3
        if ip addr show "$WIFI_IFACE" | grep -q '192\.168\.4\.'; then
            ESP_IP="192.168.4.1"
            run_ota_update "$ESP_IP" "$clean_ssid_name" "$type"
            if [ "$type" = "fw" ] && [ "${STATUS_FW[$ssid_hash]}" = "🟢" ]; then success=1; break; fi
            if [ "$type" = "fs" ] && [ "${STATUS_FS[$ssid_hash]}" = "🟢" ]; then success=1; break; fi
        else
            echo -e "${RED}❌ Verbindung zu $clean_ssid_name fehlgeschlagen!${NC}"
        fi
        disconnect_wifi "$clean_ssid_name"
        sleep 2
        try=$((try+1))
    done
    # Track retries (Anzahl der Versuche insgesamt, also try)
    if [ "$type" = "fw" ]; then
        RETRIES_FW["$ssid_hash"]=$try
    elif [ "$type" = "fs" ]; then
        RETRIES_FS["$ssid_hash"]=$try
    fi
    if [ $success -eq 1 ]; then
        # Status nach erfolgreichem Retry auf grün setzen
        if [ "$type" = "fw" ]; then
            STATUS_FW["$ssid_hash"]="🟢"
        elif [ "$type" = "fs" ]; then
            STATUS_FS["$ssid_hash"]="🟢"
        fi
        echo -e "${GREEN}✅ OTA-$type-Update für $clean_ssid_name nach Wiederholung erfolgreich!${NC}"
    else
        echo -e "${RED}❌ OTA-$type-Update für $clean_ssid_name endgültig fehlgeschlagen!${NC}"
    fi
}

# OTA-Update durchführen
run_ota_update() {
    local ip="$1"
    local ssid="$2"
    local type="${3:-}"
    local fw_status=0
    local fs_status=0
    local clean_ssid_name=$(clean_ssid "$ssid")
    local ssid_hash=$(echo -n "$clean_ssid_name" | md5sum | awk '{print $1}')
    local start_fw end_fw start_fs end_fs
    local do_fw=0
    local do_fs=0
    if [ -n "$type" ]; then
        if [ "$type" = "fw" ]; then do_fw=1; fi
        if [ "$type" = "fs" ]; then do_fs=1; fi
    else
        if [ -z "${UPDATE_TYPE:-}" ] || [ "$UPDATE_TYPE" = "fw" ]; then do_fw=1; fi
        if [ -z "${UPDATE_TYPE:-}" ] || [ "$UPDATE_TYPE" = "fs" ]; then do_fs=1; fi
    fi
    if [ $do_fw -eq 1 ]; then
        echo -e "${YELLOW}⚙️  OTA Firmware-Update für $clean_ssid_name...${NC}"
        connect_wifi "$clean_ssid_name"
        sleep 3
        start_fw=$(date +%s.%N)
        set +e
        PLATFORMIO_UPLOAD_PROTOCOL=espota platformio run --target upload --upload-port "$ip"
        fw_status=$?
        set -e
        end_fw=$(date +%s.%N)
        STATUS_FW["$ssid_hash"]=$([ $fw_status -eq 0 ] && echo "🟢" || echo "🔴")
        DURATION_FW["$ssid_hash"]=$(awk "BEGIN {print sprintf(\"%.2f\", $end_fw - $start_fw)}")
        disconnect_wifi "$clean_ssid_name"
        sleep 2
    fi
    if [ $do_fs -eq 1 ]; then
        echo -e "${YELLOW}⚙️  OTA Filesystem-Update für $clean_ssid_name...${NC}"
        connect_wifi "$clean_ssid_name"
        sleep 3
        start_fs=$(date +%s.%N)
        set +e
        PLATFORMIO_UPLOAD_PROTOCOL=espota platformio run --target uploadfs --upload-port "$ip"
        fs_status=$?
        set -e
        end_fs=$(date +%s.%N)
        STATUS_FS["$ssid_hash"]=$([ $fs_status -eq 0 ] && echo "🟢" || echo "🔴")
        DURATION_FS["$ssid_hash"]=$(awk "BEGIN {print sprintf(\"%.2f\", $end_fs - $start_fs)}")
        disconnect_wifi "$clean_ssid_name"
        sleep 2
    fi
    if { [ $do_fw -eq 1 ] && [ $fw_status -eq 0 ]; } || { [ $do_fs -eq 1 ] && [ $fs_status -eq 0 ]; }; then
        echo -e "${GREEN}✅ OTA-Update für $clean_ssid_name erfolgreich!${NC}"
    else
        echo -e "${RED}❌ OTA-Update für $clean_ssid_name fehlgeschlagen!${NC}"
    fi
}

# Mit WLAN verbinden
connect_wifi() {
    local ssid="$1"
    if [ -n "${WIFI_PASS:-}" ]; then
        nmcli dev wifi connect "$ssid" password "$WIFI_PASS" ifname "$WIFI_IFACE" >/dev/null 2>&1
    else
        nmcli dev wifi connect "$ssid" ifname "$WIFI_IFACE" >/dev/null 2>&1
    fi
}


# WLAN trennen
disconnect_wifi() {
    nmcli con down id "$1" >/dev/null 2>&1 || true
}

# SSID bereinigen
clean_ssid() {
    local ssid="$1"
    ssid=$(echo "$ssid" | sed 's/\x1b\[[0-9;]*m//g')
    ssid="${ssid//\\:/:}"
    ssid="${ssid//\\\\/\\}"
    ssid=$(echo "$ssid" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
    echo "$ssid"
}

# Aktuelle WLAN-SSID sicher ermitteln
get_current_ssid() {
    local ssid=""
    ssid=$(nmcli -t -f active,ssid dev wifi | awk -F: '$1 ~ /^(yes|ja|si|oui|sí|да|はい|是|예)$/ {print $2}' | head -1)
    if [ -z "$ssid" ]; then
        ssid=$(nmcli -t -f DEVICE,TYPE,STATE,CONNECTION dev | awk -F: '$3=="connected" && $2=="wifi" {print $4}' | head -1)
    fi
    if [ -z "$ssid" ]; then
        ssid=$(nmcli -t -f NAME,TYPE,DEVICE con show --active | awk -F: '$2=="802-11-wireless" && $3!="" {print $1}' | head -1)
    fi
    ssid=$(clean_ssid "$ssid")
    if [[ "$ssid" =~ ⏱️ ]]; then
        echo ""
        return
    fi
    echo "$ssid"
}

# ESP-SSIDs finden
find_esp_ssids() {
    nmcli -t -f SSID dev wifi list | awk 'NF' | grep -F "⏱️" | while IFS= read -r ssid; do
        clean_ssid "$ssid"
    done
}

# Cleanup-Funktion für Lastwill (WLAN wiederherstellen und Ergebnis anzeigen)
__CLEANUP_CALLED=0
cleanup() {
    if [ "$__CLEANUP_CALLED" -eq 1 ]; then return; fi
    __CLEANUP_CALLED=1
    if [ "${#ESP_SSIDS[@]}" -gt 0 ]; then
        echo -e "\n${BLUE}=== OTA-Update-Prozess abgeschlossen ===${NC}"
        OTA_FAILED=0
        for ssid in "${ESP_SSIDS[@]}"; do
            clean_ssid_name=$(clean_ssid "$ssid")
            ssid_hash=$(echo -n "$clean_ssid_name" | md5sum | awk '{print $1}')
            echo -e "${YELLOW}Gerät: $clean_ssid_name${NC}"
            if [ -z "${UPDATE_TYPE:-}" ] || [ "$UPDATE_TYPE" = "fw" ]; then
                fw="${STATUS_FW[$ssid_hash]:-⚪}"
                dur_fw="${DURATION_FW[$ssid_hash]:-n/a}"
                retries_fw="${RETRIES_FW[$ssid_hash]:-0}"
                retry_str_fw=""
                if [ "$fw" = "🟢" ] && [ "$retries_fw" -gt 0 ]; then
                    retry_str_fw=" (nach $((retries_fw+1)) Versuchen)"
                elif [ "$fw" = "🔴" ] && [ "$retries_fw" -gt 0 ]; then
                    retry_str_fw=" (alle $((retries_fw+1)) Versuche fehlgeschlagen)"
                fi
                echo -e "  Firmware: $fw  (${dur_fw}s)${retry_str_fw}"
                if [ "$fw" = "🔴" ]; then OTA_FAILED=1; fi
            fi
            if [ -z "${UPDATE_TYPE:-}" ] || [ "$UPDATE_TYPE" = "fs" ]; then
                fs="${STATUS_FS[$ssid_hash]:-⚪}"
                dur_fs="${DURATION_FS[$ssid_hash]:-n/a}"
                retries_fs="${RETRIES_FS[$ssid_hash]:-0}"
                retry_str_fs=""
                if [ "$fs" = "🟢" ] && [ "$retries_fs" -gt 0 ]; then
                    retry_str_fs=" (nach $((retries_fs+1)) Versuchen)"
                elif [ "$fs" = "🔴" ] && [ "$retries_fs" -gt 0 ]; then
                    retry_str_fs=" (alle $((retries_fs+1)) Versuche fehlgeschlagen)"
                fi
                echo -e "  Filesystem: $fs  (${dur_fs}s)${retry_str_fs}"
                if [ "$fs" = "🔴" ]; then OTA_FAILED=1; fi
            fi
            echo
        done
    fi
    if [ -n "${PREV_SSID:-}" ]; then
        echo -e "${YELLOW}🔄 Versuche Rückverbindung zu: $PREV_SSID${NC}"
        if nmcli con up id "$PREV_SSID" >/dev/null 2>&1; then
            sleep 3
            if nmcli -t -f active,ssid dev wifi | awk -F: -v ssid="$PREV_SSID" '$1 ~ /^(yes|ja|si|oui|sí|да|はい|是|예)$/ && $2 == ssid { found=1 } END { exit !found }'; then
                echo -e "${GREEN}✅ Wieder verbunden mit $PREV_SSID${NC}"
            else
                echo -e "${RED}❌ Rückverbindung zu $PREV_SSID fehlgeschlagen!${NC}"
            fi
        else
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
    if [ "${OTA_FAILED:-0}" -ne 0 ]; then
        exit 1
    fi
}

# Hauptlogik
main() {
    PREV_SSID=$(get_current_ssid)
    trap 'SCRIPT_INTERRUPTED=1' SIGINT SIGTERM
    trap cleanup EXIT
    local info_msg=""
    if [ -n "${UPDATE_COUNT:-}" ] && [ "${UPDATE_COUNT:-0}" -gt 0 ]; then
        info_msg+="Nur die $UPDATE_COUNT stärksten Geräte werden geupdated. "
    fi
    if [ -n "${UPDATE_TYPE:-}" ]; then
        if [ "$UPDATE_TYPE" = "fw" ]; then
            info_msg+="Nur Firmware-Update. "
        elif [ "$UPDATE_TYPE" = "fs" ]; then
            info_msg+="Nur Filesystem-Update. "
        fi
    else
        info_msg+="Firmware und Filesystem werden aktualisiert. "
    fi
    if [ -n "$info_msg" ]; then
        echo -e "${YELLOW}ℹ️  Einschränkung: $info_msg${NC}\n"
    fi
    mapfile -t ESP_SSIDS < <(find_esp_ssids)
    if [ ${#ESP_SSIDS[@]} -eq 0 ]; then
        echo -e "${RED}❌ Keine passenden ESP-WLANs gefunden!${NC}"
        exit 1
    fi
    if [ -n "${UPDATE_COUNT:-}" ] && [ "$UPDATE_COUNT" -gt 0 ]; then
        ESP_SSIDS=("${ESP_SSIDS[@]:0:$UPDATE_COUNT}")
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
    declare -a retry_fw
    declare -a retry_fs
    for ssid in "${ESP_SSIDS[@]}"; do
        clean_ssid_name=$(clean_ssid "$ssid")
        ssid_hash=$(echo -n "$clean_ssid_name" | md5sum | awk '{print $1}')
        if [ -z "${UPDATE_TYPE:-}" ] || [ "$UPDATE_TYPE" = "fw" ]; then
            if [ "${STATUS_FW[$ssid_hash]}" = "🔴" ]; then
                retry_fw+=("$ssid")
            fi
        fi
        if [ -z "${UPDATE_TYPE:-}" ] || [ "$UPDATE_TYPE" = "fs" ]; then
            if [ "${STATUS_FS[$ssid_hash]}" = "🔴" ]; then
                retry_fs+=("$ssid")
            fi
        fi
    done
    for ssid in "${retry_fw[@]}"; do
        retry_ota_update "$ssid" "fw"
    done
    for ssid in "${retry_fs[@]}"; do
        retry_ota_update "$ssid" "fs"
    done
}

# Argumente auswerten
UPDATE_TYPE=""
UPDATE_COUNT=""
arg="${1:-}"
arg="${arg,,}"
if [ "$arg" = "-h" ] || [ "$arg" = "--help" ]; then
    echo "Usage: $0 [N][fw|fs]"
    echo "  fw     Only update firmware via OTA (alle Geräte)"
    echo "  fs     Only update filesystem via OTA (alle Geräte)"
    echo "  N      Update beide (fw+fs) für die N stärksten Geräte"
    echo "  Nfw    Nur Firmware für die N stärksten Geräte"
    echo "  fsN    Nur Filesystem für die N stärksten Geräte"
    echo "  (kein Argument) Update beide für alle Geräte"
    exit 0
elif [ -z "$arg" ]; then
    UPDATE_TYPE=""
    UPDATE_COUNT=""
elif [[ "$arg" =~ ^([0-9]+)(fw|fs)?$ ]]; then
    UPDATE_COUNT="${BASH_REMATCH[1]}"
    UPDATE_TYPE="${BASH_REMATCH[2]}"
elif [[ "$arg" =~ ^(fw|fs)([0-9]+)$ ]]; then
    UPDATE_TYPE="${BASH_REMATCH[1]}"
    UPDATE_COUNT="${BASH_REMATCH[2]}"
elif [ "$arg" = "fw" ] || [ "$arg" = "fs" ]; then
    UPDATE_TYPE="$arg"
    UPDATE_COUNT=""
else
    echo -e "${RED}❌ Invalid argument: '$1'${NC}"
    echo "Usage: $0 [N][fw|fs]"
    echo "  fw     Only update firmware via OTA (alle Geräte)"
    echo "  fs     Only update filesystem via OTA (alle Geräte)"
    echo "  N      Update beide (fw+fs) für die N stärksten Geräte"
    echo "  Nfw    Nur Firmware für die N stärksten Geräte"
    echo "  fsN    Nur Filesystem für die N stärksten Geräte"
    echo "  (kein Argument) Update beide für alle Geräte"
    exit 1
fi

# WLAN Interface bestimmen
WIFI_IFACE=$(nmcli -t -f DEVICE,TYPE,STATE dev | awk -F: '$2=="wifi" && $3=="connected"{print $1; exit}')
if [ -z "${WIFI_IFACE:-}" ]; then
    WIFI_IFACE=$(nmcli -t -f DEVICE,TYPE dev | awk -F: '$2=="wifi"{print $1; exit}')
fi
WIFI_PASS=""
main "$@"


