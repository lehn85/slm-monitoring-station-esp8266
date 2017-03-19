/****
* Solar panel monitor - a learning project, monitors sensors and send to server
* Esp8266 monitoring station module
* Created 2017 by Ngo Phuong Le
* https://github.com/lehn85/slm-monitoring-station-esp8266
* All files are provided under the MIT license.
****/

#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <SmingCore/Debug.h>
#include <Libraries/DHT/DHT.h>
#include <mqtt.h>
#include <network_setup.h>
#include <HttpClientForMySite.h>

// spiff file system location, same as in Makefile-user.mk
#define SPIFF_START_OFFSET 0x100000
#define SPIFF_SIZE 0x100000

//#define LED_PIN 16
void ledOnOff(bool on) {
#ifdef LED_PIN
	digitalWrite(LED_PIN, !on);//it is inverted
#endif
}

#define USE_DEEPSLEEP

#define DELAY_MEASUREMENT (4*60+50)*1000
//#define DELAY_MEASUREMENT 50*1000
#define DELAY_RECONNECT 50*1000
#define DELAY_FOR_STABLIZING_SOLAR_PANEL 5000

#define DHT_SENSOR_PIN 14 // D5 GPIO14

// If you want, you can define WiFi settings globally in Eclipse Environment Variables
#ifndef WIFI_SSID
#define WIFI_SSID "" // Put you SSID and Password here
#define WIFI_PWD ""
#endif

#ifndef API_WRITE_KEY
#define API_WRITE_KEY "key-to-access-nodejs-webserver"
#endif // !API_WRITE_KEY

HttpClientForMySite httpClient("http://letm-solar-panel-monitor.herokuapp.com/data/", API_WRITE_KEY);
void onHttpClientCompleted(HttpClient& client, bool successful);

#define SETUP_PIN 12 //D6 - GPIO12

DHT dht(DHT_SENSOR_PIN);
Timer timerReadSensor;
Timer delayTimer;
Timer waitHttpClientTimer;
int waitHttpClientCount = 0;
int readcount = 0;
int retry_mqtt_broker = 0;

#define MQTT_TOPIC "mysensor/data"
MyMQTTClient* mqtt;

void publishSensor();
void prepareReadSolar1();
void readSolar1();
void prepareReadSolar2();
void readSolar2();
void readHT();

#define SWITCH_LOAD_1 5 //D1 GPIO5
#define SWITCH_LOAD_2 4 //D2 GPIO4
const int A0min = 4;
const int A0max = 1024;
const float r1 = 1000;
const float r2 = 100;
const float panel_area = 1800e-6;//m2

struct sensordata {
	float temp = 0;
	float humid = 0;
	float volt1 = 0;
	float miliwatt1 = 0;
	float volt2 = 0;
	float miliwatt2 = 0;
	float voltm = 0;
	float miliwattm = 0;
	float watt_per_m2 = 0;
};
sensordata data;

void initSensorSolar() {
	pinMode(SWITCH_LOAD_1, OUTPUT);
	digitalWrite(SWITCH_LOAD_1, LOW);
	pinMode(SWITCH_LOAD_2, OUTPUT);
	digitalWrite(SWITCH_LOAD_2, LOW);
}

void prepareReadSolar1() {
	debugf("Prepare solar panel load 1, wait 5s");
	digitalWrite(SWITCH_LOAD_1, HIGH);
	digitalWrite(SWITCH_LOAD_2, LOW);
	delayTimer.initializeMs(DELAY_FOR_STABLIZING_SOLAR_PANEL, readSolar1).startOnce();
}

void readSolar1() {
	int rawA0 = analogRead(A0);
	data.volt1 = 3.3f*(rawA0 - A0min) / (A0max - A0min);
	data.miliwatt1 = data.volt1*data.volt1 / r1 * 1000;
	debugf("Read solar panel (load=1kohm): rawA0=%d, volt=%0.2f V, power=%0.2f mW", rawA0, data.volt1, data.miliwatt1);
	prepareReadSolar2();
}

