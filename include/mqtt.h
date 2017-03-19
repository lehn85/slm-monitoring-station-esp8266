/****
* Solar panel monitor - a learning project, monitors sensors and send to server
* Esp8266 monitoring station module
* Created 2017 by Ngo Phuong Le
* https://github.com/lehn85/slm-monitoring-station-esp8266
* All files are provided under the MIT license.
****/

#pragma once

typedef Delegate<void()> MQTTOnSent;

// MQTT client to local Mosquitto broker
class MyMQTTClient : public MqttClient {

	// ... and/or MQTT username and password
#ifndef MQTT_USERNAME
#define MQTT_USERNAME ""
#define MQTT_PWD ""
#endif

	// ... and/or MQTT host and port
#ifndef MQTT_HOST
//#define MQTT_HOST "mqtt.thingspeak.com"
#define MQTT_HOST "10.1.10.105"
#ifndef ENABLE_SSL
#define MQTT_PORT 1883
#else
#define MQTT_PORT 8883
#endif
#endif

public:
	MyMQTTClient(MqttStringSubscriptionCallback onMessageReceived = 0);

	void start();

	void publishMessage(String topic, String msg);

	void setMqttOnSentListener(MQTTOnSent onsent);

protected:
	err_t onReceive(pbuf *buf);

	void onReadyToSendData(TcpConnectionEvent e);

private:
	MQTTOnSent mqttOnSent;
};