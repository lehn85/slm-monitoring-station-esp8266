## Local build configuration
## Parameters configured here will override default and ENV values.
## Uncomment and change examples:

## memory mapping customize
MAIN_OFFSET ?= 0x00000
MAIN_CAP ?= 524288
SDK_OFFSET ?= 0x80000
SPI_SIZE = 4M
LD_SCRIPT = rom.ld

## Add your source directories here separated by space
# MODULES = app
# EXTRA_INCDIR = include

## ESP_HOME sets the path where ESP tools and SDK are located.
## Windows:
# ESP_HOME = c:/Espressif

## MacOS / Linux:
# ESP_HOME = /opt/esp-open-sdk

## SMING_HOME sets the path where Sming framework is located.
## Windows:
# SMING_HOME = c:/tools/sming/Sming 

## MacOS / Linux
# SMING_HOME = /opt/sming/Sming

## COM port parameter is reqruied to flash firmware correctly.
## Windows: 
# COM_PORT = COM3

## MacOS / Linux:
# COM_PORT = /dev/tty.usbserial

## Com port speed
# com speed for download tool
COM_SPEED_ESPTOOL	= 921600
# com speed for code
COM_SPEED_SERIAL = 115200

## Configure flash parameters (for ESP12-E and other new boards):
SPI_MODE = dio

## SPIFFS options
#DISABLE_SPIFFS = 1
SPIFF_FILES = web/build
SPIFF_START_OFFSET = 0x100000
SPIFF_SIZE     = 196608