/*
 * Generic.cpp
 *
 * Created: 26/09/2019
 *  Author: Mark Riley
 */ 

#include <avr/pgmspace.h>                 // Needed for PROGMEM
#include "Wire.h"                         // Needed for I2C bus
#include <EEPROM.h>                       // Needed for internal EEPROM R/W
#include "PetitFS.h"                      // Light handler for FAT16 and FAT32 filesystem on SD
#include "Monitor.h"                      // provide "monitor" functionality, assembler(A XXXX),
// dump(D XXXX), dissasssembler(U XXXX<,NUM_INSTRUCTIONS>),
// enter XXXX
#include "RealTimeClock.h"
#include "Generic.h"
#include "SdCardFunctions.h"

char inChar;  // Input char from serial


 // ------------------------------------------------------------------------------

 // Generic routines

 // ------------------------------------------------------------------------------
 void printBinaryByte(byte value)
 {
     for (byte mask = 0x80; mask; mask >>= 1)
     {
         Serial.print((mask & value) ? '1' : '0');
     }
 }

 // ------------------------------------------------------------------------------
 // Set INT_ to ACTIVE if there are received chars from serial to read and if
 // the interrupt generation is enabled
 // ------------------------------------------------------------------------------
 void serialEvent()
 {
     if ((Serial.available()) && Z80IntEnFlag)
     {
         digitalWrite(INT_, LOW);
     }
 }

 // ------------------------------------------------------------------------------
 // Blink led IOS using a timestamp
 // ------------------------------------------------------------------------------
 void blinkIOSled(unsigned long *timestamp)
 {
     if ((millis() - *timestamp) > 200)
     {
         digitalWrite(LED_IOS,!digitalRead(LED_IOS));
         *timestamp = millis();
     }
 }



 // ------------------------------------------------------------------------------
 // Convert a binary byte to a two digits BCD byte
 // ------------------------------------------------------------------------------
 byte decToBcd(byte val)
 {
     return( (val/10*16) + (val%10) );
 }

 // ------------------------------------------------------------------------------
 // Convert binary coded decimal to normal decimal numbers
 // ------------------------------------------------------------------------------
 byte bcdToDec(byte val)
 {
     return( (val/16*10) + (val%16) );
 }

 // ------------------------------------------------------------------------------
 // Print to serial the current date/time from the global variables.
 //
 // Flag readSourceFlag [0..1] usage:
 //    If readSourceFlag = 0 the RTC read is not done
 //    If readSourceFlag = 1 the RTC read is done (global variables are updated)
 // ------------------------------------------------------------------------------
 void printDateTime(byte readSourceFlag)
 {
     if (readSourceFlag)
     {
         readRTC(&seconds, &minutes, &hours, &day,  &month,  &year, &tempC);
     }
     print2digit(day);
     Serial.print("/");
     print2digit(month);
     Serial.print("/");
     print2digit(year);
     Serial.print(" ");
     print2digit(hours);
     Serial.print(":");
     print2digit(minutes);
     Serial.print(":");
     print2digit(seconds);
 }

 // ------------------------------------------------------------------------------
 // Print a byte [0..99] using 2 digit with leading zeros if needed
 // ------------------------------------------------------------------------------
 void print2digit(byte data)
 {
     if (data < 10)
     {
         Serial.print("0");
     }
     Serial.print(data);
 }

 // ------------------------------------------------------------------------------
 // Check if the year 2000+XX (where XX is the argument yearXX [00..99]) is a leap year.
 // Returns 1 if it is leap, 0 otherwise.
 // This function works in the [2000..2099] years range. It should be enough...
 // ------------------------------------------------------------------------------
 byte isLeapYear(byte yearXX)
 {
     if (((2000 + yearXX) % 4) == 0) return 1;
     else return 0;
 }
 
 
 // ------------------------------------------------------------------------------
 // Wait a key to continue
 // ------------------------------------------------------------------------------
 void waitKey(const String *prompt )
 {
     while (Serial.available() > 0) Serial.read();   // Flush serial Rx buffer
     if( prompt != NULL )
     {
         Serial.println(prompt->begin());
     }
     else
     {
         Serial.println(F("\r\n?"));
     }
     while(Serial.available() < 1);
 }

 // ------------------------------------------------------------------------------
 // Print the current Disk Set number and the OS name, if it is defined.
 // The OS name is inside the file defined in DS_OSNAME
 // ------------------------------------------------------------------------------
 void printOsName(byte currentDiskSet)
 {
     Serial.print("Disk Set ");
     Serial.print(currentDiskSet);
     OsName[2] = currentDiskSet + 48;    // Set the Disk Set
     openSD(OsName);                     // Open file with the OS name
     readSD(bufferSD, &numReadBytes);    // Read the OS name
     if (numReadBytes > 0)
     {
         // Print the OS name
         Serial.print(" (");
         Serial.print((const char *)bufferSD);
         Serial.print(")");
     }
 }

 // end of source file