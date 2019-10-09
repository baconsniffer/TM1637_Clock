# TM1637_Clock
A simple clock using a TM1637-based 4-digit LED display
A simple DS3231-based digital clock with 4-digit
LED display via TM1637 serial interface. Clock will show
the time (selectable 12 or 24 hour format). 

****************************************
Version 3
****************************************
Changed to using a BMP280 sensor for the temperature.
Moved DST rules from internal EEPROM to the AT24C04
in the RTC module.

****************************************
Version 2
****************************************
Displays the temperature at the 20 s mark for 5 s.
Displays the year, month and day at the 40 s mark.
Uses a photocell to determine ambient illumination
and dims the display. Automatically adjusts for
Daylight Savings Time. Rules are stored in the
Arduino internal EEPROM.

****************************************
Initial version
****************************************
For the initial version the time is set as per the
"ds3231_set_time.ino" sketch where serial input is used
to perform initial configuration. Unless the time is being set
the clock will wait until a minute has elapsed and then
update the display.

The second iteration will use a CDS photocell to check
ambient lighting and dim the display.

The third iteration will allow the year, month and day to
be displayed every 5 minutes at the bottom of the minute.
Each component will display for 5 seconds. For example,
at 13:05:30 the year is displayed as 4 digits. At 13:05:35
the month is displayed (maybe as letters) and then at
13:05:40 the day of the month is displayed.

May add more functions as I think of them and code space allows.

