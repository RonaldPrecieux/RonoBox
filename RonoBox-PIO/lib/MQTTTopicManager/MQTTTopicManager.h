#ifndef MQTTTopicManager_h
#define MQTTTopicManager_h

// Includes spécifiques aux plateformes
#ifdef ESP32
  #include <WiFi.h>
#else // ESP8266
  #include <ESP8266WiFi.h>
#endif

#include <PubSubClient.h>

class MQTTTopicManager {
public:
    MQTTTopicManager(PubSubClient& mqttClient, const String& deviceMac)
        : client(mqttClient), macAddress(deviceMac) {}

    String getMacAddress() const {
        return macAddress;
    }

    PubSubClient& getClient() {
        return client;
    }

    String getBaseTopic(const String& location = "") {
        String topic = "home/";
        if (location.length() > 0) {
            topic += location + "/";
        }
        #ifdef ESP32
            topic += "esp32-" + macAddress;
        #else
            topic += "esp8266-" + macAddress;
        #endif
        return topic;
    }

    String getTopic(const String& location, const String& device, const String& type) {
        return getBaseTopic(location) + "/" + device + "/" + type;
    }

    bool publish(const String& location, const String& device, const String& type, 
                const String& payload, bool retained = false) {
        String topic = getTopic(location, device, type);
        return client.publish(topic.c_str(), payload.c_str(), retained);
    }

    bool publish(const String& location, const String& device, const String& type, 
                const bool& payload, bool retained = false) {
        String topic = getTopic(location, device, type);
        const char* boolStr = payload ? "true" : "false";
        return client.publish(topic.c_str(), boolStr, retained);
    }
    
    bool ensureConnected() {
        if (!client.connected()) {
            #ifdef ESP32
                String clientId = "ESP32Client-" + String((uint32_t)ESP.getEfuseMac());
            #else
                String clientId = "ESP8266Client-" + String(ESP.getChipId());
            #endif

            if (client.connect(clientId.c_str())) {
                // Resubscribe to necessary topics
                client.subscribe((getBaseTopic() + "/+/set").c_str());
                client.subscribe((getBaseTopic("salon") + "/+/set").c_str());
                #ifdef ESP32
                    client.subscribe((getBaseTopic("cuisine") + "/+/set").c_str());
                #endif
                return true;
            }
            return false;
        }
        return true;
    }

private:
    PubSubClient& client;
    String macAddress;
};

#endif