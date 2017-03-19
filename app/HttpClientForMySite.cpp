/****
* Solar panel monitor - a learning project, monitors sensors and send to server
* Esp8266 monitoring station module
* Created 2017 by Ngo Phuong Le
* https://github.com/lehn85/slm-monitoring-station-esp8266
* All files are provided under the MIT license.
****/

#include <SmingCore/SmingCore.h>
#include <HttpClientForMySite.h>

HttpClientForMySite::HttpClientForMySite(String url, String apiWriteKey)
{
	this->url = url + apiWriteKey;
	this->apiWriteKey = apiWriteKey;
}

void HttpClientForMySite::postData(String msg, HttpClientCompletedDelegate cb)
{
	debugf("preparing request");
	setRequestContentType("application/json");
	setPostBody(msg);
	downloadString(url, cb);
}