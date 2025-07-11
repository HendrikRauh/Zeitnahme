#!/bin/bash

# Farben definieren
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Funktion für Ladeanimation (Braille-Punkte)
spinner() {
    local pid=$1
    local delay=0.1
    # Unicode Braille-Muster für eine Kreisanimation
    local spinstr=("⠋" "⠙" "⠹" "⠸" "⠼" "⠴" "⠦" "⠧" "⠇" "⠏")
    local i=0
    local num_frames=${#spinstr[@]}
    
    # ANSI-Escape-Sequenz, um den Cursor zu verstecken
    tput civis
    
    while [ "$(ps a | awk '{print $1}' | grep $pid)" ]; do
        printf " ${YELLOW}[%s]${NC}" "${spinstr[$i]}"
        i=$(( (i + 1) % num_frames ))
        sleep $delay
        printf "\b\b\b\b\b" # Cursor zurücksetzen (5 Zeichen: Leerzeichen, [, Zeichen, ], Leerzeichen)
    done
    
    # ANSI-Escape-Sequenz, um den Cursor wieder anzuzeigen
    tput cnorm
    printf "      \b\b\b\b\b\b" # Spinner entfernen und Platz löschen
}

# Wrapper-Funktion für platformio, um Farben und Spinner zu erhalten
run_platformio() {
    local cmd=("$@")
    local output_file=$(mktemp)
    
    # Führe den Befehl im Hintergrund aus und leite die Ausgabe um
    "${cmd[@]}" &> "$output_file" &
    local pid=$!
    spinner $pid # Starte den Spinner mit der PID des Hintergrundprozesses
    wait $pid    # Warte auf den Hintergrundprozess
    local status=$?
    
    cat "$output_file" # Zeigt den kompletten Output an, inkl. Farben
    rm "$output_file"
    return $status
}

# Alle USB-Geräte finden, die wie ESP-Ports aussehen (z.B. ttyUSBx, ttyACMx)
# und nur eindeutige Pfade speichern
ESP_PORTS=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | sort -u)

# Standardmäßig beides aktualisieren
UPDATE_FIRMWARE=true
UPDATE_FILESYSTEM=true

# Array für die Ergebnisübersicht
declare -A RESULTS_FW
declare -A RESULTS_FS
declare -A RESULTS_MAC

# Prüfen, ob ein Parameter übergeben wurde
if [ ! -z "$1" ]; then
    case "$1" in
        fw)
            UPDATE_FILESYSTEM=false
            echo -e "${YELLOW}ℹ️ Es wird nur die Firmware aktualisiert.${NC}"
        ;;
        fs)
            UPDATE_FIRMWARE=false
            echo -e "${YELLOW}ℹ️ Es wird nur das Filesystem aktualisiert.${NC}"
        ;;
        *)
            echo -e "${RED}❌ Ungültiger Parameter: $1${NC}"
            echo -e "${YELLOW}Verwendung: $0 [fw | fs]${NC}"
            exit 1
        ;;
    esac
fi

# Prüfen, ob überhaupt ESP-Geräte gefunden wurden
if [ -z "$ESP_PORTS" ]; then
    echo -e "${RED}❌ Keine ESP-Geräte (ttyUSBx oder ttyACMx) gefunden.${NC}"
    exit 1
fi

echo ""
echo -e "${BLUE}=== Starte ESP-Update-Skript ===${NC}"
echo "---"
echo -e "${YELLOW}Gefundene ESP-Geräte:${NC}"
for port in $ESP_PORTS; do
    echo -e "  ➡️ ${port}"
    # Initialisiere Ergebnisse für jeden Port basierend auf den Update-Flags
    if $UPDATE_FIRMWARE; then
        RESULTS_FW["$port"]="wird_versucht"
    else
        RESULTS_FW["$port"]="unverändert"
    fi
    
    if $UPDATE_FILESYSTEM; then
        RESULTS_FS["$port"]="wird_versucht"
    else
        RESULTS_FS["$port"]="unverändert"
    fi
    RESULTS_MAC["$port"]="??:??:??:??:??:??" # Standardwert, falls nicht gefunden
done
echo "---"

# Zuerst einmalig den Build ausführen, ohne Upload
echo -e "${YELLOW}⚙️ Starte einmaligen Build-Prozess für PlatformIO...${NC}"
run_platformio platformio run
BUILD_STATUS=$?

if [ $BUILD_STATUS -ne 0 ]; then
    echo -e "${RED}❌ FEHLER: Build fehlgeschlagen. Überprüfe den obigen Log.${NC}"
    exit 1
