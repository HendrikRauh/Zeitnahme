#include <server.h>

AsyncWebServer server(80);

AsyncWebSocket ws("/ws");

void wsBrodcastMessage(String message)
{
  ws.textAll(message);
}

void broadcastTimeToClients(unsigned long time)
{
  wsBrodcastMessage(String(time));
}

void broadcastLichtschrankeStatus(LichtschrankeStatus status)
{
  wsBrodcastMessage("{\"type\":\"status\",\"status\":\"" + statusToString(status) + "\"}");
}

void initWebsocket()
{
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *, uint8_t *, size_t)
             {
    if (type == WS_EVT_CONNECT) {
      Serial.printf("[WS_DEBUG] WebSocket Client #%u verbunden.\n", client->id());
      // Sende initialen Status an neuen Client
      client->text("{\"type\":\"status\",\"status\":\"" + statusToString(getStatus()) + "\"}");
      // Sende aktuelle Liste der entdeckten Geräte
      sendDiscoveryMessage();
      DynamicJsonDocument doc(1024);
      JsonArray arr = doc.to<JsonArray>();
      for (const auto& dev : getDiscoveredDevices()) {
        JsonObject obj = arr.createNestedObject();
        obj["mac"] = macToString(dev.mac);
        obj["role"] = roleToString(dev.role);
      }
      String jsonDevices;
      serializeJson(arr, jsonDevices);
      client->text("{\"type\":\"initial_devices\",\"data\":" + jsonDevices + "}");
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

        let ws = new WebSocket('ws://' + location.host + '/ws');
        ws.onmessage = function(event) {
          let val = event.data;
          try {
            let msg = JSON.parse(event.data);
            if (msg.type !== undefined) return; // ignore status etc.
            val = msg;
          } catch(e) {}
          document.getElementById('zeit').textContent = formatDuration(Number(val));
        };
        // Initial formatting
        document.getElementById('zeit').textContent = formatDuration(Number(document.getElementById('zeit').textContent));
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
        .table-container {
          width: 100%;
          max-width: 600px;
          overflow-x: auto;
          margin-bottom: 1.5em;
        }
        table {
          width: 100%;
          border-collapse: collapse;
          background: #fff;
          border-radius: 10px;
          overflow: hidden;
          font-size: 1em;
          box-shadow: 0 2px 8px rgba(0,0,0,0.07);
        }
        th, td {
          padding: 0.7em 0.5em;
          text-align: center;
        }
        th {
          background: #f0f0f0;
          font-weight: 600;
        }
        tr:nth-child(even) {
          background: #fafafa;
        }
        select, button {
          font-size: 1em;
          padding: 0.3em 0.7em;
          border-radius: 6px;
          border: 1px solid #bbb;
          background: #f9f9f9;
        }
        button {
          cursor: pointer;
          transition: background 0.2s;
        }
        button:active, button:hover {
          background: #eee;
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
          table { font-size: 0.95em; }
          #home-btn {
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
      <h1>Konfiguration</h1>
      <div class="status-row">
        <span id="statusEmoji" class="unknown">⚪</span>
        <span id="statusText">Unbekannt</span>
      </div>
      <div class="table-container">
        <table id="devicesTable">
          <thead>
            <tr><th>MAC</th><th>Rolle</th><th>Aktion</th></tr>
          </thead>
          <tbody id="devicesBody"></tbody>
        </table>
      </div>
      <button id="resetBtn">Alle Einstellungen löschen</button>
      <button id="home-btn" onclick="window.location.href='/'" aria-label="Zur Hauptseite">🏠</button>
      <script>
        // Statusanzeige per WebSocket
        let ws = new WebSocket('ws://' + location.host + '/ws');
        ws.onmessage = function(event) {
          try {
            let msg = JSON.parse(event.data);
            if (msg.type === "status") {
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
            } else if (msg.type === "device") {
              if (!msg.data.role) msg.data.role = "-";
              if (!discoveredDevices.some(d => d.mac === msg.data.mac)) {
                discoveredDevices.push(msg.data);
                showAllDevices();
              }
            }
          } catch(e) {}
        };

        // Geräteverwaltung
        const selfMac = ")rawliteral" +
                macToString(getMacAddress()) + R"rawliteral(";
        const selfRole = ")rawliteral" +
                roleToString(getOwnRole()) + R"rawliteral(";
        const savedDevices = )rawliteral" +
                getSavedDevicesJson() + R"rawliteral(;
        let discoveredDevices = )rawliteral" +
                getDiscoveredDevicesJson() + R"rawliteral(;
        if (!Array.isArray(discoveredDevices)) discoveredDevices = [];

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
          let tbody = document.getElementById('devicesBody');
          tbody.innerHTML = '';

          // Eigenes Gerät immer zuerst
          let selfDev = {mac: selfMac, role: selfRole};
          let otherDevs = getAllDevices().filter(d => d.mac !== selfMac);

          // Eigene Zeile
          let tr = document.createElement('tr');
          tr.dataset.mac = selfDev.mac;
          let roleOptions = ['Start','Ziel'].map(opt =>
            `<option value="${opt}"${(selfDev.role||"Start")===opt?' selected':''}>${opt}</option>`
          ).join('');
          tr.innerHTML =
            `<td><span style="color:none;font-weight:bold">${selfDev.mac}</span></td>
             <td><select>${roleOptions}</select></td>
             <td><button onclick="saveDeviceRow(this)">Speichern</button></td>`;
          tbody.appendChild(tr);

          // Andere Geräte
          otherDevs.forEach(dev => {
            let saved = savedDevices.find(d => d.mac === dev.mac);
            let role = saved ? saved.role : "-";
            let isSaved = !!saved;
            let isOnline = discoveredDevices.some(d => d.mac === dev.mac);

            let macColor = "black";
            if (isSaved && isOnline) macColor = "#43a047";
            else if (isSaved && !isOnline) macColor = "#e53935";
            else if (!isSaved && isOnline) macColor = "#888";

            let roleOptions = ['-','Start','Ziel'].map(opt =>
              `<option value="${opt}"${role===opt?' selected':''}>${opt}</option>`
            ).join('');
            let tr = document.createElement('tr');
            tr.dataset.mac = dev.mac;
            tr.innerHTML =
              `<td><span style="color:${macColor};font-weight:bold">${dev.mac}</span></td>
               <td><select>${roleOptions}</select></td>
               <td><button onclick="saveDeviceRow(this)">Speichern</button></td>`;
            tbody.appendChild(tr);
          });
        }
        showAllDevices();

        // Automatisch beim Laden Geräte suchen
        window.addEventListener('DOMContentLoaded', () => {
          discoverDevices();
        });

        function discoverDevices() {
          fetch('/discover', {method: 'POST'});
        }

        function saveDeviceRow(btn) {
          let tr = btn.closest('tr');
          let mac = tr.dataset.mac;
          let role = tr.querySelector('select').value;
          if (mac === selfMac) {
            // Eigene Rolle ändern
            fetch('/config', {
              method: 'POST',
              headers: {'Content-Type': 'application/x-www-form-urlencoded'},
              body: 'role=' + encodeURIComponent(role)
            }).then(() => {
              alert('Rolle geändert!');
              location.reload();
            });
          } else {
            fetch('/change_device', {
              method: 'POST',
              headers: {'Content-Type': 'application/x-www-form-urlencoded'},
              body: 'mac=' + encodeURIComponent(mac) + '&role=' + encodeURIComponent(role)
            }).then(() => {
              alert('Gerät gespeichert!');
              location.reload();
            });
          }
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