#define DEBUG_ESP_HTTP_UPDATE
#define DEBUG_ESP_PORT Serial
#include "config.h"
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>             //https://github.com/esp8266/Arduino
#include <NtpClient.h>               //https://github.com/stelgenhof/NTPClient
//#define BLYNK_PRINT Serial //Blynk extra logs
#include <BlynkSimpleEsp8266.h>  	   //https://github.com/blynkkk/blynk-library, see https://examples.blynk.cc
//TODO: https://github.com/blynkkk/blynk-library/blob/master/examples/Export_Demo/myPlant_ESP8266/myPlant_ESP8266.ino
WidgetTerminal terminal(BLYNK_PIN_TERMINAL);
#include "esplight-log.h"
#include "esplight-eeprom.h"
#include "DHT.h"                    //https://github.com/adafruit/DHT-sensor-library.git
DHT dht(PIN_TEMP, DHT22);


int reconnectIn = 0;
int lightState = LIGHT_OFF;
int lightValue = 0;
float voltageAvg = -1.0;
static tmElements_t tm;
bool firmwareUpdate = false;


BLYNK_WRITE(BLYNK_PIN_TERMINAL)
{
	Serial.print("Terminal: ");
	Serial.println(param.asStr());

  if (String("state") == param.asStr()) {
    const char* MAP_WEEK_DAY[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    const char* MAP_OVERRIDE[] = {"Manual OFF", "Auto", "Manual ON"};
    if (lightState == LIGHT_OFF) logln("LIGHT is OFF");
		else if (lightState == LIGHT_ON) logln("LIGHT is ON");
		logln("Override: ", MAP_OVERRIDE[overridedeState+1]);
		logln("Light (V): ", lightValue);
		logln("Light Avg. (V): ", (int)voltageAvg*1000);
		logln("Week Day: ", MAP_WEEK_DAY[tm.Wday-1]);
		log("Date: ", tm.Month); log("/", tm.Day); logln("/", tm.Year);
		log("Time: ", tm.Hour); logln(":", tm.Minute);
    logln("Temperature (C): ", (int)dht.readTemperature());
    logln("Humidity (%%): ", (int)dht.readHumidity());
    logln("Uptime (s): ", (int)millis() / SECONDS);
    logln("Compiled: " __DATE__ ", " __TIME__ ", " __VERSION__);
	}
	else if (String("on") == param.asStr()) {
		setOverrideState(+1);
	}
	else if (String("off") == param.asStr()) {
		setOverrideState(-1);
	}
	else if (String("auto") == param.asStr()) {
		setOverrideState(0);
	}
  else if (String("update") == param.asStr()) {

    logln("Updating...");
    logln(MY_FIRMWARE_URL);
    firmwareUpdate = true;
	}
	else {
		logln("Available commands: state, on, off, auto, update");
	}
  terminal.flush();
}

BLYNK_WRITE(BLYNK_PIN_SLIDER) //manual override
{
	setOverrideState(param.asInt());
}

BLYNK_READ(BLYNK_PIN_TEMP)
{
  Blynk.virtualWrite(BLYNK_PIN_TEMP, dht.readTemperature());
}

BLYNK_READ(BLYNK_PIN_HUMID)
{
  Blynk.virtualWrite(BLYNK_PIN_HUMID, dht.readHumidity());
}

void setup() {

	Serial.begin(115200);
	Serial.setDebugOutput(true);
	delay(SECONDS);
  Serial.println("Seting up...");

	// --- Setup WiFi

	static WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;

	gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& ipInfo) {
		logln("Got IP: ", IPAddress(ipInfo.ip).toString().c_str());
		NTP.init((char *)DEFAULT_NTP_SERVER, UTC0200); //DEFAULT_NTP_SERVER
	  NTP.setPollingInterval(10*60); // Poll every 10 minutes
		reconnectIn = 0; //reset
	});

	disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event_info) {
		logln("Disconnected from SSID: ", event_info.ssid.c_str());
		logln("Reason: ", event_info.reason);
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
		  logln("NTP server not reachable.");
			terminal.flush();
      break;
    case NTP_EVENT_SYNCHRONIZED:
			logln("Got NTP time: ", NTP.getTimeDate(NTP.getLastSync()));
			terminal.flush();
      break;
    }
  });

	// --- Setup MQTT (Blynk)

	Blynk.config(BLYNK_TOKEN);

	// --- setup PINs

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LIGHT_OFF);
	pinMode(PIN_LIGHT, OUTPUT);
	digitalWrite(PIN_LIGHT, LIGHT_OFF);
	pinMode(PIN_LUMENS, INPUT_PULLUP);

  // --- Setup temperature sensor

  dht.begin();

	// --- Setup EEPROM

	EEPROM.begin(16);
	readOverrideState();
	logln("Override restored: ", overridedeState);
}