fi
echo -e "${GREEN}✅ Build erfolgreich abgeschlossen.${NC}"
echo "---"

# Iteriere über jeden gefundenen Port und führe die Upload-Befehle aus
for port in $ESP_PORTS; do
    echo ""
    echo -e "${BLUE}=== Bearbeite Gerät an ${port} ===${NC}"
    
    # Firmware-Upload (optional)
    if $UPDATE_FIRMWARE; then
        echo -e "${YELLOW}⬆️ Starte Firmware-Upload für ${port}...${NC}"
        UPLOAD_FW_OUTPUT=$(run_platformio platformio run --target upload --upload-port "$port")
        UPLOAD_FW_STATUS=$?
        
        MAC_FROM_LOG=$(echo "$UPLOAD_FW_OUTPUT" | grep -Eo 'MAC: ([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}' | awk '{print $2}' | head -n 1 | tr '[:lower:]' '[:upper:]')
        if [ ! -z "$MAC_FROM_LOG" ]; then
            RESULTS_MAC["$port"]="$MAC_FROM_LOG"
        fi
        
        if [ $UPLOAD_FW_STATUS -ne 0 ]; then
            echo -e "${RED}❌ Fehler beim Firmware-Upload für $port.${NC}"
            RESULTS_FW["$port"]="failed"
            RESULTS_FS["$port"]="unverändert" # Wenn FW fehlschlägt, Filesystem nicht versuchen
            echo -e "${RED}Überprüfe den obigen Log für Details.${NC}"
            echo "---"
            continue
        else
            echo -e "${GREEN}✅ Firmware-Upload für $port erfolgreich.${NC}"
            RESULTS_FW["$port"]="erfolgreich"
        fi
    fi
    
    # Filesystem-Upload (optional)
    if $UPDATE_FILESYSTEM; then
        echo -e "${YELLOW}⬆️ Starte Filesystem-Upload für ${port}...${NC}"
        UPLOAD_FS_OUTPUT=$(run_platformio platformio run --target uploadfs --upload-port "$port")
        UPLOAD_FS_STATUS=$?
        
        if [ $UPLOAD_FS_STATUS -ne 0 ]; then
            echo -e "${RED}❌ Fehler beim Filesystem-Upload für $port.${NC}"
            RESULTS_FS["$port"]="failed"
            echo -e "${RED}Überprüfe den obigen Log für Details.${NC}"
        else
            echo -e "${GREEN}✅ Filesystem-Upload für $port erfolgreich.${NC}"
            RESULTS_FS["$port"]="erfolgreich"
        fi
    fi
    
    echo -e "${BLUE}=== Gerät an ${port} bearbeitet. ===${NC}"
    echo "---"
done

echo ""
echo -e "${BLUE}=== Abschlussübersicht der Updates ===${NC}"
echo "+----------------------+-------------------+----------------+-----------------+"
echo -e "| ${YELLOW}Port                 ${NC}| ${BLUE}MAC-Adresse       ${NC}| ${YELLOW}Firmware       ${NC}| ${YELLOW}Filesystem      ${NC}|"
echo "+----------------------+-------------------+----------------+-----------------+"

for port in $ESP_PORTS; do
    fw_status=${RESULTS_FW["$port"]}
    fs_status=${RESULTS_FS["$port"]}
    mac_addr=${RESULTS_MAC["$port"]}
    
    # Farben für die Statusanzeige
    FW_COLOR=""
    FS_COLOR=""
    
    case "$fw_status" in
        "erfolgreich") FW_COLOR="$GREEN";;
        "failed") FW_COLOR="$RED";;
        "unverändert") FW_COLOR="$NC";;
        *) FW_COLOR="$NC";;
    esac
    
    case "$fs_status" in
        "erfolgreich") FS_COLOR="$GREEN";;
        "failed") FS_COLOR="$RED";;
        "unverändert") FS_COLOR="$NC";;
        *) FS_COLOR="$NC";;
    esac
    
    printf "| %-20s | %b%-17s%b | %b%-14s%b | %b%-15s%b |\n" \
    "$port" \
    "$BLUE" "$mac_addr" "$NC" \
    "$FW_COLOR" "$fw_status" "$NC" \
    "$FS_COLOR" "$fs_status" "$NC"
done

echo "+----------------------+-------------------+----------------+-----------------+"
echo -e "${BLUE}=== Update-Prozess abgeschlossen. ===${NC}"
echo ""