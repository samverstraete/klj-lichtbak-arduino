#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <FastLED.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

// ************************* BELANGRIJKE INSTELLINGEN *************************
#define WS28XX					// Indien uit commentaar: WS28XX strip, anders RGB uitgang
//onderstaande in commentaar zetten als al ingesteld / bij update
#define NUM_LEDS_PER_STRIP 5	// Aantal LEDs op WS28XX strip
#define THISNAME "min16"  // Naam (voor mDNS), max 15 characters!
// ****************************************************************************
#define REDPIN   13
#define GREENPIN 12
#define BLUEPIN  14
#define BUTTON1  5
#define BUTTON2  4

const char* ssid = "klj";
const char* password = "zomergem";
const char* pre_host = "esp-";

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

static uint8 hue;
static uint8 brightness;
static uint8 debounce;
static uint8 mode;
static CRGB::HTMLColorCode mode2color;
static CRGB leds[NUM_LEDS_PER_STRIP];
static String host;

void setup() {
	pinMode(REDPIN, OUTPUT);
	pinMode(GREENPIN, OUTPUT);
	pinMode(BLUEPIN, OUTPUT);
	pinMode(BUTTON1, INPUT);
	pinMode(BUTTON2, INPUT);
	analogWriteRange(255);

	Serial.begin(115200);
	Serial.println("Starting...");
	EEPROM.begin(64);
	//laatste instelling terug ophalen
	EEPROM.get(16, hue); 
	EEPROM.get(17, brightness);
	if (brightness == 0) brightness = 255;
	//naam ophalen
	Serial.println("Writing...");
	delay(100);
#ifdef THISNAME
	eeprom_write_string(0, THISNAME);
	EEPROM.commit();
	delay(100);
#endif
	char this_host[16];
	eeprom_read_string(0, this_host, 16);

	//probeer met Wifi te verbinden voor update
	WiFi.begin(ssid, password);
	delay(100);
	/*memcpy(host, pre_host, sizeof(pre_host));
	memcpy(host + sizeof(pre_host), this_host, sizeof(this_host));*/
	host = pre_host + (String)this_host;
	char __host[sizeof(host)];
	host.toCharArray(__host, sizeof(__host));

	Serial.print("Dit is ");
	Serial.println(host);
	MDNS.begin(__host); 
	httpUpdater.setup(&httpServer);
	httpServer.on("/", hoofdpagina);
	httpServer.on("/kleur", kleur);
	httpServer.on("/modus", modus);
	httpServer.begin();
	MDNS.addService("http", "tcp", 80);

	Serial.println("Setup einde");
	//altijd beginnen met vast kleur
	mode = 1;

#ifdef NUM_LEDS_PER_STRIP
	EEPROM.put(18, NUM_LEDS_PER_STRIP);
	EEPROM.commit();
	delay(100);
#endif
#ifdef WS28XX
	//LED strips configureren
	uint8 num_leds_per_strip = NUM_LEDS_PER_STRIP;
	//EEPROM.get(18, num_leds_per_strip);
	FastLED.addLeds<NEOPIXEL, REDPIN>(leds, num_leds_per_strip);
	FastLED.addLeds<NEOPIXEL, GREENPIN>(leds, num_leds_per_strip);
#endif
	showAnalogRGB(CHSV(hue, 255, brightness));
}

void loop()
{
	// First, clear the existing led values
	bool button1 = !digitalRead(BUTTON1);
	bool button2 = !digitalRead(BUTTON2);
	if (button1 || button2)
	{
		if (debounce < 3)
		{
			//even wachten om zeker te zijn
			debounce++;
		}
		else
		{
			Serial.println("Press");
			//één van de knoppen is nu zeker ingedrukt
			if (button1 && !button2)
			{
				Serial.print("Hue");
				Serial.println(hue);
				mode = 1;
				//kleurenspectrum doorlopen zolang knop ingedrukt
				hue = hue + 2;
				showAnalogRGB(CHSV(hue, 255, brightness));
			}
			else if (!button1 && button2)
			{
				Serial.print("Brightness ");
				Serial.println(brightness);
				mode = 1;
				//lichtsterkte doorlopen zolang knop ingedrukt
				brightness = brightness - 3;
				if (brightness < 50) brightness = 255; //minimums
				showAnalogRGB(CHSV(hue, 255, brightness));
			}
			else if (button1 && button2)
			{
				Serial.println("Mode2");
				mode = 2;
				//diagnose knop, alle drie de kleuren + wit doorlopen
				switch (mode2color)
				{
					case CRGB::White: 
						mode2color = CRGB::Red;
						break;
					case CRGB::Red:
						mode2color = CRGB::Green;
						break;
					case CRGB::Green:
						mode2color = CRGB::Blue;
						break;
					default:
						mode2color = CRGB::White;
				}
				showAnalogRGB(mode2color);
				delay(1000);
			}
		}
	}
	else
	{
		if (debounce > 2 && mode == 1)
		{
			Serial.println("End press");
			//beide knoppen zijn 0, dus debounce resetten
			debounce = 0;
			//kleur opslaan
			EEPROM.put(16, hue);
			EEPROM.put(17, brightness);
			EEPROM.commit();
			delay(100);
		}

	}

	if (WiFi.isConnected()) httpServer.handleClient();

	delay(100);
}

