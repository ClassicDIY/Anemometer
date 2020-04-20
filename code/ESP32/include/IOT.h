#pragma once

#include "WiFi.h"
#include "ArduinoJson.h"
#include "AsyncMqttClient.h"
#include "IotWebConf.h"

#define STR_LEN 64    // general string buffer size
#define CONFIG_LEN 32 // configuration string buffer size

namespace AnemometerNS
{
class IOT
{
public:
    IOT(WebServer* pWebServer);
    void Init();
    void Run();
    void publish(const char *subtopic, const char *value, boolean retained = false);

private:
    bool _clientsConfigured = false;
};
} // namespace AnemometerNS