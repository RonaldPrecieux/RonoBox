#ifndef MQTTDevice_h
#define MQTTDevice_h

// Déclaration des includes spécifiques aux plateformes
#ifdef ESP32
  #include <WiFi.h>
#else // ESP8266
  #include <ESP8266WiFi.h>
#endif

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

    // Méthodes begin() identiques pour les deux plateformes
    bool begin(const char* mqttServer, int mqttPort = 1883) {
        mqttClient.setServer(mqttServer, mqttPort);
        mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->mqttCallback(topic, payload, length);
        });
        return reconnect();
    }
    
    bool begin(const IPAddress& mqttServer, int mqttPort = 1883) {
        mqttClient.setServer(mqttServer, mqttPort);
        mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->mqttCallback(topic, payload, length);
        });
        return reconnect();
    }

    void handle() {
        if (!mqttClient.connected()) {
            unsigned long now = millis();
            if (now - lastReconnectAttempt > reconnectInterval) {
                Serial.println("[MQTT] Tentative de reconnexion...");

                if (reconnect()) {
                    Serial.println("[MQTT] Reconnexion réussie !");
                } else {
                    Serial.println("[MQTT] Échec de reconnexion.");
                }

                lastReconnectAttempt = now;
            }
            return;
        }
        mqttClient.loop();
    }

    bool isConnected() {
        return mqttClient.connected();
    }

    // Méthodes de publication unifiées
    void publishSensorData(const String& location, const String& sensor, float value) {
        topicManager.publish(location, sensor, "state", String(value), true);
    }

    void publishSensorData(const String& location, const String& sensor, int value) {
        topicManager.publish(location, sensor, "state", String(value), true);
    }

    void publishSensorData(const String& location, const String& sensor, const String& value) {
        topicManager.publish(location, sensor, "state", value, true);
    }

    void publishSensorData(const String& location, const String& sensor, const bool& value) {
        topicManager.publish(location, sensor, "state", value?"ON":"OFF", true);
    }

    virtual void handleCommand(const String& location, const String& device, const String& value) = 0;

    HADiscoveryConfig& getHAConfig() { return haConfig; }

protected:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    MQTTTopicManager topicManager;
    HADiscoveryConfig haConfig;

private:
    unsigned long lastReconnectAttempt = 0;
    const unsigned long reconnectInterval = 10000;

    bool reconnect() {
        // Génération du client ID différente selon la plateforme
        String clientId = 
        #ifdef ESP32
            "ESP32Client-" + String((uint32_t)ESP.getEfuseMac());
        #else
            "ESP8266Client-" + String(ESP.getChipId());
        #endif

        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("[MQTT] Connecté avec succès !");

            String subscribeTopic = topicManager.getBaseTopic() + "/+/set";
            mqttClient.subscribe(subscribeTopic.c_str());

            subscribeTopic = topicManager.getBaseTopic("salon") + "/+/set";
            mqttClient.subscribe(subscribeTopic.c_str());

            return true;
        } else {
            Serial.printf("[MQTT] Échec connexion. Code = %d\n", mqttClient.state());
            return false;
        }
    }

    void mqttCallback(char* topic, byte* payload, unsigned int length) {
        // Utilisation de buffers statiques pour éviter les allocations dynamiques
        static char topicBuffer[128];
        static char payloadBuffer[128];
        
        // Conversion sécurisée
        strncpy(topicBuffer, topic, sizeof(topicBuffer) - 1);
        topicBuffer[sizeof(topicBuffer) - 1] = '\0';
        
        unsigned int copyLength = min(length, sizeof(payloadBuffer) - 1);
        strncpy(payloadBuffer, (const char*)payload, copyLength);
        payloadBuffer[copyLength] = '\0';

        #ifdef DEBUG
            Serial.printf("[MQTT] Message reçu. Topic: %s, Payload: %s\n", topicBuffer, payloadBuffer);
        #endif

        // Parsing du topic
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

        // Validation des données parsées
        if (!location || !deviceId || !device || !action) {
            #ifdef DEBUG
                Serial.println("[ERREUR] Structure de topic invalide");
            #endif
            return;
        } 

        #ifdef DEBUG
            if (length == 0) {
                Serial.println("[WARNING] Payload vide reçu");
            }
            Serial.printf("[MQTT] Parsed - Location: %s, DeviceID: %s, Device: %s, Action: %s\n", 
                      location, deviceId, device, action ? action : "none");
        #endif

        // Gestion des commandes
        if (action && strcmp(action, "set") == 0) {
            handleCommand(location, device, payloadBuffer);
        } else {
            #ifdef DEBUG
                Serial.printf("[INFO] Message ignoré (action non gérée: %s)\n", action ? action : "null");
            #endif
        }

        // Nettoyage
        memset(topicBuffer, 0, sizeof(topicBuffer));
        memset(payloadBuffer, 0, sizeof(payloadBuffer));
    }
};

#endif