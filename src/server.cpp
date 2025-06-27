#include <server.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void wsBrodcastMessage(String message)
{
  ws.textAll(message);
}

void broadcastLastTime(unsigned long lastTime)
{
  wsBrodcastMessage("{\"type\":\"lastTime\",\"value\":" + String(lastTime) + "}");
}

void broadcastSavedDevices()
{
  wsBrodcastMessage("{\"type\":\"saved_devices\",\"data\":" + getSavedDevicesJson() + "}");
}

void broadcastDiscoveredDevices()
{
  wsBrodcastMessage("{\"type\":\"discovered_devices\",\"data\":" + getDiscoveredDevicesJson() + "}");
}

void broadcastLichtschrankeStatus(LichtschrankeStatus status)
{
  wsBrodcastMessage("{\"type\":\"status\",\"status\":\"" + statusToString(status) + "\"}");
}

// Passe die WebSocket-Init an:
void initWebsocket()
{
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *, uint8_t *, size_t)
             {
    if (type == WS_EVT_CONNECT) {
      Serial.printf("[WS_DEBUG] WebSocket Client #%u verbunden.\n", client->id());
      // Status
      client->text("{\"type\":\"status\",\"status\":\"" + statusToString(getStatus()) + "\"}");
      // Letzte Zeit
      client->text("{\"type\":\"lastTime\",\"value\":" + String(getLastTime()) + "}");
      // Gespeicherte Geräte
      client->text("{\"type\":\"saved_devices\",\"data\":" + getSavedDevicesJson() + "}");
      // Entdeckte Geräte (kann noch leer sein)
      client->text("{\"type\":\"discovered_devices\",\"data\":" + getDiscoveredDevicesJson() + "}");
      // Starte Gerätesuche für diesen Client
      searchForDevices(); // <-- NEU: Suche direkt beim Connect starten!
    } else if (type == WS_EVT_DISCONNECT) {
      Serial.printf("[WS_DEBUG] WebSocket Client #%u getrennt.\n", client->id());
    } });
}

String generateMainPage(unsigned long lastTime)
{
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Zeitnahme</title>
      <meta charset="utf-8"/>
      <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
      <style>
        html, body {
          height: 100%;
          margin: 0;
          padding: 0;
          background: #f8f8f8;
        }
        body {
          min-height: 100vh;
          display: flex;
          flex-direction: column;
          justify-content: center;
          align-items: center;
          background: #f8f8f8;
        }
        #zeit {
          font-size: 10vw;
          font-family: monospace;
          margin-bottom: 0.5em;
          word-break: break-all;
          text-align: center;
          color: #222;
        }
        #settings-btn {
          position: fixed;
          right: 5vw;
          bottom: 5vw;
          font-size: 2.5em;
          background: #fff;
          border: none;
          border-radius: 50%;
          width: 60px;
          height: 60px;
          box-shadow: 0 2px 8px rgba(0,0,0,0.15);
          cursor: pointer;
          display: flex;
          align-items: center;
          justify-content: center;
          transition: background 0.2s;
          z-index: 10;
        }
        #settings-btn:active, #settings-btn:hover {
          background: #eee;
        }
        @media (max-width: 600px) {
          #zeit {
            font-size: 14vw;
          }
          #settings-btn {
            width: 56px;
            height: 56px;
            font-size: 2em;
            right: 4vw;
            bottom: 4vw;
          }
        }
      </style>
    </head>
    <body>
      <div id="zeit">)rawliteral";
  html += String(lastTime);
  html += R"rawliteral(</div>
      <button id="settings-btn" onclick="window.location.href='/config'" aria-label="Einstellungen">⚙️</button>
      <script>
        function formatDuration(ms) {
          let milliseconds = ms % 1000;
          let totalSeconds = Math.floor(ms / 1000);
          let seconds = totalSeconds % 60;
          let totalMinutes = Math.floor(totalSeconds / 60);
          let minutes = totalMinutes % 60;
          let hours = Math.floor(totalMinutes / 60);

          let parts = [];
          if (hours > 0) parts.push(String(hours));
          if (minutes > 0 || hours > 0) parts.push(String(minutes).padStart(hours > 0 ? 2 : 1, '0'));
          parts.push(String(seconds).padStart((minutes > 0 || hours > 0) ? 2 : 1, '0'));

          return parts.join(':') + ',' + String(milliseconds).padStart(3, '0');
        }

        // Initial formatting
        document.getElementById('zeit').textContent = formatDuration(Number(document.getElementById('zeit').textContent));

        let ws = new WebSocket('ws://' + location.host + '/ws');
        ws.onmessage = function(event) {
          try {
            let msg = JSON.parse(event.data);
            if (msg.type === "lastTime") {
              document.getElementById('zeit').textContent = formatDuration(Number(msg.value));
            }
          } catch(e) {}
        };
      </script>
    </body>
    </html>
  )rawliteral";
  return html;
}

