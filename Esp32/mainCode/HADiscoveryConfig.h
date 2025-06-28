#ifndef HADiscoveryConfig_h
#define HADiscoveryConfig_h

#include <ArduinoJson.h>
#include "MQTTTopicManager.h"

class HADiscoveryConfig {
public:
    HADiscoveryConfig(MQTTTopicManager& topicManager) : topics(topicManager) {}

    bool sendSensorConfig(const String& location, const String& sensor, 
                        const String& deviceClass, const String& unit, 
                        const String& friendlyName) {
        DynamicJsonDocument doc(1024);  // Augmentation de la taille pour ESP32
        
        //Serial.printf("[Config] Mémoire libre: %d bytes\n", ESP.getFreeHeap());

        doc["name"] = friendlyName;
        if(deviceClass.length() > 0) doc["device_class"] = deviceClass;
        doc["state_topic"] = topics.getTopic(location, sensor, "state");
        if(unit.length() > 0) doc["unit_of_measurement"] = unit;
        doc["unique_id"] = "esp32_" + topics.getMacAddress() + "_" + sensor;

        return sendConfig("sensor", location + "_" + sensor, doc);
    }

    bool sendSwitchConfig(const String& location, const String& switchName, 
                        const String& friendlyName) {
        DynamicJsonDocument doc(1024);  // Augmentation de la taille pour ESP32
        
        doc["name"] = friendlyName;
        doc["command_topic"] = topics.getTopic(location, switchName, "set");
        doc["state_topic"] = topics.getTopic(location, switchName, "state");
        doc["unique_id"] = "esp32_" + topics.getMacAddress() + "_" + switchName;

        return sendConfig("switch", switchName, doc);
    }

    bool sendBinarySensorConfig(const String& location, const String& sensor,
                              const String& deviceClass, const String& friendlyName) {
        DynamicJsonDocument doc(1024);  // Augmentation de la taille pour ESP32
        
        doc["name"] = friendlyName;
        doc["device_class"] = deviceClass;
        doc["state_topic"] = topics.getTopic(location, sensor, "state");
        doc["unique_id"] = "esp32_" + topics.getMacAddress() + "_" + sensor;

        return sendConfig("binary_sensor", location + "_" + sensor, doc);
    }

private:
    MQTTTopicManager& topics;

    bool sendConfig(const String& deviceType, const String& entityName, JsonDocument& config) {
        if(!topics.ensureConnected()) {
            Serial.println("[Config] Impossible de se connecter au broker MQTT");
            return false;
        }
        Serial.println("Statut MQTT: " + String(topics.getClient().state()));
        
        String topic = "homeassistant/" + deviceType + "/" + entityName + "/config";
        String payload;
        serializeJson(config, payload);

        Serial.println("Taille du payload: " + String(payload.length()));
        Serial.println("Memoire libre: " + String(ESP.getFreeHeap()));

        Serial.println("Envoi configuration à: " + topic);
        Serial.println("Payload: " + payload);

        for(int attempt = 0; attempt < 3; attempt++) {
            if(topics.getClient().publish(topic.c_str(), payload.c_str(), true)) {
                Serial.println("Configuration envoyée avec succès");
                delay(100); // Petit délai entre les messages
                return true;
            }
            Serial.println("Échec de l'envoi, tentative " + String(attempt+1));
            delay(500);
            topics.ensureConnected(); // Tentative de reconnexion
        }
        
        Serial.println("Échec après 3 tentatives");
        return false;
    }
};

#endif