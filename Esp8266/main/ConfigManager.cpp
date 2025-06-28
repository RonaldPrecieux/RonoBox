//ConfigManager.cpp
#include "ConfigManager.h"
#include <DNSServer.h>

const char* AP_SSID = "SmartHome-Config";
const char* AP_PASSWORD = "configureme";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

DNSServer dnsServer;

ConfigManager::ConfigManager() : server(80) {}

bool ConfigManager::begin() {
  EEPROM.begin(512); // Initialise l'EEPROM avec 512 octets
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

void ConfigManager::writeStringToEEPROM(int addr, const String &str) {
  int maxLen = 100;
  int len = str.length();
  if (len > maxLen) len = maxLen;

  EEPROM.write(addr, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addr + 1 + i, str[i]);
  }

  EEPROM.commit(); // Très important sur ESP32/ESP8266
}


String ConfigManager::readStringFromEEPROM(int addr) {
  int len = EEPROM.read(addr);

  // Vérification de longueur raisonnable (0 à 100)
  if (len <= 0 || len > 100) {
    return "";
  }

  char data[101];  // max 100 caractères + null-terminator
  for (int i = 0; i < len; i++) {
    byte b = EEPROM.read(addr + 1 + i);
    if (b == 0xFF || b == 0x00) { // mémoire vide ou fin de chaîne
      break;
    }
    data[i] = (char)b;
  }
  data[len] = '\0';
  return String(data);
}


void ConfigManager::loadConfiguration() {
  config.wifiSSID = readStringFromEEPROM(0);
  config.wifiSSID.trim();
  if (config.wifiSSID.length() == 0 || config.wifiSSID == "null" || config.wifiSSID == "(null)") {
    config.wifiSSID = "DefaultSSID";
  }

  config.wifiPassword = readStringFromEEPROM(50);
  config.wifiPassword.trim();
  if (config.wifiPassword.length() == 0 || config.wifiPassword == "null") {
    config.wifiPassword = "12345678";
  }

  config.mqttServer = readStringFromEEPROM(100);
  config.mqttServer.trim();
  if (config.mqttServer.length() == 0) {
    config.mqttServer = "192.168.1.10";
  }

  // Reconstruction du port à partir de 2 octets
  int portHigh = EEPROM.read(150);
  int portLow = EEPROM.read(151);
  int port = (portHigh << 8) | portLow;

  // Si port invalide, utiliser le port MQTT standard
  if (port < 1 || port > 65535) {
    config.mqttPort = 1883;
  } else {
    config.mqttPort = port;
  }

  config.mqttUser = readStringFromEEPROM(152);
  config.mqttUser.trim();
  if (config.mqttUser.length() == 0) {
    config.mqttUser = "mqtt_user";
  }

  config.mqttPassword = readStringFromEEPROM(202);
  config.mqttPassword.trim();
  if (config.mqttPassword.length() == 0) {
    config.mqttPassword = "mqtt_pass";
  }

  // Debug clair
  Serial.println("=== Configuration chargée ===");
  Serial.print("SSID: "); Serial.println(config.wifiSSID);
  Serial.print("Password: "); Serial.println(config.wifiPassword);
  Serial.print("MQTT Server: "); Serial.println(config.mqttServer);
  Serial.print("MQTT Port: "); Serial.println(config.mqttPort);
  Serial.print("MQTT User: "); Serial.println(config.mqttUser);
  Serial.print("MQTT Pass: "); Serial.println(config.mqttPassword);
}


void ConfigManager::saveConfiguration() {
  writeStringToEEPROM(0, config.wifiSSID);
  writeStringToEEPROM(50, config.wifiPassword);
  writeStringToEEPROM(100, config.mqttServer);
  EEPROM.write(150, (config.mqttPort >> 8) & 0xFF);
  EEPROM.write(151, config.mqttPort & 0xFF);
  writeStringToEEPROM(152, config.mqttUser);
  writeStringToEEPROM(202, config.mqttPassword);
  
  EEPROM.commit(); // Sauvegarde les modifications
}

// Le reste du code reste identique...
// [Les autres méthodes comme startAP, handleRoot, handleSave, etc. restent inchangées]
bool ConfigManager::isConfigured() {
  return WiFi.status() == WL_CONNECTED;
}

NetworkConfig ConfigManager::getConfig() {
  return config;
}

void ConfigManager::startAP() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  dnsServer.start(53, "*", AP_IP); // Portail captif
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



void ConfigManager::setupServer() {
  server.on("/", HTTP_GET, [this]() { handleRoot(); });
  server.on("/save", HTTP_POST, [this]() { handleSave(); });
  server.onNotFound([this]() { handleNotFound(); });
  server.begin();
}

void ConfigManager::handleRoot() {
  String html = R"=====(
  <!DOCTYPE html>
  <html>
  <head>
    <title>Configuration SmartHome</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body { font-family: Arial; margin: 20px; }
      .container { max-width: 500px; margin: 0 auto; }
      .form-group { margin-bottom: 15px; }
      label { display: block; margin-bottom: 5px; }
      input { width: 100%; padding: 8px; box-sizing: border-box; }
      button { background: #4CAF50; color: white; padding: 10px 15px; border: none; width: 100%; }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Configuration SmartHome</h1>
      <form action="/save" method="post">
        <div class="form-group">
          <label for="ssid">WiFi SSID:</label>
          <input type="text" id="ssid" name="ssid" required value=")=====";
  html += config.wifiSSID;
  html += R"=====(">
        </div>
        
        <div class="form-group">
          <label for="pass">WiFi Password:</label>
          <input type="password" id="pass" name="pass" value=")=====";
  html += config.wifiPassword;
  html += R"=====(">
        </div>
        
        <div class="form-group">
          <label for="mqtt">MQTT Server:</label>
          <input type="text" id="mqtt" name="mqtt" required value=")=====";
  html += config.mqttServer;
  html += R"=====(">
        </div>
        
        <div class="form-group">
          <label for="port">MQTT Port:</label>
          <input type="number" id="port" name="port" value=")=====";
  html += String(config.mqttPort);
  html += R"=====(">
        </div>
        
        <div class="form-group">
          <label for="muser">MQTT User:</label>
          <input type="text" id="muser" name="muser" value=")=====";
  html += config.mqttUser;
  html += R"=====(">
        </div>
        
        <div class="form-group">
          <label for="mpass">MQTT Password:</label>
          <input type="password" id="mpass" name="mpass" value=")=====";
  html += config.mqttPassword;
  html += R"=====(">
        </div>
        
        <button type="submit">Enregistrer</button>
      </form>
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
  
  String html = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='10;url=/'></head><body>";
  html += "<h1>Configuration sauvegardee!</h1>";
  html += "<p>Redemarrage dans 10 secondes...</p>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
  delay(1000);
  ESP.restart();
}

void ConfigManager::handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}


void ConfigManager::resetConfiguration() {
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0xFF); // Optionnellement 0x00 selon ton design
  }

  EEPROM.commit(); // Très important sur ESP32 / ESP8266

  Serial.println("✅ Configuration EEPROM réinitialisée !");
}
