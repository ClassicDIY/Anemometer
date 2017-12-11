
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <FS.h>
#include <WiFiUdp.h>
#include <ThreadController.h>
#include <Thread.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <Anemometer.h>

// Don't forget to click vMicro->'Publish Server Data Files' to upload the index.html file for the initial setup of the NodeMCU

IPAddress apIP(192, 168, 100, 4);

Anemometer _anemometer(A0, .0029343066);
WebSocketsServer webSocket = WebSocketsServer(81);
boolean _wsConnected = false;
ESP8266WebServer server(80);
const byte        DNS_PORT = 53;
DNSServer         dnsServer;
const char *myHostname = "anemometer";  //hostname for mDNS. http ://anemometer.local

ThreadController _controller = ThreadController();
Thread* _workerThreadUdpSend = new Thread();
Thread* _workerThreadUdpReceive = new Thread();
Thread* _workerThreadWindMonitor = new Thread();
Thread* _workerThreadWebServer = new Thread();

unsigned int localPort = 2390; // local port to listen for UDP packets
IPAddress timeServerIP; 
const char* ntpServerName = "time.nist.gov"; // time.nist.gov NTP server address
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP _udp; // A UDP instance to let us send and receive packets over UDP
unsigned long _epoch = 0; //Unix time in seconds
unsigned long _lastNTP = 0;
int _hours_Offset_From_GMT = -5;
int hour;
int minute;
int second;

