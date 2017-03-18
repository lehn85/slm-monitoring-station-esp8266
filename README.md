# Introduction
This is a learning project, which to help me learn about IoT.

This app reads sensors (temperature, humidity) and energy produced by a solar panel. Data is logged in local node-red server and [a webserver on Heroku](https://letm-solar-panel-monitor.herokuapp.com).

Other modules:
- [Nodejs webserver](https://github.com/lehn85/slm-nodejs-webserver)

Two modes:
- Normal mode: measure temperature, humidity, voltage and power output of a solar panel, send to local node-red server and [a webserver on Heroku](https://letm-solar-panel-monitor.herokuapp.com).
- Setup mode: hold button setup (see circuit diagram) when power up. Esp8266 enters setup mode, which create access point named "Esp8266-**", setup interface can be accessed via web browser. Link: http://192.168.4.1.

### Normal mode
Program on esp8266 connects to wifi, measures sensors and send data via MQTT to a local mqtt broker. It also send data to internet webserver via HTTP (POST method).
After sending data, esp8266 go to deep sleep for 5 minutes.

### Setup mode
put some pictures here maybe -))
# Dependencies
#### Esp8266
Program uses [Sming framework](https://github.com/SmingHub/Sming).

But I modified some in makefile (mostly modified memory mapping and added more command line options), and forked here: https://github.com/lehn85/Sming.
#### Front-end (web interface for setup mode)
- Angularjs
- Angularjs material

# Schematic
![Schematic](/schematic.png)

Measuring solar panel requires varying load. That is why 2 MOSFET n-channel are included: to switch between load 1000 ohm and 100 ohm. After measuring voltage for each load, optimal load (a resistance value that gets the most power out of solar panel) is calculated by interpolation. I may write detail when I have time.