void prepareReadSolar2() {
	debugf("Prepare solar panel load 2, wait 5s");
	digitalWrite(SWITCH_LOAD_1, LOW);
	digitalWrite(SWITCH_LOAD_2, HIGH);
	delayTimer.initializeMs(DELAY_FOR_STABLIZING_SOLAR_PANEL, readSolar2).startOnce();
}

void readSolar2() {
	int rawA0 = analogRead(A0);
	data.volt2 = 3.3f*(rawA0 - A0min) / (A0max - A0min);
	data.miliwatt2 = data.volt2*data.volt2 / r2 * 1000;
	debugf("Read solar panel (load=100ohm): rawA0=%d, volt=%0.2f V, power=%0.2f mW", rawA0, data.volt2, data.miliwatt2);

	// assuming characteristic solar panel is linear
	// interpolate to find the point that provide maximum output power
	float i1 = data.volt1 / r1;
	float i2 = data.volt2 / r2;
	float m = 0, um = 0, im = 0;
	if (data.volt1 != data.volt2) {
		m = (i1 - i2) / (data.volt1 - data.volt2);
		um = 0.5*(data.volt1 - i1 / m);//V
		im = i1 + m*(um - data.volt1);//A
	}
	data.voltm = um;
	data.miliwattm = um*im * 1000;//*1000 to convert to miliwatt
	data.watt_per_m2 = data.miliwattm / panel_area / 1000;// /1000 to convert to watt
	debugf("For Pmax, volt=%0.2f V, im=%0.2f mA, load = %0.2f, Pmax=%0.2f", data.voltm, im, data.voltm / im, data.miliwattm);

	readHT();

	ledOnOff(false);//turn off when stop reading
}

TempAndHumidity th;
void readHT() {
	debugf("Read HT sensor");

	int trycount = 0;
	bool success = false;

	do {
		success = dht.readTempAndHumidity(th);
	} while (!success && (++trycount < 5));

	if (success)
	{
		debugf("Humidity: %0.2f \%, Temprature: %0.2f *C\r\n", th.humid, th.temp);
		data.temp = th.temp;
		data.humid = th.humid;
		// all read, publish
		publishSensor();
	}
	else
	{
		debugf("Failed to read from DHT: %d, after %d tries", dht.getLastError(), trycount);
		System.deepSleep(DELAY_RECONNECT);
	}
}

void readSensor() {
	ledOnOff(true);//turn on when start reading sensor

	/*first reading method (Adafruit compatible) */
	readcount++;
	/* improved reading method */
	debugf("Read sensor, %d", readcount);
	prepareReadSolar1();
}

// publish data to mqtt broker, which should be connected while measuring sensor data
void publishSensor() {
	//send to mqtt broker (local) as JSON
	StaticJsonBuffer<500> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	root.set("temp", data.temp, 8);
	root.set("humid", data.humid, 8);
	root.set("volt1", data.volt1, 8);
	root.set("miliwatt1", data.miliwatt1, 8);
	root.set("volt2", data.volt1, 8);
	root.set("miliwatt2", data.miliwatt2, 8);
	root.set("voltm", data.voltm, 8);
	root.set("miliwattm", data.miliwattm, 8);
	root.set("watt_per_m2", data.watt_per_m2, 8);

	String msg;
	root.printTo(msg);

	waitHttpClientCount = 0;//reset wait count
	httpClient.postData(msg, onHttpClientCompleted);

	// if mqtt broker is connected, publish message
	if (mqtt->getConnectionState() == eTCS_Connected) {
		mqtt->publishMessage(MQTT_TOPIC, msg);	
	}
	else { //else-> sleep, try again or try start		
#ifdef USE_DEEPSLEEP
		// wait a little more
		if (retry_mqtt_broker < 3) {
			retry_mqtt_broker++;
			debugf("Mqtt broker not connected, wait for 5s (%d)", retry_mqtt_broker);
			delayTimer.initializeMs(5000, publishSensor).startOnce();
			return;
		}
		// retry so many time, retry after a while
		else {
			debugf("Mqtt broker not connected, go to deep sleep for 60s ");
			System.deepSleep(DELAY_RECONNECT, eDSO_RF_CAL_BY_INIT_DATA);
		}
#else
		debugf("Mqtt is not connected, now retrying ...");
		mqtt->start();//auto reconnect
		readSensor();//10s to read sensor and publish (hopefully in 10s mqtt is connected)
#endif // USE_DEEPSLEEP						
	}


}