unsigned long _lastHighWindTime;
float _highWindSpeed = 0;

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
	Serial.println("sending NTP packet...");
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
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
  Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
  Serial.print((_epoch % 86400L) / 3600); // print the hour (86400 equals secs per day)
  Serial.print(':');
  if (((_epoch % 3600) / 60) < 10) {
    // In the first 10 minutes of each hour, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.print((_epoch % 3600) / 60); // print the minute (3600 equals secs per minute)
  Serial.print(':');
  if ((_epoch % 60) < 10) {
    // In the first 10 seconds of each minute, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.println(_epoch % 60); // print the second

                //Update for local zone
  currentTime = currentTime + (_hours_Offset_From_GMT * 60 * 60);
  Serial.print("The current local time is ");
  Serial.print((currentTime % 86400L) / 3600); // print the hour (86400 equals secs per day)
  Serial.print(':');
  if (((currentTime % 3600) / 60) < 10) {
    // In the first 10 minutes of each hour, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.print((currentTime % 3600) / 60); // print the minute (3600 equals secs per minute)
  Serial.print(':');
  if ((currentTime % 60) < 10) {
    // In the first 10 seconds of each minute, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.println(currentTime % 60); // print the second

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
  if (_highWindSpeed < windSpeed) {
    _highWindSpeed = windSpeed;
    _lastHighWindTime = _epoch + (millis() - _lastNTP) / 1000;
  }
	//Serial.print("Wind speed: ");
	//Serial.print(windSpeed * 3.6);
	//Serial.println(" Kmh");

	//Serial.print("SensorVoltage: ");
	//Serial.print(_anemometer.SensorVoltage(), 6);
	//Serial.println(" ");
    if (_wsConnected) {
  		StaticJsonBuffer<64> jsonBuffer;
  		JsonObject& root = jsonBuffer.createObject();
  		root["ws"] = windSpeed * 3.6;
  		root["hws"] = _highWindSpeed * 3.6;
  		root["hwt"] = _lastHighWindTime;
  		char buffer[64];
  		root.printTo(buffer, 64);
  		webSocket.broadcastTXT(buffer, strlen(buffer));
  		//root.printTo(Serial);
		}
}

void runWebServer()
{
	dnsServer.processNextRequest();
	server.handleClient();
	webSocket.loop();
}

void runUdpSend()
{
	//get a random server from the pool
	WiFi.hostByName(ntpServerName, timeServerIP);
	sendNTPpacket(timeServerIP); // send an NTP packet to a time server
}

void runUdpReceive()
{
	int cb = _udp.parsePacket();
	if (cb) {
		Serial.print("packet received, length=");
		Serial.println(cb);
		// We've received a packet, read the data from it
		_udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
		//the timestamp starts at byte 40 of the received packet and is four bytes,  or two words, long. First, esxtract the two words:
		unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
		unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
		// combine the four bytes (two words) into a long integer
		// this is NTP time (seconds since Jan 1 1900):
		unsigned long secsSince1900 = highWord << 16 | lowWord;
		Serial.print("Seconds since Jan 1 1900 = ");
		Serial.println(secsSince1900);
		// now convert NTP time into everyday time:
		Serial.print("Unix time = ");
		// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
		const unsigned long seventyYears = 2208988800UL;
		// subtract seventy years:
		_epoch = secsSince1900 - seventyYears;
		_lastNTP = millis();
	}
}

void printFSInfo(){
  FSInfo fsinfo;
  if(SPIFFS.info(fsinfo)){
    Serial.print("FS total bytes: ");
    Serial.println(fsinfo.totalBytes);
    Serial.print("FS used bytes: ");
    Serial.println(fsinfo.usedBytes);
  }
}

String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

void setupFileSystem(){
  //file system
  if(!SPIFFS.begin()){
    Serial.println("Failed to mount file system");
  }
  else {
    Serial.println("File system mounted");
    printFSInfo();
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}

void setupDNS() {
	//DNS
	// if DNSServer is started with "*" for domain name, it will reply with
	// provided IP to all DNS request
	dnsServer.start(DNS_PORT, "*", apIP);

	// Setup MDNS responder
	if (!MDNS.begin(myHostname)) {
		Serial.println("Error setting up MDNS responder!");
	}
	else {
		Serial.println("mDNS responder started");
		// Add service to MDNS-SD
		MDNS.addService("http", "tcp", 80);
		MDNS.addService("ws", "tcp", 81);
	}
}

String getContentType(String filename) {
	if (server.hasArg("download")) return "application/octet-stream";
	else if (filename.endsWith(".htm")) return "text/html";
	else if (filename.endsWith(".html")) return "text/html";
	else if (filename.endsWith(".css")) return "text/css";
	else if (filename.endsWith(".js")) return "application/javascript";
	else if (filename.endsWith(".png")) return "image/png";
	else if (filename.endsWith(".gif")) return "image/gif";
	else if (filename.endsWith(".jpg")) return "image/jpeg";
	else if (filename.endsWith(".ico")) return "image/x-icon";
	else if (filename.endsWith(".xml")) return "text/xml";
	else if (filename.endsWith(".pdf")) return "application/x-pdf";
	else if (filename.endsWith(".zip")) return "application/x-zip";
	else if (filename.endsWith(".gz")) return "application/x-gzip";
	return "text/plain";

}
void handleFileList() {
	if (!server.hasArg("dir")) { server.send(500, "text/plain", "BAD ARGS"); return; }

	String path = server.arg("dir");
	Serial.println("handleFileList: " + path);
	Dir dir = SPIFFS.openDir(path);
	path = String();

	String output = "[";
	while (dir.next()) {
		File entry = dir.openFile("r");
		if (output != "[") output += ',';
		bool isDir = false;
		output += "{\"type\":\"";
		output += (isDir) ? "dir" : "file";
		output += "\",\"name\":\"";
		output += String(entry.name()).substring(1);
		output += "\"}";
		entry.close();
	}

	output += "]";
	server.send(200, "text/json", output);
}

bool handleFileRead(String path) {
	Serial.println("handleFileRead: " + path);
	if (path.endsWith("/")) path += "index.html";
	String contentType = getContentType(path);
	String pathWithGz = path + ".gz";
	if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
		if (SPIFFS.exists(pathWithGz))
			path += ".gz";
		File file = SPIFFS.open(path, "r");
		size_t sent = server.streamFile(file, contentType);
		file.close();
		return true;
	}
	return false;
}

void setupWebServer() {
	//list directory
	server.on("/list", HTTP_GET, handleFileList);
	//home page
	server.on("/", HTTP_GET, []() {
		if (!handleFileRead("/index.html")) server.send(404, "text/plain", "FileNotFound");
	});

	server.begin();
	Serial.println("HTTP server started");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

	switch (type) {
	case WStype_DISCONNECTED:
		Serial.printf("[%u] Disconnected!\r\n", num);
		_wsConnected = false;
		break;
	case WStype_CONNECTED: 
		{
			IPAddress ip = webSocket.remoteIP(num);
			Serial.printf("Client #[%u] connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
			_wsConnected = true;
		}
		break;
	case WStype_TEXT:
		Serial.printf("Client #[%u] sent message: %s\r\n", num, payload);

		if (payload[0] == '#') {
			// we get rotation data
			// decode rotation data
			uint32_t rotation = (uint32_t)strtol((const char *)&payload[1], NULL, 16);

			Serial.printf("Get rotation value: [%u]\r\n", rotation);
		}
		break;

	case WStype_ERROR:
		Serial.printf("[%u] WStype_ERROR!\r\n", num);
		break;
	}
}

void setupWebSockets() {
	// start webSocket server
	_wsConnected = false;
	webSocket.begin();
	webSocket.onEvent(webSocketEvent);
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Port Openned");

  WiFiManager wifiManager;
  //wifiManager.resetSettings(); //reset settings - for testing
  wifiManager.setTimeout(180);
  if (!wifiManager.autoConnect("AutoConnectAP")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  _udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(_udp.localPort());
  // Configure main worker thread
  _workerThreadUdpSend->onRun(runUdpSend);
  _workerThreadUdpSend->setInterval(600000);
  _workerThreadUdpSend->run();
  _controller.add(_workerThreadUdpSend);
  _workerThreadUdpReceive->onRun(runUdpReceive);
  _workerThreadUdpReceive->setInterval(5000);
  _controller.add(_workerThreadUdpReceive);
  _workerThreadWindMonitor->onRun(runWindMonitor);
  _workerThreadWindMonitor->setInterval(500);
  _controller.add(_workerThreadWindMonitor);
  _workerThreadWebServer->onRun(runWebServer);
  _workerThreadWebServer->setInterval(200);
  _controller.add(_workerThreadWebServer);
  
  setupFileSystem();
  setupWebServer();
  setupWebSockets();
  setupDNS();
  Serial.println("Setup Done");
  
}

void loop()
{
  _controller.run();

}
