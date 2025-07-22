#include <server.h>
#include <data.h>

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

void broadcastMasterStatus()
{
  JsonDocument doc;
  doc["type"] = "master_status";
  doc["status"] = masterStatusToString(getMasterStatus());
  if (isSlave())
  {
    doc["masterMac"] = macToShortString(getMasterMac());
  }
  String json;
  serializeJson(doc, json);
  wsBrodcastMessage(json);
}

void broadcastLichtschrankeStatus(LichtschrankeStatus status)
{
  // Immer senden - kein Caching für Status-Updates
  String currentJson = "{\"type\":\"status\",\"status\":\"" + statusToString(status) + "\"}";
  wsBrodcastMessage(currentJson);
  Serial.printf("[WS_DEBUG] Status gesendet: %s\n", statusToString(status).c_str());
}

// Passe die WebSocket-Init an:
void initWebsocket()
{
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *, uint8_t *, size_t)
             {
    if (type == WS_EVT_CONNECT) {
      Serial.printf("[WS_DEBUG] WebSocket Client #%u verbunden.\n", client->id());
      
      // Batch alle Initial-Daten in einer Nachricht für bessere Performance
      JsonDocument doc;
      doc["type"] = "initial_state";
      JsonObject data = doc.createNestedObject("data");
      data["status"] = statusToString(getStatus());
      data["master_status"] = masterStatusToString(getMasterStatus());
      if (isSlave()) {
        data["masterMac"] = macToShortString(getMasterMac());
      }
      data["lastTime"] = getLastTime();
      // saved_devices und discovered_devices sind bereits JSON-Strings, daher als JsonArray parsen
      JsonDocument savedDoc, discoveredDoc;
      deserializeJson(savedDoc, getSavedDevicesJson());
      deserializeJson(discoveredDoc, getDiscoveredDevicesJson());
      data["saved_devices"] = savedDoc;
      data["discovered_devices"] = discoveredDoc;
      String initialData;
      serializeJson(doc, initialData);
      client->text(initialData);
      
      // Sende ALLE aktuellen RAM-Daten an den neuen Client
      updateWebSocketClients();
      
      // Starte Gerätesuche NUR bei Bedarf, nicht automatisch
      // searchForDevices(); // Entfernt für bessere Performance
    } else if (type == WS_EVT_DISCONNECT) {
      Serial.printf("[WS_DEBUG] WebSocket Client #%u getrennt.\n", client->id());
    } });
}

void initWebpage()
{
  server.on("/NotoSansMono-Black.ttf", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/NotoSansMono-Black.ttf", "font/ttf");
    request->send(response); });
  // LittleFS initialisieren
  if (!LittleFS.begin(true))
  {
    Serial.println("[WEB] LittleFS Mount Failed!");
    return;
  }
  Serial.println("[WEB] LittleFS mounted successfully");

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("⏱️ " + macToShortString(getMacAddress()), "", ESP_NOW_CHANNEL);

  // API-Endpunkte für dynamische Daten (WICHTIG: Vor serveStatic definieren!)
  server.on("/api/device_info", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    JsonDocument doc;
    doc["selfMac"] = macToShortString(getMacAddress());
    doc["selfRole"] = roleToString(getOwnRole());
    doc["masterStatus"] = masterStatusToString(getMasterStatus());
    if (isSlave()) {
        doc["masterMac"] = macToShortString(getMasterMac());
    }
    doc["firmware_hash"] = ESP.getSketchMD5();
    
    // Berechne Filesystem-Hash zur Laufzeit
    String fsHash = "unknown";
    if (LittleFS.begin()) {
        // Einfacher Hash basierend auf verfügbaren Dateien
        uint32_t hashValue = 0;
        File root = LittleFS.open("/");
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                if (!file.isDirectory()) {
                    // Hash aus Dateiname und Größe
                    String fileName = file.name();
                    size_t fileSize = file.size();
                    // Einfacher String-Hash
                    for (int i = 0; i < fileName.length(); i++) {
                        hashValue = hashValue * 31 + fileName.charAt(i);
                    }
                    hashValue ^= fileSize;
                }
                file = root.openNextFile();
            }
        }
        fsHash = String(hashValue, HEX);
    }
    doc["filesystem_hash"] = fsHash;
    
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json); });

  server.on("/api/last_time", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    JsonDocument doc;
    doc["lastTime"] = getLastTime();
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json); });

  server.on("/api/lauf_count", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    JsonDocument doc;
    doc["count"] = getLaufCount();
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json); });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      loadDeviceListFromPreferences();
      searchForDevices();              
      Serial.println("[WEB] GET /config aufgerufen.");
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/config.html", "text/html");
      request->send(response); });

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
String shortMacStr = request->getParam("mac", true)->value();
String roleStr = request->getParam("role", true)->value();
uint8_t mac[6];