void onHttpClientCompleted(HttpClient& client, bool successful) {
	Serial.printf("HttpClient success=%d\n", successful);
}

void onMQTTSent() {
	// wait for httpClient to finish (only for 5*2=10s)
	if (waitHttpClientCount<5 && httpClient.isProcessing()) 
	{
		debugf("Wait for httpClient count=%d", waitHttpClientCount);
		waitHttpClientTimer.initializeMs(2000, onMQTTSent);
		waitHttpClientCount++;
	}
	else debugf("httpClient seems finished processing.");

#ifdef USE_DEEPSLEEP
	debugf("Go to deep sleep for sometime %d ms", DELAY_MEASUREMENT);
	System.deepSleep(DELAY_MEASUREMENT, eDSO_RF_CAL_BY_INIT_DATA);
#else
	// read sensor again in 50s
	debugf("Wait for next measurement");
	timerReadSensor.initializeMs(DELAY_MEASUREMENT, readSensor).startOnce();//every 60s
#endif // USE_DEEPSLEEP	
	// sent successful
	retry_mqtt_broker = 0;
}

Timer stationConnectTimeout;
void stationConnectTimeoutCallback() {
	if (!WifiStation.isConnected()) {
		debugf("Station is not connected for some reason. Go to deep sleep mode for %d", DELAY_RECONNECT);
		System.deepSleep(DELAY_RECONNECT);
	}
}

// call when station got ip
void STAGotIP(IPAddress ip, IPAddress mask, IPAddress gateway)
{
	debugf("GOTIP - IP: %s, MASK: %s, GW: %s\n",
		ip.toString().c_str(),
		mask.toString().c_str(),
		gateway.toString().c_str());

	// Run MQTT client
	mqtt->start();

	// read first value (after 10s, send to broker)
	readSensor();
}

void init()
{
	//spiffs_mount(); // Mount file system, in order to work with files
    // mount manually (same as to makefile-user.mk) if Sming ver <3.1 then +0x40200000 is address start of SPI flash for esp8266
	spiffs_mount_manual(SPIFF_START_OFFSET, SPIFF_SIZE);

	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(true); // Allow debug output to serial

	debugf("Initializing...");

	// check if need to enter setup mode
	pinMode(SETUP_PIN, INPUT);
	if (digitalRead(SETUP_PIN)) {
		debugf("Goto STA mode for setup");
		initSTAmode();
		return;
	}

	// otherwise: normal operation

	initSensorSolar();

	// led
#ifdef  LED_PIN
	pinMode(LED_PIN, OUTPUT);
#endif //  LED_PIN	

	// disable access point for now
	WifiAccessPoint.enable(false);

	Serial.println("Wait 1 second for the sensor to boot up");
	ledOnOff(true);
	//disable watchdog
	WDT.enable(false);
	//wait for sensor startup
	delay(1000);
	WDT.enable(true);

	ledOnOff(false);

	dht.begin();

	mqtt = new MyMQTTClient();
	mqtt->setMqttOnSentListener(onMQTTSent);//when sent to mqtt broker successfully

	//connect to wifi	
	// load settings and start
	startWifiStationFromSettings();
	// events when connected
	WifiEvents.onStationGotIP(STAGotIP);

	//time out if cannot connect to AP
	stationConnectTimeout.initializeMs(30 * 1000, stationConnectTimeoutCallback).startOnce();
}