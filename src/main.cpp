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

float voltageAvg = -1.0;

void loop()
{
	Serial.print(NTP.getTimeDate(now())); Serial.print(" - ");

	if (WiFi.isConnected()) {

		reconnectIn--;

		// --- read sensors ---
		int sensorValue = analogRead(A0);
		float voltage = sensorValue * (3.2 / 1023.0);
		Serial.print(voltage); Serial.print("v ");

		//average
		if (voltageAvg == -1.0) voltageAvg = voltage;
		else voltageAvg = (voltageAvg * 3 + voltage) * 0.25; // /4

		//logic on/off switch
		static tmElements_t tm;
		breakTime(now(), tm);

		bool onSensor = false, onWeek = false, onHoliday = false;
		if ( //sensor
			(lightState == HIGH && voltageAvg > 1.4)  ||  //if OFF turn on
			(lightState == LOW  && voltageAvg > 1.1)	//if ON turn off level is lower
		) onSensor = true;
		if ( //week
			(tm.Wday == 1 || tm.Wday == 6) && // sunday, saturday
				(tm.Hour >= 7 && tm.Hour <= 10 || tm.Hour >= 3 && tm.Hour <= 23 || tm.Hour == 1)  ||
			(tm.Wday == 5)	&&  //friday
				(tm.Hour >= 7 && tm.Hour <= 9 ||  tm.Hour >= 6 && tm.Hour <= 23 || tm.Hour == 1) ||
			(tm.Wday >= 2 || tm.Wday <= 4)	&&  //weekday
				(tm.Hour >= 7 && tm.Hour <= 9 ||  tm.Hour >= 6 && tm.Hour <= 23)
		) onWeek = true;
		if ( //holidays
			tm.Month == 1 && tm.Day == 1 || tm.Month == 12 && tm.Day == 31 ||  //New Year
			tm.Month == 2 && tm.Day == 14 ||  //valentines day
			tm.Month == 7 && tm.Day == 7 ||  //bd
			tm.Month == 10 && tm.Day == 31 ||  //halloween
			tm.Month == 12 && tm.Day == 25 //christmas
		) onHoliday = true;
		if (tm.Year == 0) onHoliday = false; //no NTP
		Serial.printf(" Wday=%i, Month=%i, Day=%i, Year=%i ", tm.Wday, tm.Month, tm.Day, tm.Year);
		Serial.printf(" onSensor=%i, onWeek=%i, onHoliday=%i \n", onSensor, onWeek, onHoliday);

		if (onSensor && (onWeek || onHoliday)) {
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

		if (reconnectIn % 60 == 0) { //each N seconds

			HTTPClient http;

			String url = String(LOGGLY_URL);
			url += "&tm=";
			url += tm.Hour;
			url += ".";
			url += tm.Minute;
			url += "&onWeek=";
			url += onWeek;
			url += "&onHoliday=";
			url += onHoliday;
			url += "&v=";
			url += voltage;
			url += "&va=";
			url += voltageAvg;
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
