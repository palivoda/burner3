#include <ESP8266WiFi.h>             //https://github.com/esp8266/Arduino
#include <NtpClient.h>               //https://github.com/stelgenhof/NTPClient
//#include <ESP8266HTTPClient.h>       //https://github.com/esp8266/Arduino
#define BLYNK_MAX_SENDBYTES 256			 // Default is 128
#include <BlynkSimpleEsp8266.h>  	   //https://github.com/blynkkk/blynk-library, see https://examples.blynk.cc
#include "Config.h"

//#define BLYNK_PRINT Serial //Blynk extra logs

int reconnectIn = 0;
int lightState = LOW;
int overridedeState = 0;

// Attach virtual serial terminal to Virtual Pin V0
WidgetTerminal terminal(V1);

BLYNK_WRITE(V1) //Terminal
{
	Serial.print("Terminal: ");
	Serial.println(param.asStr());

  if (String("hello") == param.asStr()) {
    terminal.println("light");
		Serial.println("light");
	}
  terminal.flush();
}

BLYNK_WRITE(V2) //manual override
{
	overridedeState = param.asInt();
  Serial.print("Manual override (V1): ");
  Serial.println(overridedeState);
}

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

	// --- Setup NTP

	NTP.onSyncEvent([](const NTPSyncEvent_t& ntpEvent) {
    switch (ntpEvent) {
    case NTP_EVENT_INIT:
      break;
    case NTP_EVENT_STOP:
      break;
    case NTP_EVENT_NO_RESPONSE:
      Serial.println("NTP server not reachable.\n");
			terminal.println("NTP server not reachable.\n");
			terminal.flush();
      break;
    case NTP_EVENT_SYNCHRONIZED:
      Serial.printf("Got NTP time: %s\n", NTP.getTimeDate(NTP.getLastSync()));
			terminal.printf("Got NTP time: %s\n", NTP.getTimeDate(NTP.getLastSync()));
			terminal.flush();
      break;
    }
  });

	// --- Setup MQTT (Blynk)
	Blynk.config(BLYNK_TOKEN);

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

		if (overridedeState != 0) { // manual On/Off
			lightState = overridedeState == 1 ? LOW : HIGH;
			Serial.printf("Manual light: %i\n", lightState);
			if (reconnectIn % BLYNK_TERMINAL_INTERVAL == 0) { //update each N seconds
				terminal.printf("Manual light: %i\n", lightState);  terminal.flush();
			}
		}
		else { //automatic On/Off

			// --- read sensors ---
			int sensorValue = analogRead(A0);
			float voltage = sensorValue * (3.2 / 1023.0);

			//30 seconds floating average
			if (voltageAvg == -1.0) voltageAvg = voltage;
			else voltageAvg = (voltageAvg * 30 + voltage) / 31;

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
					(tm.Hour >= 7 && tm.Hour <= 10 || tm.Hour >= 15 && tm.Hour <= 23 || tm.Hour == 1)  ||
				(tm.Wday == 5)	&&  //friday
					(tm.Hour >= 7 && tm.Hour <= 9 ||  tm.Hour >= 18 && tm.Hour <= 23 || tm.Hour == 1) ||
				(tm.Wday >= 2 || tm.Wday <= 4)	&&  //weekday
					(tm.Hour >= 7 && tm.Hour <= 9 ||  tm.Hour >= 18 && tm.Hour <= 22)
			) onWeek = true;
			if ( //holidays
				tm.Month == 1 && tm.Day == 1 || tm.Month == 12 && tm.Day == 31 ||  //New Year
				tm.Month == 2 && tm.Day == 14 ||  //valentines day
				tm.Month == 7 && tm.Day == 7 ||  //bd
				tm.Month == 10 && tm.Day == 31 ||  //halloween
				tm.Month == 12 && tm.Day == 25 //christmas
			) onHoliday = true;
			if (tm.Year == 0) onHoliday = false; //no NTP

			if (onSensor && (onWeek || onHoliday)) {
				lightState = LOW;
				Serial.print("Light ON: ");
			}
			else {
				lightState = HIGH;
				Serial.print("Light OFF: ");
			}

			if (reconnectIn % BLYNK_TERMINAL_INTERVAL == 0) { //update each N seconds
				terminal.printf("Wday=%i, Month=%i, Day=%i, Year=%i, onSensor=%i, onWeek=%i, onHoliday=%i, V=%i, V(avg)=%i\n",
					tm.Wday, tm.Month, tm.Day, tm.Year, onSensor, onWeek, onHoliday, (int)voltage*100, (int)voltageAvg*100);
				terminal.flush();
				Serial.print("(T) ");
			}

			Serial.printf("Wday=%i, Month=%i, Day=%i, Year=%i, onSensor=%i, onWeek=%i, onHoliday=%i, V=%i, V(avg)=%i\n",
				tm.Wday, tm.Month, tm.Day, tm.Year, onSensor, onWeek, onHoliday, (int)voltage*100, (int)voltageAvg*100);

		}

		digitalWrite(LED_BUILTIN, lightState);
		digitalWrite(PIN_LUMENS, lightState);

		// loop MQTT (Blynk)
  	Blynk.run();

	}
	else {
		Serial.print("Offline ");

		// --- Reconnect WiFI each minute @ 7 second ----

		reconnectIn++;
		Serial.println(reconnectIn);

		if (reconnectIn % 60 == 7) { //once a minute on each 7th second
			Serial.println("Reconnecting...");
		  WiFi.reconnect();
		}
	}

	delay(SECONDS);

}
