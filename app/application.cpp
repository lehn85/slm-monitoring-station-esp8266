#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <Libraries/DHT/DHT.h>
#include <mqtt.h>

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

#define DHT_SENSOR_PIN 14 // GPIO14

// If you want, you can define WiFi settings globally in Eclipse Environment Variables
#ifndef WIFI_SSID
#define WIFI_SSID "" // Put you SSID and Password here
#define WIFI_PWD ""
#endif

DHT dht(DHT_SENSOR_PIN);
Timer timerReadSensor;
Timer delayTimer;
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

#define SWITCH_LOAD_1 5
#define SWITCH_LOAD_2 4
const int A0min = 4;
const int A0max = 1024;
const float r1 = 1000;
const float r2 = 100;

float volt1 = 0;
float power1 = 0;
float volt2 = 0;
float power2 = 0;
float voltm = 0;
float powerm = 0;

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
	volt1 = 3.3f*(rawA0 - A0min) / (A0max - A0min);
	power1 = volt1*volt1 / r1 * 1000;
	debugf("Read solar panel (load=1kohm): rawA0=%d, volt=%0.2f V, power=%0.2f mW", rawA0, volt1, power1);
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
	volt2 = 3.3f*(rawA0 - A0min) / (A0max - A0min);
	power2 = volt2*volt2 / r2 * 1000;
	debugf("Read solar panel (load=100ohm): rawA0=%d, volt=%0.2f V, power=%0.2f mW", rawA0, volt2, power2);

	// assuming characteristic solar panel is linear
	// interpolate to find the point that provide maximum output power
	float i1 = volt1 / r1;
	float i2 = volt2 / r2;
	float m = (volt1 == volt2) ? 1e10f : ((i1 - i2) / (volt1 - volt2));
	float um = 0.5*(volt1 - i1 / m);
	float im = (i1 + m*(um - volt1)) * 1000;//mA
	voltm = um;
	powerm = um*im;
	debugf("For Pmax, volt=%0.2f V, im=%0.2f mA, load = %0.2f, Pmax=%0.2f", voltm, im, voltm / im * 1000, powerm);

	readHT();

	ledOnOff(false);//turn off when stop reading
}

TempAndHumidity th;
void readHT() {
	debugf("Read HT sensor");
	if (dht.readTempAndHumidity(th))
	{
		debugf("Humidity: %0.2f \%, Temprature: %0.2f *C\r\n", th.humid, th.temp);
		publishSensor();
	}
	else
	{
		debugf("Failed to read from DHT: %d", dht.getLastError());		
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
	//send to mqtt		
	String msg = "field1=" + String(th.temp) + "&field2=" + String(th.humid)
		+ "&field3=" + String(volt1) + "&field4=" + String(power1, 2)
		+ "&field5=" + String(volt2) + "&field6=" + String(power2, 2)
		+ "&field7=" + String(voltm) + "&field8=" + String(powerm, 2);
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

void onMQTTSent() {
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
	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(true); // Allow debug output to serial

	debugf("Initializing...");

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
	WifiStation.enable(true);
	WifiStation.config(WIFI_SSID, WIFI_PWD, true, true);
	// static ip if needed
	WifiStation.setIP(IPAddress(10, 1, 10, 198), IPAddress(255, 255, 255, 0), IPAddress(10, 1, 10, 1));
	// events
	WifiEvents.onStationGotIP(STAGotIP);

	//time out if cannot connect to AP
	stationConnectTimeout.initializeMs(30 * 1000, stationConnectTimeoutCallback).startOnce();
}