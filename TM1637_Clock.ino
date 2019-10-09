/*
 * Simple clock for the Living Room
 *
 * Uses a DS3231 RTC and TM1637 serial display.
 * Self-adjusts for Daylight Savings Time
 */

/*
 * Put includes here
 *
 */
// Internal EEPROM for Timezone offsets
#include <EEPROM.h>
// For TM1637
#include <Arduino.h>
#include <TM1637Display.h>  // https://github.com/avishorp/TM1637
// For DS3231
#include <DS3232RTC.h>      // https://github.com/JChristensen/DS3232RTC
#include <Streaming.h>      // http://arduiniana.org/libraries/streaming/
#include <Timezone.h>       // https://github.com/JChristensen/Timezone
// TimeLib.h is brought in by DS3232RTC.h
// Next line is for reference only
//#include <TimeLib.h>      // https://github.com/PaulStoffregen/Time
/*
 * End of includes
 *
 */
 
/*
 * Put defines here
 *
 */
// for serial output debugging
#define DEBUG
#undef DEBUG
// For the TM1637
#define CLK 3              // For TM1637 (can be any pin)
#define DIO 4              // ditto
// For the EEPROM
#define EEBASE 100         // Base address
#define SIG1   0x55        // Expected signature value at EEBASE
#define SIG2   0xAA        // Expected signature value at EEBASE + 1
#define TZBASE EEBASE + 2  // Where Timezone rules are stored
// For the display brightness
#define THRESHOLD 900
#define DIM 0x08
#define BRIGHT 0x0f
// Blank digit
#define BLANK 0x00
// For the degree symbol
#define DEGREE 0x63
// The dash symbol
#define DASH 0x40
// Bitmask for colon in time
#define COLON 0x40
// For the 12/24 hour mode
// Pull MODEPIN high to show military time
#define MODEPIN 5
#define HR12 0
#define HR24 1
/*
 * End of defines
 *
 */
  
/*
 * Program constants
 *
 */
   
/*
 * End of constants
 *
 */
    
/*
 * Global objects here
 *
 */
TM1637Display display(CLK, DIO);       // LED display object

// CAN Eastern Time Zone (Toronto)
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};    //Daylight time = UTC - 4 hours
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};     //Standard time = UTC - 5 hours
Timezone myTZ(myDST, mySTD);                                  // The default rules
// If TimeChangeRules are already stored in EEPROM,
// they are read in setup().

TimeChangeRule *tcr;                   //pointer to the time change rule, use to get TZ abbrev
/*
 * End of global objects
 *
 */
    
/*
 * Global simple variables here
 *
 */

uint8_t data[] = { 0xff, 0xff, 0xff, 0xff };
uint8_t blank[] = { 0x00, 0x00, 0x00, 0x00 };

byte t24 = HR12;                       // if zero, 12-hour time, 1 for 24 hour time

/*
 * End of global simple variables
 *
 */
    
/*
 * Setup code
 *
 */

void setup () {
    // Configure 12/24 hour mode pin
    pinMode(MODEPIN, INPUT_PULLUP);
    // Configure tell-tale
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);  
	
    Serial.begin(115200);
    Serial << endl << F("Blackfire Clock") << endl << F("Startup ...") << endl;
    
	// Un-comment to force update for testing
    // setClock();
    
    if (validSignature()) {
        Serial << F("EEPROM Valid");
		// Fetch rules from EEPROM
        Timezone myTZ(EEBASE+2);
        Serial << F("Read Timezone Data") << endl;
    } else {
        Serial << F("EEPROM Invalid") << endl << F("Setting time") << endl;
		// Go into timeset mode
        setClock();
    }

    // setSyncProvider() causes the Time library to synchronize with the
    // external RTC by calling RTC.get() every five minutes by default.
    setSyncProvider(RTC.get);
	
	// 3600 s (1 h) between syncs with hardware clock
    setSyncInterval((time_t)3600);

    Serial << F("RTC Sync ");
	
    if (timeStatus() != timeSet) {
        Serial << F("RTC FAIL!");
		// Go into timeset mode
        setClock();
    }
    Serial << endl;

	// Set display brightness
    adjustIllumination();
	
	// Set 12/24 hour mode based on switch
	t24 = adjustMode();                          
}

    
/*
 * Main program loop
 *
 */
