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

void initWebpage()
{
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
    String json = "{";
    json += "\"selfMac\":\"" + macToShortString(getMacAddress()) + "\",";
    json += "\"selfRole\":\"" + roleToString(getOwnRole()) + "\"";
    json += "}";
    request->send(200, "application/json", json); });

  server.on("/api/last_time", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String json = "{\"lastTime\":" + String(getLastTime()) + "}";
    request->send(200, "application/json", json); });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      loadDeviceListFromPreferences();
      searchForDevices();              
      Serial.println("[WEB] GET /config aufgerufen.");
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/config.html.gz", "text/html");
      response->addHeader("Content-Encoding", "gzip");
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

  // Statische Dateien über LittleFS bereitstellen (WICHTIG: Nach allen API-Routen!)
  // Spezifische Dateien explizit mappen mit korrekten Compression-Headern
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response); });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/style.css.gz", "text/css");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response); });
  server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/app.js.gz", "application/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response); });
  server.on("/config.css", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/config.css.gz", "text/css");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response); });
  server.on("/config.js", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/config.js.gz", "application/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response); });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/favicon.ico.gz", "image/x-icon");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response); });

  server.begin();
  Serial.println("[WEB] Webserver gestartet.");
}