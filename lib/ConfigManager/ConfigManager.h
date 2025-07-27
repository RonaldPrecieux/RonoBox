// ConfigManager.h
#ifndef ConfigManager_h
#define ConfigManager_h

#include <Arduino.h>
#include <DNSServer.h>

#ifdef ESP32
  #include <Preferences.h>
  #include <WiFi.h>
  #include <WebServer.h>
  #define WEBSERVER_TYPE WebServer
#else // ESP8266
  #include <EEPROM.h>
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #define WEBSERVER_TYPE ESP8266WebServer
#endif

struct NetworkConfig {
  String wifiSSID;
  String wifiPassword;
  String mqttServer;
  int mqttPort = 1883;
  String mqttUser;
  String mqttPassword;
};

class ConfigManager {
public:
    ConfigManager();
    bool begin();
    bool isConfigured();
    NetworkConfig getConfig();
    void handleClient();
    void resetConfiguration();

private:
    void startAP();
    void loadConfiguration();
    void saveConfiguration();
    void setupServer();
    void handleRoot();
    void handleSave();
    void handleNotFound();
    void handleReset();


    #ifdef ESP8266
        void writeStringToEEPROM(int addr, const String &str);
        String readStringFromEEPROM(int addr);
    #endif

    NetworkConfig config;
    bool apMode = false;
    WEBSERVER_TYPE server;  // Changed from WebServer to WEBSERVER_TYPE
    DNSServer dnsServer;

    #ifdef ESP32
        Preferences preferences;
    #endif

    const char* AP_SSID = "SmartHome-Config";
    const char* AP_PASSWORD = "configureme";
    const IPAddress AP_IP = IPAddress(192, 168, 4, 1);
    const IPAddress AP_SUBNET = IPAddress(255, 255, 255, 0);
};

#endif