// HTML für /config
String generateConfigPage()
{
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Konfiguration</title>
      <meta charset="utf-8"/>
      <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
      <style>
        html, body {
          height: 100%;
          margin: 0;
          padding: 0;
          background: #f8f8f8;
        }
        body {
          min-height: 100vh;
          display: flex;
          flex-direction: column;
          align-items: center;
          font-family: system-ui, sans-serif;
          background: #f8f8f8;
        }
        h1 {
          font-size: 2em;
          margin-top: 1.2em;
          margin-bottom: 0.5em;
          text-align: center;
        }
        .status-row {
          margin: 1.2em 0 1em 0;
          display: flex;
          align-items: center;
          justify-content: center;
          font-size: 1.3em;
        }
        #statusEmoji {
          font-size: 2.2em;
          margin-right: 0.5em;
        }
        .triggered { color: #e53935; }
        .not-triggered { color: #43a047; }
        .cooldown { color: #fbc02d; }
        .unknown { color: #888; }
        .threshold-container {
          background: #fff;
          border-radius: 10px;
          padding: 1.5em;
          margin: 1em auto;
          box-shadow: 0 2px 8px rgba(0,0,0,0.07);
          max-width: 600px;
          width: fit-content;
          min-width: 300px;
        }
        .sensor-grid {
          display: grid;
          grid-template-columns: auto auto;
          gap: 2em;
          justify-content: center;
        }
        .sensor-item {
          display: flex;
          flex-direction: column;
          min-width: 180px;
          max-width: 250px;
        }
        .sensor-label {
          font-weight: 600;
          font-size: 1.1em;
          margin-bottom: 0.5em;
          color: #333;
        }
        .sensor-value {
          font-family: monospace;
          font-size: 1.2em;
          color: #2196f3;
          margin-bottom: 0.8em;
          padding: 0.5em;
          background: #f0f8ff;
          border-radius: 6px;
          border-left: 4px solid #2196f3;
          min-height: 1.5em;
          display: flex;
          align-items: center;
        }
        .sensor-input {
          display: flex;
          align-items: center;
          gap: 0.5em;
          margin-bottom: 0.8em;
        }
        .sensor-input input {
          flex: 1;
          padding: 0.5em;
          border: 1px solid #bbb;
          border-radius: 6px;
          font-size: 1em;
          min-width: 80px;
          max-width: 120px;
        }
        .sensor-input span {
          color: #666;
          font-size: 0.9em;
          min-width: 30px;
        }
        .sensor-button {
          width: 200px;
          max-width: 200px;
          min-width: 200px;
          background: #2196f3;
          color: white;
          border: 1px solid #1976d2;
          padding: 0.8em 1.5em;
          border-radius: 6px;
          cursor: pointer;
          font-size: 1em;
          font-weight: 600;
          transition: all 0.3s ease;
          box-sizing: border-box;
        }
        .sensor-button:hover {
          background: #1976d2;
        }
        .sensor-button.success {
          background: #4caf50 !important;
          border-color: #45a049 !important;
          color: white !important;
        }
        .sensor-button.success:hover {
          background: #4caf50 !important;
          border-color: #45a049 !important;
          color: white !important;
        }
        .sensor-button.success:disabled {
          background: #4caf50 !important;
          border-color: #45a049 !important;
          color: white !important;
          opacity: 1 !important;
          cursor: default !important;
        }
        .sensor-button:disabled {
          background: #bbbbbb !important;
          border-color: #aaaaaa !important;
          color: #ffffff !important;
          cursor: not-allowed !important;
          opacity: 0.6;
        }
        .sensor-button.changed {
          background: #2196f3 !important;
          border-color: #1976d2 !important;
          opacity: 1;
        }
        .sensor-button.changed:hover {
          background: #1976d2 !important;
        }
        .devices-container {
          background: #fff;
          border-radius: 10px;
          padding: 1.5em;
          margin: 1em auto;
          box-shadow: 0 2px 8px rgba(0,0,0,0.07);
          max-width: 600px;
          width: fit-content;
          min-width: 300px;
        }
        .device-grid {
          display: flex;
          flex-direction: column;
          gap: 1.2em;
        }
        .device-item {
          display: grid;
          grid-template-columns: 1fr auto;
          gap: 1em;
          align-items: center;
          padding: 1em;
          background: #f8f9fa;
          border-radius: 8px;
          border-left: 4px solid #ddd;
          transition: all 0.3s ease;
        }
        .device-item.self {
          border-left-color: #2196f3;
          background: #f0f8ff;
        }
        .device-item.saved-online {
          border-left-color: #43a047;
        }
        .device-item.saved-offline {
          border-left-color: #e53935;
        }
        .device-item.discovered {
          border-left-color: #ffc107;
        }
        .device-info {
          display: flex;
          flex-direction: column;
          min-width: 0;
        }
        .device-mac {
          font-family: monospace;
          font-size: 1.1em;
          font-weight: 600;
          margin-bottom: 0.3em;
          color: #333;
          word-break: break-all;
        }
        .device-status {
          font-size: 0.9em;
          color: #666;
          display: flex;
          align-items: center;
          gap: 0.5em;
        }
        .device-status-icon {
          font-size: 1.1em;
        }
        .device-role-select {
          min-width: 120px;
          padding: 0.6em 0.8em;
          border: 1px solid #ccc;
          border-radius: 6px;
          background: #fff;
          font-size: 1em;
          cursor: pointer;
          transition: all 0.2s ease;
        }
        .device-role-select:hover {
          border-color: #2196f3;
        }
        .device-role-select:focus {
          outline: none;
          border-color: #2196f3;
          box-shadow: 0 0 0 2px rgba(33, 150, 243, 0.2);
        }
        .device-role-select.pending {
          background: #fff3cd;
          border-color: #ffc107;
        }
        #resetBtn {
          color: #e53935;
          border: 1px solid #e53935;
          background: #fff;
          margin-top: 1.2em;
          margin-bottom: 2.5em;
        }
        #home-btn {
          position: fixed;
          right: 5vw;
          bottom: 5vw;
          font-size: 2.5em;
          background: #fff;
          border: none;
          border-radius: 50%;
          width: 60px;
          height: 60px;
          box-shadow: 0 2px 8px rgba(0,0,0,0.15);
          cursor: pointer;
          display: flex;
          align-items: center;
          justify-content: center;
          transition: background 0.2s;
          z-index: 10;
        }
        #home-btn:active, #home-btn:hover {
          background: #eee;
        }
        @media (max-width: 600px) {
          h1 { font-size: 1.3em; }
          .status-row { font-size: 1em; }
          #statusEmoji { font-size: 1.5em; }
          #home-btn {
            width: 56px;
            height: 56px;
            font-size: 2em;
            right: 4vw;
            bottom: 4vw;
          }
          .threshold-container, .devices-container {
            padding: 1em;
            margin: 1em 0.5em;
            width: fit-content;
            min-width: 280px;
            max-width: calc(100vw - 1em);
          }
          .sensor-grid {
            grid-template-columns: 1fr;
            gap: 1.5em;
            justify-content: stretch;
          }
          .sensor-item {
            min-width: auto;
            max-width: none;
          }
          .sensor-label {
            font-size: 1em;
          }
          .sensor-value {
            font-size: 1.1em;
            padding: 0.4em;
          }
          .sensor-button {
            width: 100%;
            max-width: none;
            min-width: 160px;
            padding: 0.7em 1em;
            font-size: 0.95em;
            box-sizing: border-box;
          }
          .sensor-input input {
            padding: 0.6em 0.5em;
            max-width: 100px;
          }
          .device-item {
            grid-template-columns: 1fr;
            gap: 0.8em;
            text-align: center;
          }
          .device-info {
            text-align: left;
          }
          .device-role-select {
            width: 100%;
            min-width: 100px;
          }
        }
        
        @media (max-width: 400px) {
          .threshold-container, .devices-container {
            margin: 0.5em 0.25em;
            padding: 0.8em;
            min-width: 250px;
            max-width: calc(100vw - 0.5em);
          }
          .sensor-grid {
            gap: 1em;
          }
          .sensor-button {
            width: 100%;
            max-width: none;
            min-width: 140px;
            padding: 0.8em 0.5em;
            font-size: 0.9em;
            box-sizing: border-box;
          }
          .sensor-input input {
            max-width: 80px;
          }
          .device-grid {
            gap: 1em;
          }
          .device-item {
            padding: 0.8em;
          }
        }
      </style>
    </head>
    <body>
      <h1>Konfiguration</h1>
      <div class="status-row">
        <span id="statusEmoji" class="unknown">⚪</span>
        <span id="statusText">Unbekannt</span>
      </div>
      <div class="threshold-container">
        <h3 style="margin-top: 0; margin-bottom: 1.5em;">Sensor-Einstellungen</h3>
        
        <div class="sensor-grid">
          <div class="sensor-item">
            <div class="sensor-label">Basisdistanz</div>
            <div class="sensor-value" id="baseDistanceValue">-- cm</div>
            <button class="sensor-button" id="recalibrateBtn">Rekalibrieren</button>
          </div>
          
          <div class="sensor-item">
            <div class="sensor-label">Auslöseschwelle</div>
            <div class="sensor-input">
              <input type="number" id="thresholdInput" min="1" max="200" step="0.1" value="50.0" placeholder="50.0">
              <span>cm</span>
            </div>
            <button class="sensor-button" id="saveThresholdBtn">Schwelle speichern</button>
          </div>
        </div>
        
      </div>
      <div class="devices-container">
        <h3 style="margin-top: 0; margin-bottom: 1.5em;">ESP-Geräte</h3>
        <div class="device-grid" id="devicesContainer">
          <!-- Geräte werden hier dynamisch eingefügt -->
        </div>
      </div>
      <button id="resetBtn">Alle Einstellungen löschen</button>
      <button id="home-btn" onclick="window.location.href='/'" aria-label="Zur Hauptseite">🏠</button>
      <script>
        // Variablen nur einmal deklarieren!
        const selfMac = ")rawliteral" +
                macToString(getMacAddress()) + R"rawliteral(";
        const selfRole = ")rawliteral" +
                roleToString(getOwnRole()) + R"rawliteral(";
        let savedDevices = [];
        let discoveredDevices = [];

        let ws = new WebSocket('ws://' + location.host + '/ws');
        ws.onmessage = function(event) {
          let msg = {};
          try { msg = JSON.parse(event.data); } catch(e) {}
          if (msg.type === "saved_devices") {
            savedDevices = msg.data;
            showAllDevices();
          } else if (msg.type === "discovered_devices") {
            discoveredDevices = msg.data;
            showAllDevices();
          } else if (msg.type === "status") {
            let emoji = document.getElementById('statusEmoji');
            let text = document.getElementById('statusText');
            switch (msg.status) {
              case "normal":
                emoji.textContent = "🟢";
                emoji.className = "not-triggered";
                text.textContent = "Bereit";
                break;
              case "triggered":
                emoji.textContent = "🔴";
                emoji.className = "triggered";
                text.textContent = "Ausgelöst";
                break;
              case "cooldown":
                emoji.textContent = "🟡";
                emoji.className = "cooldown";
                text.textContent = "Cooldown";
                break;
              case "triggered_in_cooldown":
                emoji.textContent = "🟠";
                emoji.className = "cooldown";
                text.textContent = "Ausgelöst (Cooldown)";
                break;
              default:
                emoji.textContent = "⚪";
                emoji.className = "unknown";
                text.textContent = "Unbekannt";
            }
          }
        };

        document.getElementById('recalibrateBtn').onclick = function() {
          const btn = this;
          const originalText = btn.textContent;
          btn.textContent = 'Kalibriere...';
          btn.disabled = true;
          btn.classList.remove('success'); // Stelle sicher, dass kein alter Zustand da ist
          
          fetch('/recalibrate', {method: 'POST'})
            .then(response => response.text())
            .then(text => {
              // Aktualisiere die Basisdistanz-Anzeige nach Rekalibrierung
              loadBaseDistance();
              
              // Visuelles Feedback
              btn.textContent = '✓ Kalibriert';
              btn.disabled = true; // Button ist während grünem Zustand deaktiviert
              btn.classList.add('success');
              
              setTimeout(() => {
                btn.textContent = originalText;
                btn.classList.remove('success');
                btn.disabled = false;
              }, 2000);
            })
            .catch(() => {
              btn.textContent = originalText;
              btn.classList.remove('success');
              btn.disabled = false;
            });
        };

        // Lade aktuellen Threshold-Wert beim Laden der Seite
        let originalThreshold = 50.0;
        
        fetch('/get_threshold')
          .then(response => response.json())
          .then(data => {
            originalThreshold = data.threshold;
            document.getElementById('thresholdInput').value = data.threshold;
            updateThresholdButton();
          })
          .catch(err => console.log('Fehler beim Laden des Threshold-Werts:', err));

        // Lade aktuelle Basisdistanz beim Laden der Seite
        function loadBaseDistance() {
          fetch('/get_base_distance')
            .then(response => response.json())
            .then(data => {
              const baseDistanceElement = document.getElementById('baseDistanceValue');
              if (data.isMaxRange) {
                baseDistanceElement.textContent = 'Kein Hindernis';
                baseDistanceElement.style.color = '#888';
                baseDistanceElement.style.fontStyle = 'italic';
              } else {
                baseDistanceElement.textContent = data.baseDistance.toFixed(1) + ' cm';
                baseDistanceElement.style.color = '';
                baseDistanceElement.style.fontStyle = '';
              }
            })
            .catch(err => console.log('Fehler beim Laden der Basisdistanz:', err));
        }
        loadBaseDistance();

        function updateThresholdButton() {
          const input = document.getElementById('thresholdInput');
          const button = document.getElementById('saveThresholdBtn');
          const currentValue = parseFloat(input.value);
          
          if (isNaN(currentValue) || currentValue === originalThreshold) {
            button.disabled = true;
            button.classList.remove('changed');
            button.textContent = 'Schwelle speichern';
          } else {
            button.disabled = false;
            button.classList.add('changed');
            button.textContent = 'Schwelle speichern';
          }
        }

        // Überwache Eingabefeld für Änderungen
        document.getElementById('thresholdInput').addEventListener('input', updateThresholdButton);
        document.getElementById('thresholdInput').addEventListener('change', updateThresholdButton);

        document.getElementById('saveThresholdBtn').onclick = function() {
          let threshold = parseFloat(document.getElementById('thresholdInput').value);
          if (isNaN(threshold) || threshold <= 0 || threshold > 200) {
            // Visuelles Feedback für Fehler
            const input = document.getElementById('thresholdInput');
            const originalBorder = input.style.borderColor;
            input.style.borderColor = '#e53935';
            input.style.boxShadow = '0 0 5px rgba(229, 57, 53, 0.3)';
            
            setTimeout(() => {
              input.style.borderColor = originalBorder;
              input.style.boxShadow = '';
            }, 2000);
            return;
          }
          
          const btn = this;
          const originalText = btn.textContent;
          btn.textContent = 'Speichere...';
          btn.disabled = true;
          btn.classList.remove('changed');
          
          fetch('/set_threshold', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: 'threshold=' + threshold
          })
            .then(response => response.text())
            .then(text => {
              // Aktualisiere den ursprünglichen Wert
              originalThreshold = threshold;
              
              // Visuelles Feedback für Erfolg
              btn.textContent = '✓ Gespeichert';
              btn.classList.remove('changed');
              btn.classList.add('success');
              btn.disabled = true; // Button ist während grünem Zustand deaktiviert
              
              setTimeout(() => {
                btn.classList.remove('success');
                updateThresholdButton(); // Reset Button-Status (wird wieder grau/deaktiviert)
              }, 2000);
            })
            .catch(err => {
              btn.textContent = originalText;
              btn.classList.add('changed');
              updateThresholdButton();
            });
        };

        function getAllDevices() {
          let all = [...savedDevices];
          discoveredDevices.forEach(dev => {
            if (!all.some(d => d.mac === dev.mac)) {
              all.push(dev);
            }
          });
          return all;
        }

        function showAllDevices() {
          let container = document.getElementById('devicesContainer');
          container.innerHTML = '';

          // Eigenes Gerät immer zuerst
          let selfDev = {mac: selfMac, role: selfRole};
          let otherDevs = getAllDevices().filter(d => d.mac !== selfMac);

          // Eigenes Gerät
          createDeviceItem(container, selfDev, true);

          // Andere Geräte
          otherDevs.forEach(dev => {
            createDeviceItem(container, dev, false);
          });
        }

        function createDeviceItem(container, dev, isSelf) {
          let saved = savedDevices.find(d => d.mac === dev.mac);
          let role = saved ? saved.role : (isSelf ? dev.role : "-");
          let isSaved = !!saved || isSelf;
          let isOnline = discoveredDevices.some(d => d.mac === dev.mac) || isSelf;

          let deviceClass = "device-item";
          let statusIcon = "";
          let statusText = "";
          
          if (isSelf) {
            deviceClass += " self";
            statusIcon = "📱";
            statusText = "Dieses Gerät";
          } else if (isSaved && isOnline) {
            deviceClass += " saved-online";
            statusIcon = "🟢";
            statusText = "Gespeichert & Online";
          } else if (isSaved && !isOnline) {
            deviceClass += " saved-offline";
            statusIcon = "🔴";
            statusText = "Gespeichert & Offline";
          } else if (!isSaved && isOnline) {
            deviceClass += " discovered";
            statusIcon = "🟡";
            statusText = "Entdeckt";
          } else {
            statusIcon = "⚪";
            statusText = "Unbekannt";
          }

          let roleOptions = (isSelf ? ['Start','Ziel'] : ['-','Start','Ziel']).map(opt =>
            `<option value="${opt}"${role===opt?' selected':''}>${opt}</option>`
          ).join('');

          let deviceItem = document.createElement('div');
          deviceItem.className = deviceClass;
          deviceItem.dataset.mac = dev.mac;
          deviceItem.innerHTML = `
            <div class="device-info">
              <div class="device-mac">${dev.mac}</div>
              <div class="device-status">
                <span class="device-status-icon">${statusIcon}</span>
                <span>${statusText}</span>
              </div>
            </div>
            <select class="device-role-select" onchange="saveDeviceRole(this, '${dev.mac}', ${isSelf})">${roleOptions}</select>
          `;
          container.appendChild(deviceItem);
        }

        function saveDeviceRole(selectElement, mac, isSelf) {
          let role = selectElement.value;
          
          // Visueller Feedback während des Speicherns
          selectElement.classList.add('pending');
          selectElement.disabled = true;
          
          if (isSelf) {
            // Eigene Rolle ändern
            fetch('/config', {
              method: 'POST',
              headers: {'Content-Type': 'application/x-www-form-urlencoded'},
              body: 'role=' + encodeURIComponent(role)
            }).then(response => {
              if (response.ok) {
                // Erfolg - Seite neu laden nach kurzer Verzögerung
                setTimeout(() => {
                  location.reload();
                }, 500);
              } else {
                // Fehler - Zurücksetzen
                selectElement.classList.remove('pending');
                selectElement.disabled = false;
                alert('Fehler beim Speichern der Rolle');
              }
            }).catch(() => {
              selectElement.classList.remove('pending');
              selectElement.disabled = false;
              alert('Fehler beim Speichern der Rolle');
            });
          } else {
            fetch('/change_device', {
              method: 'POST',
              headers: {'Content-Type': 'application/x-www-form-urlencoded'},
              body: 'mac=' + encodeURIComponent(mac) + '&role=' + encodeURIComponent(role)
            }).then(response => {
              selectElement.classList.remove('pending');
              selectElement.disabled = false;
              if (response.ok) {
                // Erfolg - Update der Anzeige
                showAllDevices();
              } else {
                alert('Fehler beim Speichern des Geräts');
              }
            }).catch(() => {
              selectElement.classList.remove('pending');
              selectElement.disabled = false;
              alert('Fehler beim Speichern des Geräts');
            });
          }
        }
        showAllDevices();

        // Automatisch beim Laden Geräte suchen
        window.addEventListener('DOMContentLoaded', () => {
          discoverDevices();
        });

        function discoverDevices() {
          fetch('/discover', {method: 'POST'});
        }

        document.getElementById('resetBtn').onclick = function() {
          if (confirm("Wirklich alle Einstellungen löschen? Das Gerät muss danach neu konfiguriert werden!")) {
            fetch('/reset', {method: 'POST'})
              .then(r => r.text())
              .then(msg => {
                alert(msg);
                location.reload();
              });
          }
        };
      </script>
    </body>
    </html>
  )rawliteral";
  return html;
}

