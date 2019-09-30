/*
 * RealtimeClock.cpp
 *
 * Created: 26/09/2019
 *  Author: Mark Riley
 */
 
#include <avr/pgmspace.h>                 // Needed for PROGMEM
#include "Wire.h"                         // Needed for I2C bus
#include <EEPROM.h>                       // Needed for internal EEPROM R/W
#include "PetitFS.h"                      // Light handler for FAT16 and FAT32 filesystem on SD
#include "DefinitionsFile.h"
#include "Monitor.h"
#include "Generic.h"
#include "RealTimeClock.h"

const byte    daysOfMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
const String  compTimeStr  = __TIME__;    // Compile timestamp string
const String  compDateStr  = __DATE__;    // Compile datestamp string

// DS3231 RTC variables
byte    foundRTC;                   // Set to 1 if RTC is found, 0 otherwise
byte    seconds, minutes, hours, day, month, year;
byte    tempC;                      // Temperature (Celsius) encoded in two’s complement integer format
unsigned long timeStamp;            // Timestamp for led blinking

// ------------------------------------------------------------------------------
// RTC Module routines
// ------------------------------------------------------------------------------
 
// ------------------------------------------------------------------------------
// Change manually the RTC Date/Time from keyboard
// ------------------------------------------------------------------------------
void ChangeRTC()
{
    byte    tempB = 0;                   // Temporary variable (buffer)     // Read RTC
    readRTC(&seconds, &minutes, &hours, &day,  &month,  &year, &tempC);

    // Change RTC date/time from keyboard
    Serial.println("\nIOS: RTC manual setting:");
    Serial.println("\nPress T/U to increment +10/+1 or CR to accept");
    do
    {
        do
        {
            Serial.print(" ");
            switch (tempB)
            {
                case 0:
                Serial.print("Year -> ");
                print2digit(year);
                break;
                 
                case 1:
                Serial.print("Month -> ");
                print2digit(month);
                break;

                case 2:
                Serial.print("             ");
                Serial.write(13);
                Serial.print(" Day -> ");
                print2digit(day);
                break;

                case 3:
                Serial.print("Hours -> ");
                print2digit(hours);
                break;

                case 4:
                Serial.print("Minutes -> ");
                print2digit(minutes);
                break;

                case 5:
                Serial.print("Seconds -> ");
                print2digit(seconds);
                break;
            } // switch

            timeStamp = millis();
            do
            {
                blinkIOSled(&timeStamp);
                inChar = Serial.read();
            } while ((inChar != 'u') && (inChar != 'U') && (inChar != 't') && (inChar != 'T') && (inChar != 13));
             
            if ((inChar == 'u') || (inChar == 'U'))
            {
                // Change units
                switch (tempB)
                {
                    case 0:
                    year = (year == 99) ? 0 : year + 1;
                    break;

                    case 1:
                    month = (month == 12) ? 1 : month + 1;
                    break;

                    case 2:
                    day++;
                    if (month == 2)
                    {
                        if (day > (daysOfMonth[month - 1] + isLeapYear(year)))
                        {
                            day = 1;
                        }
                    }
                    else
                    {
                        if (day > (daysOfMonth[month - 1]))
                        {
                            day = 1;
                        }
                    }
                    break;

                    case 3:
                    hours = ( hours == 23 ) ? 0 : hours + 1;
                    break;

                    case 4:
                    minutes = ( minutes == 59 ) ? 0 : minutes + 1;
                    break;

                    case 5:
                    seconds = ( seconds == 59 ) ? 0 : seconds + 1;
                    break;
                } // switch
            } // if
             
            if ((inChar == 't') || (inChar == 'T'))
            {
                // Change tens
                switch (tempB)
                {
                    case 0:
                    year = year + 10;
                    if (year > 99)
                    {
                        year -= (year / 10) * 10;
                    }
                    break;

                    case 1:
                    if (month > 10)
                    {
                        month -= 10;
                    }
                    else if (month < 3)
                    {
                        month += 10;
                    }
                    break;

                    case 2:
                    day += 10;
                    if (day > (daysOfMonth[month - 1] + isLeapYear(year)))
                    {
                        day -= (day / 10) * 10;
                    }
                    if (day == 0)
                    {
                        day = 1;
                    }
                    break;

                    case 3:
                    hours += 10;
                    if (hours > 23)
                    {
                        hours -= (hours / 10 ) * 10;
                    }
                    break;

                    case 4:
                    minutes += 10;
                    if (minutes > 59)
                    {
                        minutes -= (minutes / 10 ) * 10;
                    }
                    break;

                    case 5:
                    seconds += 10;
                    if (seconds > 59)
                    {
                        seconds -= (seconds / 10 ) * 10;
                    }
                    break;
                }
            }
            Serial.write(13);
        } while (inChar != 13);
         
        tempB++;
    } while (tempB < 6);

    // Write new date/time into the RTC
    writeRTC(seconds, minutes, hours, day, month, year);
    Serial.println(" ...done      ");
    Serial.print("IOS: RTC date/time updated (");
    printDateTime(1);
    Serial.println(")");
}


 // ------------------------------------------------------------------------------
 // Read current date/time binary values and the temprerature (2 complement) from the DS3231 RTC
 // ------------------------------------------------------------------------------
 void readRTC(byte *second, byte *minute, byte *hour, byte *day, byte *month, byte *year, byte *tempC)
 {
     byte i;
     Wire.beginTransmission(DS3231_RTC);
     Wire.write(DS3231_SECRG);                       // Set the DS3231 Seconds Register
     Wire.endTransmission();
     // Read from RTC and convert to binary
     Wire.requestFrom(DS3231_RTC, 18);
     *second = bcdToDec(Wire.read() & 0x7f);
     *minute = bcdToDec(Wire.read());
     *hour = bcdToDec(Wire.read() & 0x3f);
     Wire.read();                                    // Jump over the DoW
     *day = bcdToDec(Wire.read());
     *month = bcdToDec(Wire.read());
     *year = bcdToDec(Wire.read());
     for (i = 0; i < 10; i++)
     {
         Wire.read();           // Jump over 10 registers
     }
     *tempC = Wire.read();
 }

 // ------------------------------------------------------------------------------
 // Write given date/time binary values to the DS3231 RTC
 // ------------------------------------------------------------------------------
 void writeRTC(byte second, byte minute, byte hour, byte day, byte month, byte year)
 {
     Wire.beginTransmission(DS3231_RTC);
     Wire.write(DS3231_SECRG);                       // Set the DS3231 Seconds Register
     Wire.write(decToBcd(second));
     Wire.write(decToBcd(minute));
     Wire.write(decToBcd(hour));
     Wire.write(1);                                  // Day of week not used (always set to 1 = Sunday)
     Wire.write(decToBcd(day));
     Wire.write(decToBcd(month));
     Wire.write(decToBcd(year));
     Wire.endTransmission();
 }

 // ------------------------------------------------------------------------------
 // Check if the DS3231 RTC is present and set the date/time at compile date/time if
 // the RTC "Oscillator Stop Flag" is set (= date/time failure).
 // Return value: 0 if RTC not present, 1 if found.
 // ------------------------------------------------------------------------------
 byte autoSetRTC()
 {
     byte    OscStopFlag;

     Wire.beginTransmission(DS3231_RTC);
     if (Wire.endTransmission() != 0)
     {
         return 0;      // RTC not found
     }
     Serial.print("IOS: Found RTC DS3231 Module (");
     printDateTime(1);
     Serial.println(")");

     // Print the temperaturefrom the RTC sensor
     Serial.print("IOS: RTC DS3231 temperature sensor: ");
     Serial.print((int8_t)tempC);
     Serial.println("C");
     
     // Read the "Oscillator Stop Flag"
     Wire.beginTransmission(DS3231_RTC);
     Wire.write(DS3231_STATRG);                      // Set the DS3231 Status Register
     Wire.endTransmission();
     Wire.requestFrom(DS3231_RTC, 1);
     OscStopFlag = Wire.read() & 0x80;               // Read the "Oscillator Stop Flag"

     if (OscStopFlag)
     {
         // RTC oscillator stopped. RTC must be set at compile date/time
         // Convert compile time strings to numeric values
         seconds = compTimeStr.substring(6,8).toInt();
         minutes = compTimeStr.substring(3,5).toInt();
         hours   = compTimeStr.substring(0,2).toInt();
         day     = compDateStr.substring(4,6).toInt();
         switch (compDateStr[0])
         {
             case 'J':
             month = 1;
             if( compDateStr[1] != 'a' )
             {
                 if( compDateStr[2] == 'n' )
                 {
                     month = 6;
                 }
                 else
                 {
                     month = 7;
                 }
             }
             break;
             case 'F': month = 2; break;
             case 'A': month = compDateStr[2] == 'r' ? 4 : 8; break;
             case 'M': month = compDateStr[2] == 'r' ? 3 : 5; break;
             case 'S': month = 9; break;
             case 'O': month = 10; break;
             case 'N': month = 11; break;
             case 'D': month = 12; break;
         } // switch
         year = compDateStr.substring(9,11).toInt();

         // Ask for RTC setting al compile date/time
         Serial.println("IOS: RTC clock failure!");
         Serial.print("\nDo you want set RTC at IOS compile time (");
         printDateTime(0);
         Serial.print(")? [Y/N] >");
         timeStamp = millis();
         do
         {
             blinkIOSled(&timeStamp);
             inChar = Serial.read();
         } while ((inChar != 'y') && (inChar != 'Y') && (inChar != 'n') &&(inChar != 'N'));
         Serial.println(inChar);
         
         // Set the RTC at the compile date/time and print a message
         if ((inChar == 'y') || (inChar == 'Y'))
         {
             writeRTC(seconds, minutes, hours, day, month, year);
             Serial.print("IOS: RTC set at compile time - Now: ");
             printDateTime(1);
             Serial.println();
         }

         // Reset the "Oscillator Stop Flag"
         Wire.beginTransmission(DS3231_RTC);
         Wire.write(DS3231_STATRG);                    // Set the DS3231 Status Register
         Wire.write(0x08);                             // Reset the "Oscillator Stop Flag" (32KHz output left enabled)
         Wire.endTransmission();
     }
     return 1;
 }

 // end of source file