void showAnalogRGB(const CRGB& rgb)
{
#ifdef WS28XX
	for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
		leds[i] = rgb;
	}
	FastLED.show();
#else
	analogWrite(REDPIN, rgb.r);
	analogWrite(GREENPIN, rgb.g);
	analogWrite(BLUEPIN, rgb.b);
#endif
}

void hoofdpagina() 
{
	httpServer.send(200, "text/plain", host);
}

void kleur()
{
	if (httpServer.hasArg("h"))
	{
		uint8_t h = atoi(httpServer.arg("h").c_str());
		uint8_t s = atoi(httpServer.arg("s").c_str());
		uint8_t v = atoi(httpServer.arg("v").c_str());

		showAnalogRGB(CHSV(h, s, v));
	}
	String s = "{\"h\":" + String(hue) + ",\"s\":" + String(255) + ",\"v\":" + String(brightness) + "}";
	httpServer.send(200, "application/json", s);
}

void modus()
{
	if (httpServer.hasArg("modus")) 
	{

	}
	httpServer.send(200, "text/plain", "Not implemented");
}

// Writes a sequence of bytes to eeprom starting at the specified address.
// Returns true if the whole array is successfully written.
// Returns false if the start or end addresses aren't between
// the minimum and maximum allowed values.
// When returning false, nothing gets written to eeprom.
boolean eeprom_write_bytes(int startAddr, const byte* array, int numBytes) {
	// counter
	int i;

	for (i = 0; i < numBytes; i++) {
		EEPROM.put(startAddr + i, array[i]);
	}
	return true;
}

// Writes a string starting at the specified address.
// Returns true if the whole string is successfully written.
// Returns false if the address of one or more bytes fall outside the allowed range.
// If false is returned, nothing gets written to the eeprom.
boolean eeprom_write_string(int addr, const char* string) {

	int numBytes; // actual number of bytes to be written
				  //write the string contents plus the string terminator byte (0x00)
	numBytes = strlen(string) + 1;
	return eeprom_write_bytes(addr, (const byte*)string, numBytes);
}

// Reads a string starting from the specified address.
// Returns true if at least one byte (even only the string terminator one) is read.
// Returns false if the start address falls outside the allowed range or declare buffer size is zero.
// 
// The reading might stop for several reasons:
// - no more space in the provided buffer
// - last eeprom address reached
// - string terminator byte (0x00) encountered.
boolean eeprom_read_string(int addr, char* buffer, int bufSize) {
	byte ch; // byte read from eeprom
	int bytesRead; // number of bytes read so far

	if (bufSize == 0) { // how can we store bytes in an empty buffer ?
		return false;
	}
	// is there is room for the string terminator only, no reason to go further
	if (bufSize == 1) {
		buffer[0] = 0;
		return true;
	}
	bytesRead = 0; // initialize byte counter
	EEPROM.get(addr + bytesRead, ch); // read next byte from eeprom
	buffer[bytesRead] = ch; // store it into the user buffer
	bytesRead++; // increment byte counter
				 // stop conditions:
				 // - the character just read is the string terminator one (0x00)
				 // - we have filled the user buffer
				 // - we have reached the last eeprom address
	while ((ch != 0x00) && (bytesRead < bufSize)) {
		// if no stop condition is met, read the next byte from eeprom
		EEPROM.get(addr + bytesRead, ch);
		buffer[bytesRead] = ch; // store it into the user buffer
		bytesRead++; // increment byte counter
	}
	// make sure the user buffer has a string terminator, (0x00) as its last byte
	if ((ch != 0x00) && (bytesRead >= 1)) {
		buffer[bytesRead - 1] = 0;
	}
	return true;
}