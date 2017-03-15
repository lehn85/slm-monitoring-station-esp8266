#include <network_setup.h>

#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <AppSettings.h>

HttpServer server;
FTPServer ftp;

BssList networks;
String network, password;
Timer connectionTimer;

bool handlePreflight(HttpRequest &request, HttpResponse &response) {
	// incase of a preflight request (for application/json content)
	//https://developer.mozilla.org/en-US/docs/Web/HTTP/Access_control_CORS#Preflighted_requests
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

	json["status"] = (bool)true;

	bool connected = WifiStation.isConnected();
	json["connected"] = connected;
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

	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	bool updating = curNet.length() > 0 && (WifiStation.getSSID() != curNet || WifiStation.getPassword() != curPass);
	bool connectingNow = WifiStation.getConnectionStatus() == eSCS_Connecting || network.length() > 0;

	if (updating && connectingNow)
	{
		debugf("wrong action: %s %s, (updating: %d, connectingNow: %d)", network.c_str(), password.c_str(), updating, connectingNow);
		json["status"] = (bool)false;
		json["connected"] = (bool)false;
	}
	else
	{
		json["status"] = (bool)true;
		if (updating)
		{
			network = curNet;
			password = curPass;
			debugf("CONNECT TO: %s %s", network.c_str(), password.c_str());
			json["connected"] = false;
			connectionTimer.initializeMs(1200, makeConnection).startOnce();
		}
		else
		{
			json["connected"] = WifiStation.isConnected();
			debugf("Network already selected. Current status: %s", WifiStation.getConnectionStatusName());
		}
	}

	if (!updating && !connectingNow && WifiStation.isConnectionFailed())
		json["error"] = WifiStation.getConnectionStatusName();

	response.setHeader("Access-Control-Allow-Origin", "*");
	response.sendJsonObject(stream);//stream automatic deleted afterward
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
	// restful api
	server.addPath("/api/setip", onSetIp);
	server.addPath("/api/getip", onGetIp);
	server.addPath("/api/getnetworks", onApiNetworkList);
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
}

void onSTAfailed() {
	debugf("STA failed to connect");
	WifiStation.disconnect();//stop station from auto reconnect
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
	else {
		debugf("Appsettings not exist, use default maybe");
		WifiStation.config("bullshit", "sss", true, true);
	}
	WifiStation.waitConnection(onSTAconnected, 15, onSTAfailed);
}

void initSTAmode()
{
	startWifiStationFromSettings();

	WifiStation.startScan(networkScanCompleted);

	// Start AP for configuration
	WifiAccessPoint.enable(true);
	WifiAccessPoint.config("ESP8266-" + WifiAccessPoint.getMAC(), "password", AUTH_MODE::AUTH_WPA2_PSK);

	// Run WEB server on system ready
	System.onReady(startServers);
}