void initWebpage()
{
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("⏱️" + macToString(getMacAddress()), "", 5);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
Serial.println("[WEB] GET / aufgerufen.");
request->send(200, "text/html", generateMainPage(getLastTime())); });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      loadDeviceListFromPreferences();
      searchForDevices();              
      Serial.println("[WEB] GET /config aufgerufen.");
      request->send(200, "text/html", generateConfigPage()); });

  server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request)
            {
Serial.println("[WEB] POST /config aufgerufen (Rollenänderung der eigenen Rolle).");
if (request->hasParam("role", true)) {
String roleStr = request->getParam("role", true)->value();
Role newRole = stringToRole(roleStr);
Serial.printf("[WEB] Eigene Rolle soll geändert werden zu: %s\n", roleToString(newRole).c_str());
changeOwnRole(newRole); // Speichert und sendet Updates an andere
}
request->redirect("/config"); });

  server.on("/discover", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              searchForDevices();
request->send(200, "text/plain", "OK"); });

  server.on("/change_device", HTTP_POST, [](AsyncWebServerRequest *request)
            {
Serial.println("[WEB] POST /change_device aufgerufen (Änderung/Entfernung eines anderen Geräts).");
if (request->hasParam("mac", true) && request->hasParam("role", true)) {
String macStr = request->getParam("mac", true)->value();
String roleStr = request->getParam("role", true)->value();
uint8_t mac[6];
sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
 &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
Role role = stringToRole(roleStr);

if (memcmp(mac, getMacAddress(), 6) == 0) {
Serial.println("[WEB] Versuch, eigene Rolle über /change_device zu ändern. Nicht erlaubt.");
request->send(400, "text/plain", "Eigene Rolle kann nur über /config geändert werden.");
return;
}

if (role == ROLE_IGNORE) {
Serial.printf("[WEB] Gerät %s soll entfernt/ignoriert werden.\n", macToString(mac).c_str());
removeSavedDevice(mac); // Entfernt aus Preferences
// Sende Nachricht an das Zielgerät, damit es sich selbst entfernt
tellOtherDeviceToChangeHisRole(mac, ROLE_IGNORE);
request->send(200, "text/plain", "Gerät entfernt");
} else {
Serial.printf("[WEB] Gerät %s soll auf Rolle %s gesetzt werden.\n", macToString(mac).c_str(), roleToString(role).c_str());
if (changeOtherDevice(mac, role)) { // Sendet Nachricht an das Zielgerät und aktualisiert meine Liste
  changeSavedDevice(mac, role); // Aktualisiert meine Liste
request->send(200, "text/plain", "Saved");
} else {
Serial.println("[WEB] Fehler beim Speichern/Ändern des Geräts.");
request->send(400, "text/plain", "Failed to save device");
}
}
} else {
Serial.println("[WEB] Fehlende MAC oder Rolle bei /change_device.");
request->send(400, "text/plain", "Missing MAC or role");
} });

  server.on("/preferences", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String msg = "S: "+getSavedDevicesJson()+"\nD: "+getDiscoveredDevicesJson()+"\nR: "+roleToString(getOwnRole());
    request->send(200, "application/json", msg); });

  server.on("/recalibrate", HTTP_POST, [](AsyncWebServerRequest *request)
            {
Serial.println("[WEB] POST /recalibrate aufgerufen. Starte Rekalibrierung.");
String msg = "Rekalibriert! Neue Distanz: " + String(calibrateSensor(), 2) + " cm";
request->send(200, "text/plain", msg); });

  server.on("/get_threshold", HTTP_GET, [](AsyncWebServerRequest *request)
            {
Serial.println("[WEB] GET /get_threshold aufgerufen.");
float threshold = getSensorThreshold();
String json = "{\"threshold\":" + String(threshold, 1) + "}";
request->send(200, "application/json", json); });

  server.on("/get_base_distance", HTTP_GET, [](AsyncWebServerRequest *request)
            {
Serial.println("[WEB] GET /get_base_distance aufgerufen.");
float baseDistance = getBaseDistance();
bool isMaxRange = isBaseDistanceMaxRange();
String json = "{\"baseDistance\":" + String(baseDistance, 1) + ",\"isMaxRange\":" + (isMaxRange ? "true" : "false") + "}";
request->send(200, "application/json", json); });

  server.on("/set_threshold", HTTP_POST, [](AsyncWebServerRequest *request)
            {
Serial.println("[WEB] POST /set_threshold aufgerufen.");
if (request->hasParam("threshold", true)) {
  float threshold = request->getParam("threshold", true)->value().toFloat();
  setSensorThreshold(threshold);
  String msg = "Schwelle gesetzt auf " + String(threshold, 1) + " cm";
  request->send(200, "text/plain", msg);
} else {
  request->send(400, "text/plain", "Fehlender Threshold-Parameter");
} });

  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request)
            {
Serial.println("[WEB] POST /reset aufgerufen. Lösche alle Preferences.");
resetAll();
request->send(200, "text/plain", "Alle Einstellungen wurden gelöscht. Bitte Gerät neu konfigurieren."); });

  server.on("/save_device", HTTP_POST, [](AsyncWebServerRequest *request)
            {
Serial.println("[WEB] POST /save_device aufgerufen (Speichern eines Geräts).");
if (request->hasParam("mac", true) && request->hasParam("role", true)) {
  String macStr = request->getParam("mac", true)->value();
  String roleStr = request->getParam("role", true)->value();
  uint8_t mac[6];
  sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
      &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
  Role role = stringToRole(roleStr);
  addSavedDevice(mac, role);
  request->send(200, "text/plain", "Gerät gespeichert");
} else {
  request->send(400, "text/plain", "Fehlende Parameter");
} });
  server.addHandler(&ws);
  server.begin();
  Serial.println("[WEB] Webserver gestartet.");
}