#pragma once
#ifndef LIGHT_CONTROL_CONFIG_H
#define LIGHT_CONTROL_CONFIG_H

#define MY_WIFI_SSID "wifi"
#define MY_WIFI_PASSWD "secret"

#define BLYNK_TOKEN "BLINK-TOKEN-12345"
#define BLYNK_PIN_TERMINAL V1
#define BLYNK_PIN_SLIDER V2
#define BLYNK_PIN_LED V3
#define BLYNK_PIN_TEMP V4
#define BLYNK_PIN_HUMID V5
#define BLYNK_PIN_LIGHT_AVG V6

//Upload .bin file from project compilation folder, harware update initiated with Blynk console 'update' command
#define MY_FIRMWARE_URL "https://raw.githubusercontent.com/palivoda/esp8266-light-control/master/firmware/hw-1.0/firmware.bin"
//use https://www.grc.com/fingerprints.htm to get correct HTTPS fingerprints, replace ":" with " "
#define MY_FIRMWARE_HTTPS_CHECK "CC AA 48 48 66 46 0E 91 53 2C 9C 7C 23 2A B1 74 4D 29 9D 33"

#define PIN_LUMENS 4
#define PIN_LIGHT 5
#define PIN_TEMP D3
#define TIME_ZONE +2

#define EEPROM_SIZE 16 //bytes
#define EEPROM_MANUAL_OVERRIDE 0

#define SECONDS 1000
#define LIGHT_OFF HIGH
#define LIGHT_ON LOW



#endif //LIGHT_CONTROL_CONFIG_H
