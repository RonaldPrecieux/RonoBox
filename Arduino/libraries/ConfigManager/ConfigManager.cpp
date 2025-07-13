// ConfigManager.cpp
#include "ConfigManager.h"

ConfigManager::ConfigManager() : server(80) {}

bool ConfigManager::begin() {
  #ifdef ESP32
    preferences.begin("smart-home", false);
  #else
    EEPROM.begin(512);
  #endif

  loadConfiguration();

  if (config.wifiSSID.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSSID.c_str(), config.wifiPassword.c_str());
    
    Serial.print("Connexion WiFi");
    unsigned long startTime = millis();
    
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnecté!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return true;
    }
    
    Serial.println("\nÉchec de connexion WiFi");
  }

  startAP();
  return false;
}

bool ConfigManager::isConfigured() {
  return WiFi.status() == WL_CONNECTED;
}

NetworkConfig ConfigManager::getConfig() {
  return config;
}

void ConfigManager::setupServer() {
  server.on("/", HTTP_GET, [this]() { handleRoot(); });
  server.on("/save", HTTP_POST, [this]() { handleSave(); });
  server.on("/reset", HTTP_POST, [this]() { handleReset(); });
  server.onNotFound([this]() { handleNotFound(); });
  server.begin();
}
void ConfigManager::startAP() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  dnsServer.start(53, "*", AP_IP);
  setupServer();
  
  Serial.println("\nMode AP activé");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
}

void ConfigManager::handleClient() {
  if (apMode) {
    dnsServer.processNextRequest();
    server.handleClient();
  }
}

void ConfigManager::loadConfiguration() {
  #ifdef ESP32
    config.wifiSSID = preferences.getString("wifi_ssid", "");
    config.wifiPassword = preferences.getString("wifi_pass", "");
    config.mqttServer = preferences.getString("mqtt_server", "");
    config.mqttPort = preferences.getInt("mqtt_port", 1883);
    config.mqttUser = preferences.getString("mqtt_user", "");
    config.mqttPassword = preferences.getString("mqtt_pass", "");
  #else
    config.wifiSSID = readStringFromEEPROM(0);
    config.wifiPassword = readStringFromEEPROM(50);
    config.mqttServer = readStringFromEEPROM(100);
    
    int portHigh = EEPROM.read(150);
    int portLow = EEPROM.read(151);
    config.mqttPort = (portHigh << 8) | portLow;
    if (config.mqttPort < 1 || config.mqttPort > 65535) {
      config.mqttPort = 1883;
    }
    
    config.mqttUser = readStringFromEEPROM(152);
    config.mqttPassword = readStringFromEEPROM(202);
  #endif

  // Debug
  Serial.println("=== Configuration chargée ===");
  Serial.print("SSID: "); Serial.println(config.wifiSSID);
  Serial.print("MQTT Server: "); Serial.println(config.mqttServer);
}

void ConfigManager::saveConfiguration() {
  #ifdef ESP32
    preferences.putString("wifi_ssid", config.wifiSSID);
    preferences.putString("wifi_pass", config.wifiPassword);
    preferences.putString("mqtt_server", config.mqttServer);
    preferences.putInt("mqtt_port", config.mqttPort);
    preferences.putString("mqtt_user", config.mqttUser);
    preferences.putString("mqtt_pass", config.mqttPassword);
  #else
    writeStringToEEPROM(0, config.wifiSSID);
    writeStringToEEPROM(50, config.wifiPassword);
    writeStringToEEPROM(100, config.mqttServer);
    EEPROM.write(150, (config.mqttPort >> 8) & 0xFF);
    EEPROM.write(151, config.mqttPort & 0xFF);
    writeStringToEEPROM(152, config.mqttUser);
    writeStringToEEPROM(202, config.mqttPassword);
    EEPROM.commit();
  #endif
}

void ConfigManager::resetConfiguration() {
  #ifdef ESP32
    preferences.clear();
  #else
    for (int i = 0; i < 512; i++) {
      EEPROM.write(i, 0xFF);
    }
    EEPROM.commit();
  #endif
  config = NetworkConfig();
}

