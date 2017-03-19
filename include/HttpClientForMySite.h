/****
* Solar panel monitor - a learning project, monitors sensors and send to server
* Esp8266 monitoring station module
* Created 2017 by Ngo Phuong Le
* https://github.com/lehn85/slm-monitoring-station-esp8266
* All files are provided under the MIT license.
****/

#pragma once
#include <SmingCore/SmingCore.h>

class HttpClientForMySite : public HttpClient {
public:
	HttpClientForMySite(String url, String apiWriteKey);
	void postData(String msg, HttpClientCompletedDelegate cb);

private:
	String apiWriteKey;	
	String url;
};