/****
* Solar panel monitor - a learning project, monitors sensors and send to server
* Esp8266 monitoring station module
* Created 2017 by Ngo Phuong Le
* https://github.com/lehn85/slm-monitoring-station-esp8266
* All files are provided under the MIT license.
****/

#include <network_setup.h>
#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <AppSettings.h>

HttpServer server;
FTPServer ftp;

BssList networks;
String network, password;
Timer connectionTimer;
enum ConnectionStatus {
	Not_Connected = 0,
	Connecting = 1,
	Connected = 2,
	ConnectionFailed = 3
};
ConnectionStatus connStatus = ConnectionStatus::Not_Connected;

//declare functions
void networkScanCompleted(bool succeeded, BssList list);
void onSTAconnected();
void onSTAfailed();

// Handle a preflight request
// Return true if it is a preflight request and is handled
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Access_control_CORS#Preflighted_requests
bool handlePreflight(HttpRequest &request, HttpResponse &response) {
	if (request.getRequestMethod() == "OPTIONS")
	{
		debugf("Preflight request. Response OK.");
		response.setHeader("Access-Control-Allow-Origin", "*");
		response.setHeader("Access-Control-Allow-Headers", "Content-Type");
		response.setHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
		response.sendString("");
		return true;
	}

	return false;
}

void onIndex(HttpRequest &request, HttpResponse &response)
{
	response.sendFile("index.html");
}

void onSetIp(HttpRequest &request, HttpResponse &response)
{
	if (handlePreflight(request, response))
		return;

	if (request.getRequestMethod() == RequestMethod::POST)
	{
		DynamicJsonBuffer jsonBuffer;
		JsonObject& jsonIn = jsonBuffer.parseObject(request.getBody());
		String ip = jsonIn["ip"];
		String subnetmask = jsonIn["subnetmask"];
		String gateway = jsonIn["gateway"];

		AppSettings.dhcp = jsonIn["dhcp"];// request.getPostParameter("dhcp") == "1";		
		AppSettings.ip = IPAddress(ip); //request.getPostParameter("ip");
		AppSettings.netmask = IPAddress(subnetmask);// request.getPostParameter("netmask");
		AppSettings.gateway = IPAddress(gateway);//request.getPostParameter("gateway");
		debugf("Updating IP settings: dhcp=%d, ip=%s, subnetmask=%s, gateway=%s", AppSettings.dhcp, ip.c_str(), subnetmask.c_str(), gateway.c_str());
		AppSettings.save();

		// belows code can only be run in init(), meaning require device reboot
		//if (!AppSettings.dhcp && !AppSettings.ip.isNull())
		//	WifiStation.setIP(AppSettings.ip, AppSettings.netmask, AppSettings.gateway);

		response.setHeader("Access-Control-Allow-Origin", "*");
		response.sendString("OK");
	}
	else {
		response.badRequest();
	}
}

void onGetIp(HttpRequest &request, HttpResponse &response) {
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	bool connected = WifiStation.isConnected();
	json["enable"] = WifiStation.isEnabled();
	json["connectionStatus"] = (int)connStatus;
	json["connected"] = connected;

	if (connected) {
		json["ssid"] = WifiStation.getSSID();
		json["dhcp"] = WifiStation.isEnabledDHCP();
		json["signal"] = WifiStation.getRssi();
		if (!WifiStation.getIP().isNull())
		{
			json["ip"] = WifiStation.getIP().toString();
			json["subnetmask"] = WifiStation.getNetworkMask().toString();
			json["gateway"] = WifiStation.getNetworkGateway().toString();
		}
		else {
			json["ip"] = "";
			json["subnetmask"] = "";
			json["gateway"] = "";
		}
	}

	response.setAllowCrossDomainOrigin("*");
	response.sendJsonObject(stream); // will be automatically deleted
}

void onFile(HttpRequest &request, HttpResponse &response)
{
	String file = request.getPath();
	if (file[0] == '/')
		file = file.substring(1);

	if (file[0] == '.')
		response.forbidden();
	else
	{
		response.setCache(86400, true); // It's important to use cache for better performance.
		response.sendFile(file);
	}
}

String getBSSID(const BssInfo &bssi)
{
	String bssid;
	for (int i = 0; i < 6; i++)
	{
		if (bssi.bssid[i] < 0x10) bssid += "0";
		bssid += String(bssi.bssid[i], HEX);
	}
	return bssid;
}

