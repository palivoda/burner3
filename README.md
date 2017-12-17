# ESP8266 based light control

Automatic switch of external lights.

Board: Wemos D1 Mini

Input1: Time from NTP server.
Input2: Photoresistor.

Output1: 5V 1 Channel SSR G3MB-202P Solid State Relay Module 240V 2A Output with Resistive Fuse

Power Supply: LM2596S based 220 AC to  5 DC 0.5A

Extras:
- Temperature and humidity reading with DHT22
- Sketch update over WiFi connection

Monitoring: Blynk.cc widgets
- Manual override V2 slider (-1 - always off, 0 - automatic, +1 - always on) widget on V2 for manul override.  
- Terminal control on V1 (commands: on, off, state, update)
- Light state V3
- Chart with Temperature (V4) and Humidity (V5)
- Temperature text (V4)

![Hardware setup photo](preview_hw.jpg?raw=true "Hardware setup")

![Mobile app screenshot](preview_mobile.jpg?raw=true "Mobile app")
