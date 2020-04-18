#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <WiFiUdp.h>
#include <ThreadController.h>
#include <Thread.h>
#include "Log.h"
#include "Anemometer.h"

using namespace AnemometerNS;

Anemometer _anemometer(AnemometerPin);
AsyncWebServer _server(80);
AsyncWebSocket _ws("/ws");
DNSServer _dnsServer;
WiFiUDP _udp; // A UDP instance to let us send and receive packets over UDP
ThreadController _controller = ThreadController();
Thread *_workerThreadUdpSend = new Thread();
Thread *_workerThreadUdpReceive = new Thread();
Thread *_workerThreadWindMonitor = new Thread();
Thread *_workerThreadWebServer = new Thread();

unsigned int localPort = 2390; // local port to listen for UDP packets
IPAddress timeServerIP;
const char *ntpServerName = "time.nist.gov"; // time.nist.gov NTP server address
const int NTP_PACKET_SIZE = 48;				 // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE];			 //buffer to hold incoming and outgoing packets

unsigned long _epoch = 0; //Unix time in seconds
unsigned long _lastNTP = 0;
int _hours_Offset_From_GMT = -5;
int hour;
int minute;
int second;

unsigned long _lastHighWindTime;
float _highWindSpeed = 0;

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress &address)
{
	logd("sending NTP packet...");
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011; // LI, Version, Mode
	packetBuffer[1] = 0;		  // Stratum, or type of clock
	packetBuffer[2] = 6;		  // Polling Interval
	packetBuffer[3] = 0xEC;		  // Peer Clock Precision
								  // 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;

	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	_udp.beginPacket(address, 123); //NTP requests are to port 123
	_udp.write(packetBuffer, NTP_PACKET_SIZE);
	return _udp.endPacket();
}

void PrintTime()
{
	unsigned long currentTime = _epoch + (millis() - _lastNTP) / 1000;
	// print the hour, minute and second:
	logd("The UTC time is ");		// UTC is the time at Greenwich Meridian (GMT)
	logd((_epoch % 86400L) / 3600); // print the hour (86400 equals secs per day)
	logd(':');
	if (((_epoch % 3600) / 60) < 10)
	{
		// In the first 10 minutes of each hour, we'll want a leading '0'
		logd('0');
	}
	logd((_epoch % 3600) / 60); // print the minute (3600 equals secs per minute)
	logd(':');
	if ((_epoch % 60) < 10)
	{
		// In the first 10 seconds of each minute, we'll want a leading '0'
		logd('0');
	}
	logd(_epoch % 60); // print the second

	//Update for local zone
	currentTime = currentTime + (_hours_Offset_From_GMT * 60 * 60);
	logd("The current local time is ");
	logd((currentTime % 86400L) / 3600); // print the hour (86400 equals secs per day)
	logd(':');
	if (((currentTime % 3600) / 60) < 10)
	{
		// In the first 10 minutes of each hour, we'll want a leading '0'
		logd('0');
	}
	logd((currentTime % 3600) / 60); // print the minute (3600 equals secs per minute)
	logd(':');
	if ((currentTime % 60) < 10)
	{
		// In the first 10 seconds of each minute, we'll want a leading '0'
		logd('0');
	}
	logd(currentTime % 60); // print the second

	hour = (currentTime % 86400L) / 3600;
	if (hour > 12)
	{
		hour -= 12;
	}
	minute = (currentTime % 3600) / 60;
	second = currentTime % 60;
}

void runWindMonitor()
{
	//PrintTime();

	float windSpeed = _anemometer.WindSpeed();
	if (_highWindSpeed < windSpeed)
	{
		_highWindSpeed = windSpeed;
		_lastHighWindTime = _epoch + (millis() - _lastNTP) / 1000;
	}
	//logd("Wind speed: ");
	//logd(windSpeed * 3.6);
	//logd(" Kmh");

	//logd("SensorVoltage: ");
	//logd(_anemometer.SensorVoltage(), 6);
	//logd(" ");
	if (_ws.enabled())
	{
		StaticJsonDocument<128> root;
		String s;
		root["ws"] = windSpeed * 3.6;
		root["hws"] = _highWindSpeed * 3.6;
		root["hwt"] = _lastHighWindTime;
		serializeJson(root, s);
		_ws.textAll(s.c_str());
		logd(s);
	}
}

// void runWebServer()
// {
// 	_dnsServer.processNextRequest();
// 	// server.handleClient();
// 	// webSocket.loop();
// }

void runUdpSend()
{
	//get a random server from the pool
	WiFi.hostByName(ntpServerName, timeServerIP);
	sendNTPpacket(timeServerIP); // send an NTP packet to a time server
}

