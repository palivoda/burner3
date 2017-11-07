#include <Arduino.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>             //https://github.com/esp8266/Arduino
#include <NtpClient.h>               //https://github.com/stelgenhof/NTPClient
#include <ESP8266HTTPClient.h>       //https://github.com/esp8266/Arduino
#include "Config.h"

void setup() {

	Serial.begin(115200);
	delay(SECONDS);
  Serial.println("Seting up...");

	// --- Setup wifi

	static WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;

	gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& ipInfo){
		Serial.printf("Got IP: %s\r\n", IPAddress(ipInfo.ip).toString().c_str());
		NTP.init((char *)"pool.ntp.org", UTC0900); //DEFAULT_NTP_SERVER
	  NTP.setPollingInterval(10*60); // Poll every 10 minutes
	});

	disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event_info){
		Serial.printf("Disconnected from SSID: %s\n", event_info.ssid.c_str());
		Serial.printf("Reason: %d\n", event_info.reason);
		NTP.stop(); // NTP sync can be disabled to avoid sync errors
	});

	WiFi.mode(WIFI_STA);
	WiFi.begin(MY_WIFI_SSID, MY_WIFI_PASSWD);

	while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

	// --- Setup

	NTP.onSyncEvent([](const NTPSyncEvent_t& ntpEvent) {
    switch (ntpEvent) {
    case NTP_EVENT_INIT:
      break;
    case NTP_EVENT_STOP:
      break;
    case NTP_EVENT_NO_RESPONSE:
      Serial.printf("NTP server not reachable.\n");
      break;
    case NTP_EVENT_SYNCHRONIZED:
      Serial.printf("Got NTP time: %s\n", NTP.getTimeDate(NTP.getLastSync()));
      break;
    }
  });

	// --- setup PINs

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);
	pinMode(PIN_LUMENS, INPUT_PULLUP);
	pinMode(PIN_LIGHT, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);
}


void loop()
{

	if (WiFi.isConnected()) {

		Serial.print(NTP.getTimeDate(now())); Serial.print(" - ");

		// --- read sensors ---

		int sensorValue = analogRead(A0);
		float voltage = sensorValue * (3.2 / 1023.0);
		Serial.print(voltage); Serial.print("v ");


		//logic on/off switc
		static tmElements_t tm;
		breakTime(now(), tm);
		int lightState = LOW;

		if (voltage > 0.8 &&  //if dark
			 (tm.Hour >= 6 && tm.Minute >= 30 ) ||   //from time
			 (tm.Hour <= 23 && tm.Minute <= 59 )   //untill time
		) {
			lightState = HIGH;
		  Serial.println("Light ON");
		}
		else {
			lightState = HIGH;
		  Serial.println("Light OFF");
		}
		digitalWrite(LED_BUILTIN, lightState);
		digitalWrite(PIN_LUMENS, lightState);

	  //  ---  POST logs to web  ---

		HTTPClient http;

		String url = String("https://logs-01.loggly.com/inputs/7d918780-857f-44f7-be14-d1cc12b7d9a5.gif?source=pixel&");
		url += "data=";
		url += voltage;
		url += "&light=";
		url += lightState;


		Serial.print("[HTTP] begin... ");
		Serial.print(url);
		http.begin(url, "39 1e 5f eb 58 00 95 7c fa 25 67 af d9 59 fe 70 33 62 a0 85"); //HTTPS

		Serial.print(" get...");
		int httpCode = http.GET();

		if(httpCode > 0) {
				Serial.printf(" code: %d\n", httpCode);
		} else {
				Serial.printf(" get failed, error: %s\n", http.errorToString(httpCode).c_str());
		}

		http.end();

	}
	else {
		Serial.println("Offline...");
		//TODO: try reconnect
	}

	delay(SECONDS);

}