void loop()
{

	if (WiFi.isConnected()) {

		reconnectIn--;

    // --- Firmware update if flag set

    if (firmwareUpdate) {

      firmwareUpdate = false;

      HTTPUpdateResult ret = ESPhttpUpdate.update(MY_FIRMWARE_URL, "1.0", MY_FIRMWARE_HTTPS_CHECK);

      switch(ret) {
        case HTTP_UPDATE_FAILED:
          log("Update failed (");
          log(ESPhttpUpdate.getLastError());
          log("): ");
          logln(ESPhttpUpdate.getLastErrorString().c_str());
          break;
        case HTTP_UPDATE_NO_UPDATES:
          logln("No updates");
          break;
      }

    }

		if (overridedeState != 0) { // manual On/Off
			if (lightState == LIGHT_ON && overridedeState == -1) {
				lightState = LIGHT_OFF;
				logln("Manual OFF");
			}
			if (lightState == LIGHT_OFF && overridedeState == +1) {
				lightState = LIGHT_ON;
				logln("Manual ON");
			}
		}
		else { //automatic On/Off

			// --- read sensors ---
			lightValue = analogRead(A0);
			float voltage = lightValue * (3.2 / 1023.0);

			//30 seconds floating average
			if (voltageAvg == -1.0) voltageAvg = voltage;
			else voltageAvg = (voltageAvg * 30 + voltage) / 31;

			//logic on/off switch
			breakTime(now(), tm);

			bool onSensor = false, onWeek = false, onHoliday = false;
			if ( //sensor
				(lightState == LIGHT_OFF && voltageAvg > 1.6)  ||  //if OFF turn on
				(lightState == LIGHT_ON  && voltageAvg > 1.3)	//if ON turn off level is lower
			) onSensor = true;
			if ( //week
				(tm.Wday == 1) && // sunday
					(tm.Hour >= 7 && tm.Hour <= 10 || tm.Hour >= 15 && tm.Hour <= 20)  ||
				(tm.Wday == 6) && // saturday
					(tm.Hour >= 7 && tm.Hour <= 10 || tm.Hour >= 15 && tm.Hour <= 22)  ||
				(tm.Wday == 5)	&&  //friday
					(tm.Hour >= 7 && tm.Hour <= 8 ||  tm.Hour >= 18 && tm.Hour <= 22) ||
				(tm.Wday >= 2 || tm.Wday <= 4)	&&  //weekday
					(tm.Hour >= 7 && tm.Hour <= 8 ||  tm.Hour >= 18 && tm.Hour <= 20)
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
				if (lightState != LIGHT_ON) {
					lightState = LIGHT_ON;
					logln("Auto ON");
					logln("On sensor: ", onSensor);
					logln("On week: ", onWeek);
					logln("On holiday: ", onHoliday);
				}
			}
			else {
				if (lightState != LIGHT_OFF) {
					lightState = LIGHT_OFF;
					logln("Auto  OFF");
					logln("On sensor: ", onSensor);
					logln("On week: ", onWeek);
					logln("On holiday: ", onHoliday);
				}
			}

		}

		digitalWrite(LED_BUILTIN, lightState);
		digitalWrite(PIN_LUMENS, lightState);

		// loop MQTT (Blynk)
		Blynk.virtualWrite(BLYNK_PIN_LED, !lightState);
		Blynk.virtualWrite(BLYNK_PIN_SLIDER, overridedeState);
    Blynk.run();
    terminal.flush();

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

		if (lightState == LIGHT_ON) { //turn off light is not connection
			digitalWrite(LED_BUILTIN, LIGHT_OFF);
			digitalWrite(PIN_LUMENS, LIGHT_OFF);
		}

	}

	delay(SECONDS);
	Serial.flush();
}