#ifdef ESP8266
void ConfigManager::writeStringToEEPROM(int addr, const String &str) {
  int len = str.length();
  EEPROM.write(addr, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addr + 1 + i, str[i]);
  }
}

String ConfigManager::readStringFromEEPROM(int addr) {
  int len = EEPROM.read(addr);
  if (len <= 0 || len > 100) return "";
  
  char data[101];
  for (int i = 0; i < len; i++) {
    data[i] = EEPROM.read(addr + 1 + i);
  }
  data[len] = '\0';
  return String(data);
}
#endif


void ConfigManager::handleRoot() {
  String html = R"=====(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <title>Configuration SmartHome</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body { font-family: Arial; margin: 20px; }
      .container { max-width: 500px; margin: 0 auto; }
      .form-group { margin-bottom: 15px; }
      label { display: block; margin-bottom: 5px; }
      input { width: 100%; padding: 8px; box-sizing: border-box; }
      button { background: #4CAF50; color: white; padding: 10px 15px; border: none; width: 100%; }
      .reset-btn { background: #f44336; margin-top: 20px; }
      .form-section { background: #f9f9f9; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
      h2 { margin-top: 0; color: #333; }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Configuration SmartHome</h1>
      
      <div class="form-section">
        <h2>Paramètres WiFi</h2>
        <form action="/save" method="post">
          <div class="form-group">
            <label for="ssid">SSID WiFi:</label>
            <input type="text" id="ssid" name="ssid" required value=")=====";
  html += config.wifiSSID;
  html += R"=====(">
          </div>
          
          <div class="form-group">
            <label for="pass">Mot de passe WiFi:</label>
            <input type="password" id="pass" name="pass" value=")=====";
  html += config.wifiPassword;
  html += R"=====(">
          </div>
      </div>
      
     
          
          <div class="form-group">
            <label for="muser">Utilisateur MQTT:</label>
            <input type="text" id="muser" name="muser" value=")=====";
  html += config.mqttUser;
  html += R"=====(">
          </div>
          
          <div class="form-group">
            <label for="mpass">Mot de passe MQTT:</label>
            <input type="password" id="mpass" name="mpass" value=")=====";
  html += config.mqttPassword;
  html += R"=====(">
          </div>
          
          <button type="submit">Enregistrer</button>
        </form>
      </div>
      
      <div class="form-section">
        <h2>Réinitialisation</h2>
        <p>Ceci effacera tous les paramètres et redémarrera l'appareil.</p>
        <form action="/reset" method="post" onsubmit="return confirm('Êtes-vous sûr de vouloir réinitialiser tous les paramètres?');">
          <button type="submit" class="reset-btn">Réinitialiser la configuration</button>
        </form>
      </div>
    </div>
  </body>
  </html>
  )=====";

  server.send(200, "text/html", html);
}

void ConfigManager::handleSave() {
  config.wifiSSID = server.arg("ssid");
  config.wifiPassword = server.arg("pass");
  config.mqttServer = server.arg("mqtt");
  config.mqttPort = server.arg("port").toInt();
  config.mqttUser = server.arg("muser");
  config.mqttPassword = server.arg("mpass");
  
  saveConfiguration();
  
  String html = "<!DOCTYPE html><html><head><meta http-equiv='refresh' charset='UTF-8' content='10;url=/'></head><body>";
  html += "<h1>Configuration sauvegardée!</h1>";
  html += "<p>Redémarrage dans 10 secondes...</p>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
  delay(1000);
  ESP.restart();
}

void ConfigManager::handleReset() {
  resetConfiguration();
  
  String html = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='10;url=/'></head><body>";
  html += "<h1>Configuration réinitialisée!</h1>";
  html += "<p>Redémarrage dans 10 secondes...</p>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
  delay(1000);
  ESP.restart();
}


void ConfigManager::handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// [Les méthodes handleRoot, handleSave, handleNotFound restent identiques à la version ESP32]