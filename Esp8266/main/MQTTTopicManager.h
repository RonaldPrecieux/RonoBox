#ifndef MQTTTopicManager_h
#define MQTTTopicManager_h

#include <ESP8266WiFi.h>
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
        topic += "esp8266-" + macAddress; // Changé esp32 -> esp8266
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
    
    bool ensureConnected() {
    if (!client.connected()) {
        String clientId = "ESP8266Client-" + String(ESP.getChipId());
        if (client.connect(clientId.c_str())) {
            // Resubscribe to necessary topics
            client.subscribe((getBaseTopic() + "/+/set").c_str());
            client.subscribe((getBaseTopic("salon") + "/+/set").c_str());
           // client.subscribe((getBaseTopic("cuisine") + "/+/set").c_str());
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