// Konvertiere verkürzte MAC zu vollständiger MAC
if (!findFullMacFromShortMac(shortMacStr, mac)) {
  Serial.printf("[WEB] Gerät mit verkürzter MAC %s nicht gefunden.\n", shortMacStr.c_str());
  request->send(400, "text/plain", "Gerät nicht gefunden");
  return;
}

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
// Sende nur die Nachricht an das andere Gerät, aber ändere noch nicht unsere lokale Liste
if (changeOtherDevice(mac, role)) { // Sendet nur Nachricht an das Zielgerät
  // NICHT: changeSavedDevice(mac, role); // Warten auf Identity-Nachricht vom anderen Gerät
  Serial.printf("[WEB] Anfrage an Gerät %s gesendet. Warte auf Bestätigung via Identity-Nachricht.\n", macToString(mac).c_str());
  request->send(200, "text/plain", "Anfrage gesendet, warte auf Bestätigung");
} else {
Serial.println("[WEB] Fehler beim Senden der Nachricht an das Gerät.");
request->send(400, "text/plain", "Failed to send message to device");
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

  server.on("/get_distance_settings", HTTP_GET, [](AsyncWebServerRequest *request)
            {
Serial.println("[WEB] GET /get_distance_settings aufgerufen.");
int minDistance = getMinDistance();
int maxDistance = getMaxDistance();
JsonDocument doc;
doc["minDistance"] = minDistance;
doc["maxDistance"] = maxDistance;
String json;
serializeJson(doc, json);
request->send(200, "application/json", json); });

  server.on("/set_min_distance", HTTP_POST, [](AsyncWebServerRequest *request)
            {
Serial.println("[WEB] POST /set_min_distance aufgerufen.");
if (request->hasParam("minDistance", true)) {
  int minDistance = request->getParam("minDistance", true)->value().toInt();
  setMinDistance(minDistance);
  String msg = "Min-Distanz gesetzt auf " + String(minDistance) + " cm";
  request->send(200, "text/plain", msg);
} else {
  request->send(400, "text/plain", "Fehlender Min-Distanz-Parameter");
} });

  server.on("/set_max_distance", HTTP_POST, [](AsyncWebServerRequest *request)
            {
Serial.println("[WEB] POST /set_max_distance aufgerufen.");
if (request->hasParam("maxDistance", true)) {
  int maxDistance = request->getParam("maxDistance", true)->value().toInt();
  setMaxDistance(maxDistance);
  String msg = "Max-Distanz gesetzt auf " + String(maxDistance) + " cm";
  request->send(200, "text/plain", msg);
} else {
  request->send(400, "text/plain", "Fehlender Max-Distanz-Parameter");
} });

  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request)
            {

Serial.println("[WEB] POST /reset aufgerufen. Lösche alle Preferences.");
resetAll();
request->send(200, "text/plain", "Alle Einstellungen wurden gelöscht. Bitte Gerät neu konfigurieren."); });

  // ESP Neustart-Endpunkt
  server.on("/reset_esp", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    Serial.println("[WEB] POST /reset_esp aufgerufen. Starte ESP neu.");
    request->send(200, "text/plain", "ESP wird neugestartet...");
    delay(500);
    ESP.restart(); });

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

  // Statische Dateien über LittleFS bereitstellen (WICHTIG: Nach allen API-Routen!)
  // Spezifische Dateien explizit mappen
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html", "text/html");
    request->send(response); });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/style.css", "text/css");
    request->send(response); });
  server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/app.js", "application/javascript");
    request->send(response); });
  server.on("/config.css", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/config.css", "text/css");
    request->send(response); });
  server.on("/config.js", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/config.js", "application/javascript");
    request->send(response); });
  server.on("/wsManager.js", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/wsManager.js", "application/javascript");
    request->send(response); });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/favicon.ico", "image/x-icon");
    request->send(response); });

  server.begin();
  Serial.println("[WEB] Webserver gestartet.");
}