void loop() {
    static time_t tLast;
    time_t t;
    tmElements_t tm;
    int s;

	// Read UTC time
    time_t utc = now();
	
	// Convert to local
    t = myTZ.toLocal(utc, &tcr);
    
    if (t != tLast) {

        if (Serial.available())
           setClock();
           
        tLast = t;

        // 
        s = second(t);
        if ((s >= 20) & (s <= 25)) {                // At the 20 s mark,
            displayTemp();                          // ...  display the temp for 5 s
#ifdef DEBUG
            printTemp();                            // Serial display
#endif
        } else if ((s == 44) || (s == 45)) {        // At the 44 s mark
		    displayDay(t);                            // Show the day
        } else if ((s == 42) || (s == 43)) {        // At the 42 s mark
            displayMonth(t);                        // Show the month
        } else if ((s == 40) || (s == 41)) {        // At the 40 s mark
            displayYear(t);                         // Show the year
        } else {
            displayTime(t);                         // Show the time
#ifdef DEBUG
			printDateTime(t);                          // Serial display
#endif
        }
		 
		t24 = adjustMode();                           // See is 12/24 display mode changed 
		adjustIllumination();                         // Check lighting and adjust brightness
    }
}

/*
 * Display the hours and minutes on the display
 * showNumberDec(int num, bool leading_zero, uint8_t length, uint8_t pos)
 *              (minute, leading 0, The number of digits to set, (0 - leftmost, 3 - rightmost))
 * showNumberDec(   m,      true,              2,                          2)
 */
void displayTime(time_t t) {
    // hours in hour(t)
    // minute in minute(t)
    
    uint8_t h = hour(t);    // internally, h is always in 24-hour format
    uint8_t m = minute(t);
    uint8_t s = second(t);
 
    // Show the colon on even seconds   
    uint8_t dot = (((s % 2) == 0) ? COLON : 0);

#ifdef DEBUG
    if (dot ==0)
        digitalWrite(13, LOW);
    else
        digitalWrite(13, HIGH);
#endif

    if (t24 == 0)              // if showing 12-hour time
        h = hourFormat12(t);   // get the hour in 12-hour format

    // Show the 2 digit hour starting at position 0
    display.showNumberDecEx(h, dot, false, 2, 0);
    // Show the 2 digit minute with leading zero starting at position 2
    display.showNumberDec(m, true, 2, 2);
  
}

/*
 * Show the temperature of the DS3231 on-board sensor
 */
void displayTemp() {
    float c = RTC.temperature() / 4.;

    int t = (int)(c);

    data[0] = (t < 0 ? DASH : 0);           // Account for freezing
    data[1] = display.encodeDigit(t / 10);  // Tens
    data[2] = display.encodeDigit(t % 10);  // Units
    data[3] = DEGREE;                       // degree symbol

    display.setSegments(data);
}

/*
 * Show the 4 digit year on the display
 */
void displayYear(time_t t) {
    int yr = year(t);
#ifdef DEBUG
    Serial << F("Year: ") << yr << endl;
#endif
	/* use library call showNumberDec() to simplify - it does all the work
    data[0] = display.encodeDigit(yr / 1000);
    yr = (yr % 1000);
    data[1] = display.encodeDigit(yr / 100);
    yr = (yr % 100);
    data[2] = display.encodeDigit(yr / 10);
    yr = (yr % 10);
    data[3] = display.encodeDigit(yr);

    display.setSegments(data);
	*/
	display.showNumberDec(yr, false, 4, 0); // 4 digits starting at the leftmost position
}

/*
 * Show the month on the two middle digits of the display
 */
void displayMonth(time_t t) {
    int mo = month(t);
#ifdef DEBUG
    Serial << F("Month: ") << mo << endl;
#endif
    data[0] = DASH;
    data[1] = display.encodeDigit( mo / 10 );
    data[2] = display.encodeDigit( mo % 10 );
    data[3] = DASH;
  
    display.setSegments(data);
}

/*
 * Show the day of the month on the two middle digits of the display
 */
void displayDay(time_t t) {
    int dy = day(t);
#ifdef DEBUG
    Serial << F("Day: ") << dy << endl;
#endif
    data[0] = DASH;
    data[1] = display.encodeDigit( dy / 10 );
    data[2] = display.encodeDigit( dy % 10 );
    data[3] = BLANK;
  
    display.setSegments(data);
}