void runUdpReceive()
{
	int cb = _udp.parsePacket();
	if (cb)
	{
		logd("packet received, length=");
		logd(cb);
		// We've received a packet, read the data from it
		_udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
		//the timestamp starts at byte 40 of the received packet and is four bytes,  or two words, long. First, esxtract the two words:
		unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
		unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
		// combine the four bytes (two words) into a long integer
		// this is NTP time (seconds since Jan 1 1900):
		unsigned long secsSince1900 = highWord << 16 | lowWord;
		logd("Seconds since Jan 1 1900 = ");
		logd(secsSince1900);
		// now convert NTP time into everyday time:
		logd("Unix time = ");
		// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
		const unsigned long seventyYears = 2208988800UL;
		// subtract seventy years:
		_epoch = secsSince1900 - seventyYears;
		_lastNTP = millis();
	}
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
	logd("onWsEvent");
	if (type == WS_EVT_CONNECT)
	{
		logd("ws[%s][%u] connect\n", server->url(), client->id());
		client->printf("Hello Client %u :)", client->id());
		client->ping();
	}
	else if (type == WS_EVT_DISCONNECT)
	{
		logd("ws[%s][%u] disconnect\n", server->url(), client->id());
	}
	else if (type == WS_EVT_ERROR)
	{
		logd("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
	}
	else if (type == WS_EVT_PONG)
	{
		logd("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
	}
	else if (type == WS_EVT_DATA)
	{
		AwsFrameInfo *info = (AwsFrameInfo *)arg;
		String msg = "";
		if (info->final && info->index == 0 && info->len == len)
		{
			//the whole message is in a single frame and we got all of it's data
			logd("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
			if (info->opcode == WS_TEXT)
			{
				for (size_t i = 0; i < info->len; i++)
				{
					msg += (char)data[i];
				}
			}
			else
			{
				char buff[3];
				for (size_t i = 0; i < info->len; i++)
				{
					sprintf(buff, "%02x ", (uint8_t)data[i]);
					msg += buff;
				}
			}
			logd("%s\n", msg.c_str());

			if (info->opcode == WS_TEXT)
				client->text("I got your text message");
			else
				client->binary("I got your binary message");
		}
		else
		{
			//message is comprised of multiple frames or the frame is split into multiple packets
			if (info->index == 0)
			{
				if (info->num == 0)
					logd("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
				logd("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
			}

			logd("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

			if (info->opcode == WS_TEXT)
			{
				for (size_t i = 0; i < len; i++)
				{
					msg += (char)data[i];
				}
			}
			else
			{
				char buff[3];
				for (size_t i = 0; i < len; i++)
				{
					sprintf(buff, "%02x ", (uint8_t)data[i]);
					msg += buff;
				}
			}
			logd("%s\n", msg.c_str());
			if ((info->index + len) == info->len)
			{
				logd("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
				if (info->final)
				{
					logd("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
					if (info->message_opcode == WS_TEXT)
						client->text("I got your text message");
					else
						client->binary("I got your binary message");
				}
			}
		}
	}
}

void notFound(AsyncWebServerRequest *request)
{
	request->send(404, "text/plain", "Not found");
}

const char *ssid = "SkyeNet";
const char *password = "acura22546";
const char *hostName = "esp-async";
const char *http_username = "admin";
const char *http_password = "admin";

void setup()
{
	Serial.begin(115200);
	logd();
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

	logd("WiFi connected");
	logd("IP address: ");
	logd(WiFi.localIP());

	logd("Starting UDP");
	_udp.begin(localPort);
	logd("UDP IP: ");
	logd(_udp.remoteIP());
	// Configure main worker thread
	// _workerThreadUdpSend->onRun(runUdpSend);
	// _workerThreadUdpSend->setInterval(600000);
	// _workerThreadUdpSend->run();
	// _controller.add(_workerThreadUdpSend);
	// _workerThreadUdpReceive->onRun(runUdpReceive);
	// _workerThreadUdpReceive->setInterval(5000);
	// _controller.add(_workerThreadUdpReceive);
	_workerThreadWindMonitor->onRun(runWindMonitor);
	_workerThreadWindMonitor->setInterval(2000);
	_controller.add(_workerThreadWindMonitor);
	// _workerThreadWebServer->onRun(runWebServer);
	// _workerThreadWebServer->setInterval(200);
	// _controller.add(_workerThreadWebServer);

	MDNS.addService("http", "tcp", 80);
	if (!SPIFFS.begin())
	{
		logd("Failed to mount file system");
	}
	else
	{
		logd("File system mounted");
		File root = SPIFFS.open("/");
		File file = root.openNextFile();
		while (file)
		{

			logd("FILE: ");
			logd(file.name());
			file = root.openNextFile();
		}
		_server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");
	}
	_server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", String(ESP.getFreeHeap()));
	});

	_ws.onEvent(onWsEvent);
	_server.addHandler(&_ws);
	_server.onNotFound(notFound);
	_server.begin();
	logd("Setup Done");
}

void loop()
{
	_controller.run();
	_ws.cleanupClients();
}
