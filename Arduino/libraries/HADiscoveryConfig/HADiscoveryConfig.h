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
        #ifdef ESP32
            DynamicJsonDocument doc(1024);  // Taille plus grande pour ESP32
            doc["unique_id"] = "esp32_" + topics.getMacAddress() + "_" + sensor;
        #else // ESP8266
            DynamicJsonDocument doc(512);   // Taille réduite pour ESP8266
            doc["unique_id"] = "esp8266_" + topics.getMacAddress() + "_" + sensor;
        #endif
        
        doc["name"] = friendlyName;
        if(deviceClass.length() > 0) doc["device_class"] = deviceClass;
        doc["state_topic"] = topics.getTopic(location, sensor, "state");
        if(unit.length() > 0) doc["unit_of_measurement"] = unit;

        return sendConfig("sensor", location + "_" + sensor, doc);
    }
    // Overloaded function for sensors without device class
    bool sendSensorConfig(const String& location, const String& sensor, const String& unit, 
                      const String& friendlyName) {
    return sendSensorConfig(location, sensor, "", unit, friendlyName);
    }


    bool sendSwitchConfig(const String& location, const String& switchName, 
                        const String& friendlyName) {
        #ifdef ESP32
            DynamicJsonDocument doc(1024);
            doc["unique_id"] = "esp32_" + topics.getMacAddress() + "_" + switchName;
        #else // ESP8266
            DynamicJsonDocument doc(512);
            doc["unique_id"] = "esp8266_" + topics.getMacAddress() + "_" + switchName;
        #endif
        
        doc["name"] = friendlyName;
        doc["command_topic"] = topics.getTopic(location, switchName, "set");
        doc["state_topic"] = topics.getTopic(location, switchName, "state");

        return sendConfig("switch", switchName, doc);
    }

    bool sendBinarySensorConfig(const String& location, const String& sensor,
                              const String& deviceClass, const String& friendlyName) {
        #ifdef ESP32
            DynamicJsonDocument doc(1024);
            doc["unique_id"] = "esp32_" + topics.getMacAddress() + "_" + sensor;
        #else // ESP8266
            DynamicJsonDocument doc(512);
            doc["unique_id"] = "esp8266_" + topics.getMacAddress() + "_" + sensor;
        #endif
        
        doc["name"] = friendlyName;
        doc["device_class"] = deviceClass;
        doc["state_topic"] = topics.getTopic(location, sensor, "state");

        return sendConfig("binary_sensor", location + "_" + sensor, doc);
    }

private:
    MQTTTopicManager& topics;

    bool sendConfig(const String& deviceType, const String& entityName, JsonDocument& config) {
        if(!topics.ensureConnected()) {
            Serial.println("[Config] Impossible de se connecter au broker MQTT");
            return false;
        }
        
        String topic = "homeassistant/" + deviceType + "/" + entityName + "/config";
        String payload;
        serializeJson(config, payload);

        #ifdef DEBUG
            Serial.println("Statut MQTT: " + String(topics.getClient().state()));
            Serial.println("Taille du payload: " + String(payload.length()));
            Serial.println("Memoire libre: " + String(ESP.getFreeHeap()));
            Serial.println("Envoi configuration à: " + topic);
            Serial.println("Payload: " + payload);
        #endif

        for(int attempt = 0; attempt < 3; attempt++) {
            if(topics.getClient().publish(topic.c_str(), payload.c_str(), true)) {
                #ifdef DEBUG
                    Serial.println("Configuration envoyée avec succès");
                #endif
                delay(100); // Petit délai entre les messages
                return true;
            }
            #ifdef DEBUG
                Serial.println("Échec de l'envoi, tentative " + String(attempt+1));
            #endif
            delay(500);
            topics.ensureConnected(); // Tentative de reconnexion
        }
        
        #ifdef DEBUG
            Serial.println("Échec après 3 tentatives");
        #endif
        return false;
    }
};

#endif