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