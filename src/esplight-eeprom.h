#include <EEPROM.h>

int overridedeState = 0;

void setOverrideState(int newState) {
	logln("Overrided: ", newState);
	overridedeState = newState;
	EEPROM.write(EEPROM_MANUAL_OVERRIDE, byte(newState + 1)); //only unsigned value can be stored
	EEPROM.commit();
}

void readOverrideState()
{
	overridedeState = EEPROM.read(EEPROM_MANUAL_OVERRIDE) - 1;
}
