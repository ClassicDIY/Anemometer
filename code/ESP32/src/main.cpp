#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <time.h>
#include <ThreadController.h>
#include <Thread.h>
#include "Log.h"
#include "IOT.h"
#include "Anemometer.h"

using namespace AnemometerNS;

Anemometer _anemometer(AnemometerPin);
WebServer _webServer(80);
WebSocketsServer _webSocket = WebSocketsServer(81);
boolean _wsConnected = false;
ThreadController _controller = ThreadController();
Thread *_workerThreadWindMonitor = new Thread();
IOT _iot = IOT(&_webServer);

unsigned long _epoch = 0; //Unix time in seconds
unsigned long _lastNTP = 0;
unsigned long _lastHighWindTime = 0;
float _highWindSpeed = 0;
float _lastWindSpeed = 0;

void runWindMonitor()
{
	float windSpeed = _anemometer.WindSpeed();
	if (abs(_lastWindSpeed - windSpeed) > AnemometerWindSpeedGranularity) // limit broadcast to AnemometerWindSpeedGranularity km/h changes
	{
		windSpeed = windSpeed <= AnemometerWindSpeedGranularity ? 0 : windSpeed;
		_lastWindSpeed = windSpeed;
		if (_highWindSpeed < windSpeed)
		{
			_highWindSpeed = windSpeed;
			struct timeval tv;
			gettimeofday(&tv, NULL);
			logi("unixtime: %d", tv.tv_sec);
			_lastHighWindTime = tv.tv_sec;
		}
		if (_wsConnected)
		{
			StaticJsonDocument<128> root;
			String s;
			root["ws"] = windSpeed;
			root["hws"] = _highWindSpeed;
			root["hwt"] = _lastHighWindTime;
			serializeJson(root, s);
			_webSocket.broadcastTXT(s.c_str(), s.length());
			logd("Wind Speed: %f JSON: %s", windSpeed, s.c_str());
		}
	}
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t lenght)
{
	switch (type)
	{
	case WStype_DISCONNECTED:
		logd("[%u] Disconnected!\r\n", num);
		_wsConnected = false;
		break;
	case WStype_CONNECTED:
	{
		IPAddress ip = _webSocket.remoteIP(num);
		logd("Client #[%u] connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
		_wsConnected = true;
	}
	break;
	case WStype_ERROR:
		logd("[%u] WStype_ERROR!\r\n", num);
		break;
	default:
		logd("WStype: %u\r\n", type);
		break;
	}
}

void setupFileSystem()
{
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
}

void WiFiEvent(WiFiEvent_t event)
{
	switch (event)
	{
	case SYSTEM_EVENT_STA_GOT_IP:
		// start webSocket server
		_wsConnected = false;
		_webSocket.begin();
		_webSocket.onEvent(webSocketEvent);
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		_webSocket.close();
		break;
	default:
		break;
	}
}

void handleRoot()
{
	File f = SPIFFS.open("/index.htm", "r");
	if (!f)
	{
		logw("index.htm not uploaded to device");
		return;
	}
	_webServer.streamFile(f, "text/html");
}

void setup()
{
	Serial.begin(115200);
	while (!Serial)
	{
		; // wait for serial port to connect.
	}
	// Configure main worker thread
	_workerThreadWindMonitor->onRun(runWindMonitor);
	_workerThreadWindMonitor->setInterval(20);
	_controller.add(_workerThreadWindMonitor);
	setupFileSystem();
	WiFi.onEvent(WiFiEvent);
	_iot.Init();
	_webServer.on("/", handleRoot);
	logd("Setup Done");
}

void loop()
{
	_iot.Run();
	if (WiFi.isConnected())
	{
		_controller.run();
		_webSocket.loop();
	}
}
