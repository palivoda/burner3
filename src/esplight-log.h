
void log(const char* text)
{
	terminal.print(text);
	Serial.print(text);
}

void log(const int text)
{
	terminal.print(text);
	Serial.print(text);
}

void logln(const char* text, int value)
{
	log(text);
	log(value);
	log("\n");
}

void logln(const char* text, const char* value)
{
	log(text);
	log(value);
	log("\n");
}

void logln(const char* text)
{
	log(text);
	log("\n");
}
