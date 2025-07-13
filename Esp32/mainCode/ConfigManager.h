#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

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
  Preferences preferences;
  WebServer server;
  NetworkConfig config;
  bool apMode = false;

  void handleRoot();
  void handleSave();
  void handleNotFound();
  void loadConfiguration();
  void saveConfiguration();
  void setupServer();
  void resetConfiguration();
  void handleReset();
};

#endif