#ifndef HADiscoveryConfig_h
#define HADiscoveryConfig_h

#include <ArduinoJson.h>
#include "MQTTTopicManager.h"

class HADiscoveryConfig {
public:
    HADiscoveryConfig(MQTTTopicManager& topicManager);

    bool sendSensorConfig(const String& location, const String& sensor, 
                          const String& deviceClass, const String& unit, 
                          const String& friendlyName);

    bool sendSwitchConfig(const String& location, const String& switchName, 
                          const String& friendlyName);

    bool sendBinarySensorConfig(const String& location, const String& sensor,
                                const String& deviceClass, const String& friendlyName);

private:
    MQTTTopicManager& topics;

    bool sendConfig(const String& deviceType, const String& entityName, JsonDocument& config);
};

#endif
