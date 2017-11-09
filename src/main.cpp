#include <Arduino.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>             //https://github.com/esp8266/Arduino
#include <NtpClient.h>               //https://github.com/stelgenhof/NTPClient
#include <ESP8266HTTPClient.h>       //https://github.com/esp8266/Arduino
#include "Config.h"

int reconnectIn = 0;
int lightState = LOW;

void setup() {

	Serial.begin(115200);
	delay(SECONDS);
  Serial.println("Seting up...");

	// --- Setup wifi

	static WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;

	gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& ipInfo){
		Serial.printf("Got IP: %s\r\n", IPAddress(ipInfo.ip).toString().c_str());
		NTP.init((char *)DEFAULT_NTP_SERVER, UTC0200); //DEFAULT_NTP_SERVER
	  NTP.setPollingInterval(10*60); // Poll every 10 minutes
		reconnectIn = 0; //reset
	});

	disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event_info){
		Serial.printf("Disconnected from SSID: %s\n", event_info.ssid.c_str());
		Serial.printf("Reason: %d\n", event_info.reason);
		NTP.stop(); // NTP sync can be disabled to avoid sync errors
	});

	WiFi.mode(WIFI_STA);
	WiFi.begin(MY_WIFI_SSID, MY_WIFI_PASSWD);

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
	digitalWrite(PIN_LIGHT, HIGH);
}

void loop()
{
	Serial.print(NTP.getTimeDate(now())); Serial.print(" - ");

	if (WiFi.isConnected()) {

		reconnectIn--;

		// --- read sensors ---

		int sensorValue = analogRead(A0);
		float voltage = sensorValue * (3.2 / 1023.0);
		Serial.print(voltage); Serial.print("v ");


		//logic on/off switch
		static tmElements_t tm;
		breakTime(now(), tm);

		if (
			 ((lightState == HIGH && voltage > 1.9) ||  //if OFF turn on
		    (lightState == LOW && voltage > 1.5)) &&  //if ON turn off level is lower
			 ((tm.Hour >= 6 && tm.Minute >= 20 ) ||     //from time
			  (tm.Hour <= 23 && tm.Minute <= 50 ))       //until time
		) {
			lightState = LOW;
		  Serial.println("Light ON");
		}
		else {
			lightState = HIGH;
		  Serial.println("Light OFF");
		}
		digitalWrite(LED_BUILTIN, lightState);
		digitalWrite(PIN_LUMENS, lightState);

	  //  ---  POST logs to web  ---

		if (reconnectIn % 15 == 0) {

			HTTPClient http;

			String url = String(LOGGLY_URL);
			url += "&data=";
			url += voltage;
			url += "&light=";
			url += (lightState == LOW);


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

	}
	else {
		Serial.print("Offline ");

		// --- Reconnect WiFI each minute @ 7 second ----

		reconnectIn++;
		Serial.println(reconnectIn);

		if (reconnectIn % 60 == 7) {
			Serial.println("Reconnecting...");
		  WiFi.reconnect();
		}
	}

	delay(SECONDS);

}
