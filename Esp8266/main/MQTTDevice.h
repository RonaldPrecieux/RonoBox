#ifndef MQTTDevice_h
#define MQTTDevice_h

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "MQTTTopicManager.h"
#include "HADiscoveryConfig.h"

class MQTTDevice {
public:
    MQTTDevice(const String& macAddress)
        : wifiClient(), 
          mqttClient(wifiClient), 
          topicManager(mqttClient, macAddress),
          haConfig(topicManager) {}

    void begin(const char* mqttServer, int mqttPort = 1883) {
       
        mqttClient.setServer(mqttServer, mqttPort);
        mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->mqttCallback(topic, payload, length);
        });
    }

    void handle() {
        if (!mqttClient.connected()) {
            reconnect();
        }
        mqttClient.loop();
    }

    // Modification ici : suppression du 'const' pour la méthode isConnected()
    bool isConnected() {
        return mqttClient.connected();
    }

    void publishSensorData(const String& location, const String& sensor, float value) {
        topicManager.publish(location, sensor, "state", String(value), true);
    }

    void publishSensorData(const String& location, const String& sensor, int value) {
        topicManager.publish(location, sensor, "state", String(value), true);
    }

    void publishSensorData(const String& location, const String& sensor, const String& value) {
        topicManager.publish(location, sensor, "state", value, true);
    }

    virtual void handleCommand(const String& location, const String& device, const String& value) = 0;

    HADiscoveryConfig& getHAConfig() { return haConfig; }

protected:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    MQTTTopicManager topicManager;
    HADiscoveryConfig haConfig;

private:
    void reconnect() {
        while (!mqttClient.connected()) {
            String clientId = "ESP8266Client-" + String(ESP.getChipId());
            
            if (mqttClient.connect(clientId.c_str())) {
                // Abonnements aux topics
                String subscribeTopic = topicManager.getBaseTopic() + "/+/set";
                mqttClient.subscribe(subscribeTopic.c_str());
                
                subscribeTopic = topicManager.getBaseTopic("salon") + "/+/set";
                mqttClient.subscribe(subscribeTopic.c_str());
            } else {
                delay(5000);
            }
        }
    }

    // void mqttCallback(char* topic, byte* payload, unsigned int length) {
    //     String topicStr = topic;
    //     String payloadStr;
    //     payloadStr.reserve(length);
        
    //     for (unsigned int i = 0; i < length; i++) {
    //         payloadStr += (char)payload[i];
    //     }

    //     // Extraction de la localisation et du dispositif
    //     int locStart = topicStr.indexOf("home/") + 5;
    //     int locEnd = topicStr.indexOf('/', locStart);
    //     String location = (locStart > 0 && locEnd > 0) ? topicStr.substring(locStart, locEnd) : "";
        
    //     int devStart = locEnd + 1;
    //     int devEnd = topicStr.indexOf('/', devStart);
    //     String device = (devStart > 0 && devEnd > 0) ? topicStr.substring(devStart, devEnd) : "";

    //     if (location.length() > 0 && device.length() > 0) {
    //         handleCommand(location, device, payloadStr);
    //     }
    // }

    void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // 1. Optimisation: Utilisation de buffers statiques pour éviter les allocations dynamiques
    static char topicBuffer[128];
    static char payloadBuffer[128];
    
    // 2. Conversion sécurisée du topic et du payload
    strncpy(topicBuffer, topic, sizeof(topicBuffer) - 1);
    topicBuffer[sizeof(topicBuffer) - 1] = '\0';
    
    unsigned int copyLength = min(length, sizeof(payloadBuffer) - 1);
    strncpy(payloadBuffer, (const char*)payload, copyLength);
    payloadBuffer[copyLength] = '\0';

    // 3. Journalisation détaillée
    Serial.printf("[MQTT] Message reçu. Topic: %s, Payload: %s\n", topicBuffer, payloadBuffer);

    // 4. Parsing plus robuste du topic
    
    // Format attendu: home/[location]/[deviceId]/[device]/[action]
    

    char* location = nullptr;
    char* deviceId = nullptr;
    char* device = nullptr;
    char* action = nullptr;

    char* token = strtok(topicBuffer, "/");
    int part = 0;

    while (token != nullptr) {
        if (strcmp(token, "home") == 0) {
            part = 1;
        } else if (part == 1) {
            location = token;
            part = 2;
        } else if (part == 2) {
            deviceId = token;
            part = 3;
        } else if (part == 3) {
            device = token;
            part = 4;
        } else if (part == 4) {
            action = token;
            break;
        }
        token = strtok(nullptr, "/");
    }


    // 5. Validation des données parsées
    if (!location || !deviceId || !device || !action) {
    Serial.println("[ERREUR] Structure de topic invalide");
    return;
    } 


    // 6. Vérification du payload
    if (length == 0) {
        Serial.println("[WARNING] Payload vide reçu");
        // Potentiellement valide pour certaines commandes
    }

    // 7. Journalisation du parsing
    Serial.printf("[MQTT] Parsed - Location: %s, DeviceID: %s, Device: %s, Action: %s\n", 
              location, deviceId, device, action ? action : "none");

    // 8. Gestion des commandes avec vérification de l'action
    if (action && strcmp(action, "set") == 0) {
        handleCommand(location, device, payloadBuffer);
    } else {
        Serial.printf("[INFO] Message ignoré (action non gérée: %s)\n", action ? action : "null");
    }

    // 9. Nettoyage (optionnel)
    memset(topicBuffer, 0, sizeof(topicBuffer));
    memset(payloadBuffer, 0, sizeof(payloadBuffer));
  }
};

#endif