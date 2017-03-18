#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <mqtt.h>

MyMQTTClient::MyMQTTClient(MqttStringSubscriptionCallback onMessageReceived)
	: MqttClient(MQTT_HOST, MQTT_PORT, onMessageReceived)
{
	// Assign a disconnect callback function
	//setCompleteDelegate(checkMQTTDisconnect);
	//mqtt->subscribe("main/status/#");		
}

void MyMQTTClient::start() {
	/*if (!mqtt->setWill("last/will", "The connection from this device is lost:(", 1, true)) {
	debugf("Unable to set the last will and testament. Most probably there is not enough memory on the device.");
	}*/
	connect("esp8266", MQTT_USERNAME, MQTT_PWD, false);
	setTimeOut(60);//connect make timeout to max, need to reset it
#ifdef ENABLE_SSL
	mqtt->addSslOptions(SSL_SERVER_VERIFY_LATER);

#include <ssl/private_key.h>
#include <ssl/cert.h>

	mqtt->setSslClientKeyCert(default_private_key, default_private_key_len,
		default_certificate, default_certificate_len, NULL, true);

#endif	
}

bool justPublish = false;
void MyMQTTClient::publishMessage(String topic, String msg) {
	if (getConnectionState() != eTCS_Connected)
	{		
		start(); // Auto reconnect
	}
	else
	{
		debugf("Publish message %s", msg.c_str());
		// publish to channel
		justPublish = true;
		publish(topic, msg);
	}
}

void MyMQTTClient::onReadyToSendData(TcpConnectionEvent e) {
	// raise event OnSent 
	if (justPublish && e == eTCE_Sent) {
		justPublish = false;		
		if (mqttOnSent != 0)
			mqttOnSent();
	}

	MqttClient::onReadyToSendData(e);
}

err_t MyMQTTClient::onReceive(pbuf *buf) {
	if (buf == NULL) {
		//disconnected
		debugf("Thinkspeak Mqtt disconnected !!!!! ");
	}
	return MqttClient::onReceive(buf);
}

void MyMQTTClient::setMqttOnSentListener(MQTTOnSent onsent) {
	mqttOnSent = onsent;
}