void onApiNetworkList(HttpRequest &request, HttpResponse &response)
{
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	bool connected = WifiStation.isConnected();
	json["enable"] = WifiStation.isEnabled();
	json["connectionStatus"] = (int)connStatus;
	json["connected"] = connected;

	// connected AP info
	if (connected)
	{
		JsonObject& nw = json.createNestedObject("network");
		nw["ssid"] = WifiStation.getSSID();
		nw["signal"] = WifiStation.getRssi();
		nw["dhcp"] = WifiStation.isEnabledDHCP();
		if (!WifiStation.getIP().isNull())
		{
			nw["ip"] = WifiStation.getIP().toString();
			nw["subnetmask"] = WifiStation.getNetworkMask().toString();
			nw["gateway"] = WifiStation.getNetworkGateway().toString();
		}
		else {
			nw["ip"] = "";
			nw["subnetmask"] = "";
			nw["gateway"] = "";
		}
	}

	// available list
	JsonArray& netlist = json.createNestedArray("available");
	for (int i = 0; i < networks.count(); i++)
	{
		if (networks[i].hidden) continue;
		JsonObject &item = netlist.createNestedObject();
		item["id"] = (int)networks[i].getHashId();
		// Copy full string to JSON buffer memory
		item["ssid"] = networks[i].ssid;
		item["signal"] = networks[i].rssi;
		item["encryption"] = networks[i].getAuthorizationMethodName();
		item["bssid"] = getBSSID(networks[i]);
	}

	response.setAllowCrossDomainOrigin("*");
	response.sendJsonObject(stream);//stream automatic deleted afterward	
}

void makeConnection()
{
	WifiStation.enable(true);
	WifiStation.config(network, password);

	AppSettings.ssid = network;
	AppSettings.password = password;
	AppSettings.save();

	network = ""; // task completed

	WifiStation.connect();

	connStatus = ConnectionStatus::Connecting;
	WifiStation.waitConnection(onSTAconnected, 15, onSTAfailed);
}

void onApiScanNetworks(HttpRequest &request, HttpResponse &response) {
	WifiStation.startScan(networkScanCompleted);
	response.setHeader("Access-Control-Allow-Origin", "*");
	response.sendString("Scanning");
}

void onApiConnect(HttpRequest &request, HttpResponse &response)
{
	if (handlePreflight(request, response))
		return;

	DynamicJsonBuffer jsonBuffer;
	JsonObject& jsonIn = jsonBuffer.parseObject(request.getBody());

	String curNet = jsonIn["ssid"];
	String curPass = jsonIn["password"];

	debugf("received ssid=%s; password=%s", curNet.c_str(), curPass.c_str());	

	network = curNet;
	password = curPass;
	connectionTimer.initializeMs(1000, makeConnection).startOnce();

	response.setHeader("Access-Control-Allow-Origin", "*");
	response.sendString("Connecting");
	//response.sendJsonObject(stream);//stream automatic deleted afterward
}

void onApiReboot(HttpRequest &request, HttpResponse &response) {
	response.sendString("Rebooting ...");
	System.restart();
}

void startWebServer()
{
	server.listen(80);
	// index
	server.addPath("/", onIndex);
	// api to control server, assuming at one time only one client
	server.addPath("/api/setip", onSetIp);
	server.addPath("/api/getip", onGetIp);
	server.addPath("/api/scannetworks", onApiScanNetworks);//call scan, but don't know when it finishes
	server.addPath("/api/getnetworks", onApiNetworkList);//client should call scan, then wait 2-5s then call getnetworks to get result
	server.addPath("/api/connect", onApiConnect);
	server.addPath("/api/reboot", onApiReboot);
	// others
	server.setDefaultHandler(onFile);
}

void startFTP()
{
	if (!fileExist("index.html"))
		fileSetContent("index.html", "<h3>Please connect to FTP and upload files from folder 'web/build' (details in code)</h3>");

	// Start FTP server
	ftp.listen(21);
	ftp.addUser("me", "123"); // FTP account
}

// Will be called when system initialization was completed
void startServers()
{
	startFTP();
	startWebServer();
}

void networkScanCompleted(bool succeeded, BssList list)
{
	if (succeeded)
	{
		for (int i = 0; i < list.count(); i++)
			if (!list[i].hidden && list[i].ssid.length() > 0)
				networks.add(list[i]);
	}
	networks.sort([](const BssInfo& a, const BssInfo& b) { return b.rssi - a.rssi; });
}

void onSTAconnected() {
	debugf("STA connected");
	connStatus = ConnectionStatus::Connected;
}

void onSTAfailed() {
	debugf("STA failed to connect");
	WifiStation.disconnect();//stop station from auto reconnect
	connStatus = ConnectionStatus::ConnectionFailed;
}

void startWifiStationFromSettings() {
	WifiStation.enable(true);

	AppSettings.load();

	if (AppSettings.exist())
	{
		debugf("Appsettings exists, connect");
		WifiStation.config(AppSettings.ssid, AppSettings.password);
		if (!AppSettings.dhcp && !AppSettings.ip.isNull())
			WifiStation.setIP(AppSettings.ip, AppSettings.netmask, AppSettings.gateway);
	}

	connStatus = ConnectionStatus::Connecting;
	WifiStation.waitConnection(onSTAconnected, 15, onSTAfailed);
}

void initSTAmode()
{
	startWifiStationFromSettings();

	// first scan
	WifiStation.startScan(networkScanCompleted);

	// Start AP for configuration
	WifiAccessPoint.enable(true);
	WifiAccessPoint.config("ESP8266-" + WifiAccessPoint.getMAC(), "password", AUTH_MODE::AUTH_WPA2_PSK);

	// Run WEB server on system ready
	System.onReady(startServers);
}
