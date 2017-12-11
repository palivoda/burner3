#pragma once
#ifndef LIGHT_CONTROL_CONFIG_H
#define LIGHT_CONTROL_CONFIG_H

#define MY_WIFI_SSID "wifi"
#define MY_WIFI_PASSWD "secret"

#define BLYNK_TOKEN "BLINK-TOKEN-12345"
#define BLYNK_PIN_TERMINAL V1
#define BLYNK_PIN_SLIDER V2
#define BLYNK_PIN_LED V3

#define PIN_LUMENS 4
#define PIN_LIGHT 5
#define TIME_ZONE +2

#define EEPROM_SIZE 4
#define EEPROM_MANUAL_OVERRIDE 0 //int, 2 bytes
//#define EEPROM_MANUAL_ 2

#define SECONDS 1000
#define LIGHT_OFF HIGH
#define LIGHT_ON LOW



#endif //LIGHT_CONTROL_CONFIG_H
