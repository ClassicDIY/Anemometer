#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <mdns.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <time.h>
#include <ThreadController.h>
#include <Thread.h>
#include "Log.h"
#include "Anemometer.h"

using namespace AnemometerNS;

Anemometer _anemometer(AnemometerPin);
AsyncWebServer _server(80);
AsyncWebSocket _ws("/ws");
ThreadController _controller = ThreadController();
Thread *_workerThreadWindMonitor = new Thread();

const char *ssid = "SkyeNet";
const char *password = "acura22546";
const char *hostName = "esp-async";
const char *ntpServer = "pool.ntp.org";

unsigned long _epoch = 0; //Unix time in seconds
unsigned long _lastNTP = 0;
unsigned long _lastHighWindTime = 0;
float _highWindSpeed = 0;

void runWindMonitor()
{
	float windSpeed = _anemometer.WindSpeed();
	if (_highWindSpeed < windSpeed)
	{
		_highWindSpeed = windSpeed;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		logi("unixtime: %d", tv.tv_sec);
		_lastHighWindTime = tv.tv_sec;
	}
	if (_ws.enabled())
	{
		StaticJsonDocument<128> root;
		String s;
		float ws = roundf(windSpeed * 3.6 * 10); // convert to km/h and round to 1 decimal place
		ws = ws / 10;
		root["ws"] = ws;
		ws = roundf(_highWindSpeed * 3.6 * 10); // convert to km/h and round to 1 decimal place
		ws = ws / 10;
		root["hws"] = ws;
		root["hwt"] = _lastHighWindTime;
		serializeJson(root, s);
		_ws.textAll(s.c_str());
		logd("JSON: %s", s.c_str());
	}
}

void notFound(AsyncWebServerRequest *request)
{
	loge("Request: %s", request->url());
	request->send(404, "text/plain", "Not found");
}

void setup()
{
	Serial.begin(115200);

	logd("Port Openned");

	//   WiFiManager wifiManager;
	//   //wifiManager.resetSettings(); //reset settings - for testing
	//   wifiManager.setTimeout(180);
	//   if (!wifiManager.autoConnect("AutoConnectAP")) {
	//     logd("failed to connect and hit timeout");
	//     delay(3000);
	//     //reset and try again, or maybe put it to deep sleep
	//     ESP.reset();
	//     delay(5000);
	//   }
	WiFi.softAP(hostName);
	WiFi.begin(ssid, password);
	if (WiFi.waitForConnectResult() != WL_CONNECTED)
	{
		logd("STA: Failed!\n");
		WiFi.disconnect(false);
		delay(1000);
		WiFi.begin(ssid, password);
	}

	logi("WiFi connected");
	logi("IP address: %s", WiFi.localIP().toString().c_str());

	// Configure main worker thread
	_workerThreadWindMonitor->onRun(runWindMonitor);
	_workerThreadWindMonitor->setInterval(2000);
	_controller.add(_workerThreadWindMonitor);
	if (!SPIFFS.begin())
	{
		logd("Failed to mount file system");
	}
	else
	{
		File root = SPIFFS.open("/");
		File file = root.openNextFile();
		while (file)
		{
			logd("FILE: %s", file.name());
			file = root.openNextFile();
		}
	}
	//initialize mDNS service
	esp_err_t err = mdns_init();
	if (err)
	{
		printf("MDNS Init failed: %d\n", err);
	}
	else
	{
		//set hostname
		mdns_hostname_set("Anemometer");
		mdns_instance_name_set("ESP32 Anemometer");
	}
	_server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", String(ESP.getFreeHeap()));
	});
	_server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");
	_server.addHandler(&_ws);
	_server.onNotFound(notFound);
	_server.begin();
	configTime(0, 0, ntpServer);
	logd("Setup Done");
}

void loop()
{
	_controller.run();
	_ws.cleanupClients();
}
