//ConfigManager.h
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

struct NetworkConfig {
  String wifiSSID;
  String wifiPassword;
  String mqttServer;
  int mqttPort;
  String mqttUser;
  String mqttPassword;
};

class ConfigManager {
public:
  ConfigManager();
  bool begin();
  bool isConfigured();
  NetworkConfig getConfig();
  void startAP();
  void handleClient();

private:
  ESP8266WebServer server;
  NetworkConfig config;
  bool apMode = false;

  void handleRoot();
  void handleSave();
  void handleNotFound();
  void loadConfiguration();
  void saveConfiguration();
  void setupServer();
  
  void writeStringToEEPROM(int addr, const String &str);
  String readStringFromEEPROM(int addr);
};

#endif