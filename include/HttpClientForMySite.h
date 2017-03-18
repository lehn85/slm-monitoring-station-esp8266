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