/*
 * The following routines are for the serial port
 */
 // print date and time to Serial
void printDateTime(time_t t)
{
    printDate(t);
    printTime(t);
	Serial << endl;
}

// print time to Serial
void printTime(time_t t)
{
    printI00(hour(t), ':');
    printI00(minute(t), ':');
    printI00(second(t), ' ');
}

// print date to Serial
void printDate(time_t t)
{
    Serial << _DEC(year(t)) << "-" << monthShortStr(month(t)) << "-";
    printI00(day(t), ' ');
}

// print temperature to serial
void printTemp() {
    float c = RTC.temperature() / 4.;
	
    Serial << F("Temperature: ") << c << F(" deg C") << endl;
}

// Print an integer in "00" format (with leading zero),
// followed by a delimiter character to Serial.
// Input value assumed to be between 0 and 99.
void printI00(int val, char delim)
{
    if (val < 10)
	    Serial << '0';
    Serial << _DEC(val);
    if (delim > 0)
	    Serial << delim;
    return;
}
// End of Serial printing routines

// EEPROM routines
boolean validSignature()
{
    byte by1 = EEPROM.read(EEBASE);
    byte by2 = EEPROM.read(EEBASE + 1);
    if ((by1 == SIG1) && (by2 == SIG2))
        return true;
    else
        return false;
}

void writeSignature()
{
    EEPROM.write(EEBASE,     SIG1);
    EEPROM.write(EEBASE + 1, SIG2);
}
// End EEPROM routines

// void    setTime(int hr,int min,int sec,int day, int month, int yr);
void setClock()
{
    time_t t;
    tmElements_t tm;
    
	Serial << F("Must set date and time") << endl << endl;
    Serial << F("Set LOCAL time (yy,m,d,h,m,s") << endl;
    
    while (!Serial.available());                 // Wait for serial input
    
    delay(10);                                   // may not be necessary
    
    // check for input to set the RTC, minimum length is 12, i.e. yy,m,d,h,m,s
    if (Serial.available() >= 12) {
        // note that the tmElements_t Year member is an offset from 1970,
        // but the RTC wants the last two digits of the calendar year.
        // use the convenience macros from the Time Library to do the conversions.
        int y = Serial.parseInt();
        if (y >= 100 && y < 1000)
            Serial << F("Error: Year must be two digits or four digits!") << endl;
        else {
            if (y >= 1000)
                tm.Year = CalendarYrToTm(y);
            else    // (y < 100)
                tm.Year = y2kYearToTm(y);
				
            tm.Month = Serial.parseInt();
            tm.Day = Serial.parseInt();
            tm.Hour = Serial.parseInt();
            tm.Minute = Serial.parseInt();
            tm.Second = Serial.parseInt();

            t = makeTime(tm);                    // This is local time
			
            time_t utc = myTZ.toUTC(t);          // convert to UTC
			
            RTC.set(utc);                        // Set the soft-clock
            setTime(utc);                        // Write to the hardwre clock
            RTC.get();                           // Get time in soft-clock from hardware clock
			
            writeSignature();                    // Write the signature bytes to EEPROM
            myTZ.writeRules(TZBASE);             // write rules to EEPROM
			
            Serial << endl << F("RTC set to: ZULU "); // Display set time on serial monitor
            printDateTime(utc);
			
            Serial << endl;
			
            // dump any extraneous input
            while (Serial.available() > 0)
                Serial.read();
        } // if (y >= 100 && y < 1000)
    } // if (Serial.available ...)
}

/*
 * Set display brightness based on ambient illumination
 * Adjust THRESHOLD, BRIGHT and DIM to your tastes
 */
void adjustIllumination() {
    int illum = analogRead(A0);
    
    if (illum > THRESHOLD)
      display.setBrightness(BRIGHT);
    else
      display.setBrightness(DIM);
}

/*
 * Select 12-hour or 24-hour display mode
 * based on user selectable switch
 * The default position of the switch is open
 * and means the input is pulled up by the internally
 * pull-up which is a 1 (TRUE) signifying 12-hour mode.
 */
int adjustMode() {
    return ((digitalRead(MODEPIN)) ? HR12 : HR24);
}
