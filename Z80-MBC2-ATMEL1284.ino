/* ------------------------------------------------------------------------------

Z80-MBC2-ATMEL1284

IOS - I/O  for Z80-MBC2 (Multi Boot Computer - Z80 128kB RAM @ 4/8Mhz @ Fosc = 16MHz)


Notes:

1:  This SW is ONLY for the Atmel1284(p) used as EEPROM and I/O subsystem (16MHz external oscillator).
    
2:  Tested on Atmega1284(p) @ Arduino IDE 1.8.9.

3:  Embedded FW: S200718 iLoad (Intel-Hex loader)

4:  To run the stand-alone Basic and Forth interpreters the SD optional module must be installed with 
    the required binary files on a microSD (FAT16 or FAT32 formatted)

5:  Utilities:   S111216 TASM conversion utility


---------------------------------------------------------------------------------

Credits:

SD library from: https://github.com/greiman/PetitFS (based on 
PetitFS: http://elm-chan.org/fsw/ff/00index_p.html)

PetitFS licence:
/-----------------------------------------------------------------------------/
/  Petit FatFs - FAT file system module  R0.03                  (C)ChaN, 2014
/-----------------------------------------------------------------------------/
/ Petit FatFs module is a generic FAT file system module for small embedded
/ systems. This is a free software that opened for education, research and
/ commercial developments under license policy of following trems.
/
/  Copyright (C) 2014, ChaN, all right reserved.
/
/ * The Petit FatFs module is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-----------------------------------------------------------------------------/

CHANGELOG:


S220718            First revision.
S220718-R010918    Added "Disk Set" feature to manage multiple OS on SD (multi-booting).
                   Added support for QP/M 2.71 (with file names timestamping).
                   Added support for Atmega32A @ 20MHz (overclocked) to show the Z80 clock speed 
                   accordingly (Note that 20MHz is out of Atmega32A specifications!).
S220718-R190918    Added support for CP/M 3.
                   Fixed a bug in the manual RTC setting.
S220718-R260119    Changed the default serial speed to 115200 bps.
                   Added support for xmodem protocol (extended serial Rx buffer check and  
                   two new flags into the SYSFLAG opcode for full 8 bit serial I/O control.
                   Added support for uTerm (A071218-R250119) reset at boot time.
S220718-R280819    Added a new Disk Set for the UCSD Pascal implementation (porting by Michel Bernard)
Z80-MBC2-ATMEL1284 Forked from S220718-R280819 - new project.  Changed register names.
                   Improved readability.

--------------------------------------------------------------------------------- */

// ------------------------------------------------------------------------------
//
// Atmega clock speed check
//
// ------------------------------------------------------------------------------

#if F_CPU == 20000000   
    #define CLOCK_LOW   "5"
    #define CLOCK_HIGH  "10"
#else
    #define CLOCK_LOW   "4"
    #define CLOCK_HIGH  "8"
#endif

// ------------------------------------------------------------------------------
//
//  Libraries
//
// ------------------------------------------------------------------------------

#include <avr/pgmspace.h>                 // Needed for PROGMEM
#include "Wire.h"                         // Needed for I2C bus
#include <EEPROM.h>                       // Needed for internal EEPROM R/W
#include "PetitFS.h"                      // Light handler for FAT16 and FAT32 filesystem on SD
#include "DefinitionsFile.h"
#include "Monitor.h"                      // provide "monitor" functionality, assembler(A XXXX), 
                                          // dump(D XXXX), dissasssembler(U XXXX<,NUM_INSTRUCTIONS>), 
                                          // enter XXXX 
#include "RealTimeClock.h"
#include "Generic.h"
#include "SdCardFunctions.h"



// ------------------------------------------------------------------------------
//
//  Constants
//
// ------------------------------------------------------------------------------
const byte    debug        = 0;           // Debug off = 0, on = 1, on = 2 with interrupt trace
const byte    bootModeAddr = 10;          // Internal EEPROM address for boot mode storage
const byte    autoexecFlagAddr = 12;      // Internal EEPROM address for AUTOEXEC flag storage
const byte    clockModeAddr = 13;         // Internal EEPROM address for the Z80 clock high/low speed switch
                                          //  (1 = low speed, 0 = high speed)
const byte    diskSetAddr  = 14;          // Internal EEPROM address for the current Disk Set [0..9]
const byte    maxDiskNum   = 99;          // Max number of virtual disks
const byte    maxDiskSet   = 4;           // Number of configured Disk Sets

// Z80 programs images into flash and related constants
const word  boot_A_StrAddr = 0xfd10;      // Payload A image starting address (flash)
const byte  boot_A_[] PROGMEM = {         // Payload A image (S200718 iLoad)
    0x31, 0x10, 0xFD, 0x21, 0x52, 0xFD, 0xCD, 0xC6, 0xFE, 0xCD, 0x3E, 0xFF, 0xCD, 0xF4, 0xFD, 0x3E, 
    0xFF, 0xBC, 0x20, 0x10, 0xBD, 0x20, 0x0D, 0x21, 0xD9, 0xFD, 0xCD, 0xC6, 0xFE, 0x21, 0x88, 0xFD, 
    0xCD, 0xC6, 0xFE, 0x76, 0xE5, 0x21, 0xD9, 0xFD, 0xCD, 0xC6, 0xFE, 0x21, 0x75, 0xFD, 0xCD, 0xC6, 
    0xFE, 0xE1, 0xCD, 0x4B, 0xFF, 0xCD, 0x3E, 0xFF, 0xCD, 0x3E, 0xFF, 0xDB, 0x01, 0xFE, 0xFF, 0x20, 
    0xFA, 0xE9, 0x69, 0x4C, 0x6F, 0x61, 0x64, 0x20, 0x2D, 0x20, 0x49, 0x6E, 0x74, 0x65, 0x6C, 0x2D, 
    0x48, 0x65, 0x78, 0x20, 0x4C, 0x6F, 0x61, 0x64, 0x65, 0x72, 0x20, 0x2D, 0x20, 0x53, 0x32, 0x30, 
    0x30, 0x37, 0x31, 0x38, 0x00, 0x53, 0x74, 0x61, 0x72, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x41, 0x64, 
    0x64, 0x72, 0x65, 0x73, 0x73, 0x3A, 0x20, 0x00, 0x4C, 0x6F, 0x61, 0x64, 0x20, 0x65, 0x72, 0x72, 
    0x6F, 0x72, 0x20, 0x2D, 0x20, 0x53, 0x79, 0x73, 0x74, 0x65, 0x6D, 0x20, 0x68, 0x61, 0x6C, 0x74, 
    0x65, 0x64, 0x00, 0x57, 0x61, 0x69, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x69, 0x6E, 0x70, 0x75, 0x74, 
    0x20, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6D, 0x2E, 0x2E, 0x2E, 0x00, 0x53, 0x79, 0x6E, 0x74, 0x61, 
    0x78, 0x20, 0x65, 0x72, 0x72, 0x6F, 0x72, 0x21, 0x00, 0x43, 0x68, 0x65, 0x63, 0x6B, 0x73, 0x75, 
    0x6D, 0x20, 0x65, 0x72, 0x72, 0x6F, 0x72, 0x21, 0x00, 0x69, 0x4C, 0x6F, 0x61, 0x64, 0x3A, 0x20, 
    0x00, 0x41, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x20, 0x76, 0x69, 0x6F, 0x6C, 0x61, 0x74, 0x69, 
    0x6F, 0x6E, 0x21, 0x00, 0xF5, 0xD5, 0xC5, 0x01, 0xFF, 0xFF, 0x21, 0xA3, 0xFD, 0xCD, 0xC6, 0xFE, 
    0xCD, 0x3E, 0xFF, 0xCD, 0x72, 0xFF, 0xFE, 0x0D, 0x28, 0xF9, 0xFE, 0x0A, 0x28, 0xF5, 0xFE, 0x20, 
    0x28, 0xF1, 0xCD, 0x1A, 0xFF, 0xCD, 0x69, 0xFF, 0xFE, 0x3A, 0xC2, 0xA3, 0xFE, 0xCD, 0xE1, 0xFE, 
    0x57, 0x1E, 0x00, 0xCD, 0xBE, 0xFE, 0xCD, 0xD6, 0xFE, 0x3E, 0xFF, 0xB8, 0x20, 0x05, 0xB9, 0x20, 
    0x02, 0x44, 0x4D, 0x7C, 0xCD, 0xBE, 0xFE, 0x7D, 0xCD, 0xBE, 0xFE, 0xCD, 0xE1, 0xFE, 0xCD, 0xBE, 
    0xFE, 0xFE, 0x01, 0x20, 0x1E, 0xCD, 0xE1, 0xFE, 0xCD, 0xBE, 0xFE, 0x7B, 0xA7, 0x28, 0x66, 0xCD, 
    0x3E, 0xFF, 0x21, 0xD9, 0xFD, 0xCD, 0xC6, 0xFE, 0x21, 0xC9, 0xFD, 0xCD, 0xC6, 0xFE, 0x01, 0xFF, 
    0xFF, 0x18, 0x52, 0x7A, 0xA7, 0x28, 0x2C, 0xCD, 0xE1, 0xFE, 0xCD, 0xBE, 0xFE, 0xE5, 0xC5, 0xA7, 
    0x01, 0xF0, 0xFC, 0xED, 0x42, 0xC1, 0xE1, 0xDA, 0x8E, 0xFE, 0xCD, 0x3E, 0xFF, 0x21, 0xD9, 0xFD, 
    0xCD, 0xC6, 0xFE, 0x21, 0xE1, 0xFD, 0xCD, 0xC6, 0xFE, 0x01, 0xFF, 0xFF, 0x18, 0x27, 0x77, 0x23, 
    0x15, 0x18, 0xD0, 0xCD, 0xE1, 0xFE, 0xCD, 0xBE, 0xFE, 0x7B, 0xA7, 0x20, 0xB2, 0xCD, 0x3E, 0xFF, 
    0xC3, 0x03, 0xFE, 0xCD, 0x3E, 0xFF, 0x21, 0xD9, 0xFD, 0xCD, 0xC6, 0xFE, 0x21, 0xBB, 0xFD, 0xCD, 
    0xC6, 0xFE, 0x01, 0xFF, 0xFF, 0xCD, 0x3E, 0xFF, 0x60, 0x69, 0xC1, 0xD1, 0xF1, 0xC9, 0xC5, 0x4F, 
    0x7B, 0x91, 0x5F, 0x79, 0xC1, 0xC9, 0xF5, 0xE5, 0x7E, 0xFE, 0x00, 0x28, 0x06, 0xCD, 0x69, 0xFF, 
    0x23, 0x18, 0xF5, 0xE1, 0xF1, 0xC9, 0xF5, 0xCD, 0xE1, 0xFE, 0x67, 0xCD, 0xE1, 0xFE, 0x6F, 0xF1, 
    0xC9, 0xC5, 0xCD, 0xF4, 0xFE, 0xCB, 0x07, 0xCB, 0x07, 0xCB, 0x07, 0xCB, 0x07, 0x47, 0xCD, 0xF4, 
    0xFE, 0xB0, 0xC1, 0xC9, 0xCD, 0x72, 0xFF, 0xCD, 0x1A, 0xFF, 0xCD, 0x06, 0xFF, 0x30, 0xF5, 0xCD, 
    0x23, 0xFF, 0xCD, 0x2E, 0xFF, 0xC9, 0xFE, 0x47, 0xD0, 0xFE, 0x30, 0x30, 0x02, 0x3F, 0xC9, 0xFE, 
    0x3A, 0xD8, 0xFE, 0x41, 0x30, 0x02, 0x3F, 0xC9, 0x37, 0xC9, 0xFE, 0x61, 0xD8, 0xFE, 0x7B, 0xD0, 
    0xE6, 0x5F, 0xC9, 0xFE, 0x3A, 0x38, 0x02, 0xD6, 0x07, 0xD6, 0x30, 0xE6, 0x0F, 0xC9, 0xF5, 0xE6, 
    0x0F, 0xC6, 0x30, 0xFE, 0x3A, 0x38, 0x02, 0xC6, 0x07, 0xCD, 0x69, 0xFF, 0xF1, 0xC9, 0xF5, 0x3E, 
    0x0D, 0xCD, 0x69, 0xFF, 0x3E, 0x0A, 0xCD, 0x69, 0xFF, 0xF1, 0xC9, 0xE5, 0xF5, 0x7C, 0xCD, 0x58, 
    0xFF, 0x7D, 0xCD, 0x58, 0xFF, 0xF1, 0xE1, 0xC9, 0xF5, 0xC5, 0x47, 0x0F, 0x0F, 0x0F, 0x0F, 0xCD, 
    0x2E, 0xFF, 0x78, 0xCD, 0x2E, 0xFF, 0xC1, 0xF1, 0xC9, 0xF5, 0x3E, 0x01, 0xD3, 0x01, 0xF1, 0xD3, 
    0x00, 0xC9, 0xDB, 0x01, 0xFE, 0xFF, 0xCA, 0x72, 0xFF, 0xC9 };

const byte * const flashBootTable[1] PROGMEM = {boot_A_}; // Payload pointers table (flash)

/* internal Strings */
const String *SD_ERROR_RETRY  = new String("IOS: Check SD and press a key to repeat\r\n");

// ------------------------------------------------------------------------------
//
//  Global variables
//
// ------------------------------------------------------------------------------

// General purpose variables
byte          ioAddress;                  // Virtual I/O address. Only two possible addresses are valid (0x00 and 0x01)
byte          ioData;                     // Data byte used for the I/O operation
byte          ioOpcode       = 0xFF;      // I/O operation code or Opcode (0xFF means "No Operation")
word          ioByteCnt;                  // Exchanged bytes counter during an I/O operation
byte          moduleGPIO     = 0;         // Set to 1 if the module is found, 0 otherwise
byte          bootMode       = 0;         // Set the program to boot (from flash or SD)
byte *        BootImage;                  // Pointer to selected flash payload array (image) to boot
word          BootImageSize  = 0;         // Size of the selected flash payload array (image) to boot
word          BootStrAddr;                // Starting address of the selected program to boot (from flash or SD)

byte          iCount;                     // Temporary variable (counter)
byte          clockMode;                  // Z80 clock HI/LO speed selector (0 = 8/10MHz, 1 = 4/5MHz)
byte          LastRxIsEmpty;              // "Last Rx char was empty" flag. Is set when a serial Rx operation was done
                                          // when the Rx buffer was empty
byte          tempByte;

// ------------------------------------------------------------------------------

void setup() 
{
// ------------------------------------------------------------------------------
//  Local variables
// ------------------------------------------------------------------------------
    char minBootChar   = '1';        // Minimum allowed ASCII value selection (boot selection)
    char maxSelChar    = '8';        // Maximum allowed ASCII value selection (boot selection)
    byte maxBootMode   = 4;          // Default maximum allowed value for bootMode [0..4]
    byte bootSelection = 0;          // Flag to enter into the boot mode selection

// ----------------------------------------
// INITIALIZATION
// ----------------------------------------

    // Initialize RESET_ and WAIT_RES_
    pinMode(RESET_, OUTPUT);                        // Configure RESET_ and set it ACTIVE
    digitalWrite(RESET_, LOW);
    pinMode(WAIT_RES_, OUTPUT);                     // Configure WAIT_RES_ and set it ACTIVE to reset the WAIT FF (U1C/D)
    digitalWrite(WAIT_RES_, LOW);

    // Check USER Key for boot mode changes 
    pinMode(USER, INPUT_PULLUP);                    // Read USER Key to enter into the boot mode selection
    if (!digitalRead(USER)) 
    {
        bootSelection = 1;
    }

    // Initialize USER,  INT_, RAM_CE2, and BUSREQ_
    pinMode(USER, OUTPUT);                          // USER led OFF
    digitalWrite(USER, HIGH);
    pinMode(INT_, INPUT_PULLUP);                    // Configure INT_ and set it NOT ACTIVE
    pinMode(INT_, OUTPUT);
    digitalWrite(INT_, HIGH);
    pinMode(RAM_CE2, OUTPUT);                       // Configure RAM_CE2 as output
    digitalWrite(RAM_CE2, HIGH);                    // Set RAM_CE2 active
    pinMode(WAIT_, INPUT);                          // Configure WAIT_ as input
    pinMode(BUSREQ_, INPUT_PULLUP);                 // Set BUSREQ_ HIGH
    pinMode(BUSREQ_, OUTPUT);
    digitalWrite(BUSREQ_, HIGH);

    // Initialize D0-D7, AD0, MREQ_, RD_ and WR_
    DDRA = 0x00;                                    // Configure Z80 data bus D0-D7 (PA0-PA7) as input with pull-up
    PORTA = 0xFF;
    pinMode(MREQ_, INPUT_PULLUP);                   // Configure MREQ_ as input with pull-up
    pinMode(RD_, INPUT_PULLUP);                     // Configure RD_ as input with pull-up
    pinMode(WR_, INPUT_PULLUP);                     // Configure WR_ as input with pull-up
    pinMode(AD0, INPUT_PULLUP);

    // Initialize the Logical RAM Bank (32KB) to map into the lower half of the Z80 addressing space
    pinMode(BANK0, OUTPUT);                         // Set RAM Logical Bank 1 (Os Bank 0)
    digitalWrite(BANK0, HIGH);
    pinMode(BANK1, OUTPUT);
    digitalWrite(BANK1, LOW);

    // Initialize CLK (single clock pulses mode) and reset the Z80 CPU
    pinMode(CLK, OUTPUT);                           // Set CLK as output
    singlePulsesResetZ80();                         // Reset the Z80 CPU using single clock pulses

    // Initialize MCU_RTS and MCU_CTS and reset uTerm (A071218-R250119) if present
    pinMode(MCU_CTS_, INPUT_PULLUP);                // Parked (not used)
    pinMode(MCU_RTS_, OUTPUT);
    digitalWrite(MCU_RTS_, LOW);                    // Reset uTerm (A071218-R250119)
    delay(100); 
    digitalWrite(MCU_RTS_, HIGH); 
    delay(500);

    // Read the Z80 CPU speed mode
    if (EEPROM.read(clockModeAddr) > 1)             // Check if it is a valid value, otherwise set it to low speed
    {
        // Not a valid value. Set it to low speed
        EEPROM.update(clockModeAddr, 1);
    }
    clockMode = EEPROM.read(clockModeAddr);         // Read the previous stored value

    // Read the stored Disk Set. If not valid set it to 0
    diskSet = EEPROM.read(diskSetAddr);
    if (diskSet >= maxDiskSet) 
    {
        EEPROM.update(diskSetAddr, 0);
        diskSet =0;
    }

    // Initialize the EXP_PORT (I2C) and search for "known" optional modules
    Wire.begin();                                   // Wake up I2C bus
    Wire.beginTransmission(GPIOEXP_ADDR);
    if (Wire.endTransmission() == 0) 
    {
        moduleGPIO = 1;// Set to 1 if GPIO Module is found
    }
    
    // Print some system information
    Serial.begin(115200);
    Serial.println(F("\r\n\nZ80-MBC2 - A040618\r\nIOS - I/O Subsystem - S220718-R280819\r\n"));

    // Print if the input serial buffer is 128 bytes wide (this is needed for xmodem protocol support)
    if (SERIAL_RX_BUFFER_SIZE >= 128) 
    {
        Serial.println(F("IOS: Found extended serial Rx buffer"));
    }

    // Print the Z80 clock speed mode
    Serial.print(F("IOS: Z80 clock set at "));
    if (clockMode) 
    {
        Serial.print(CLOCK_LOW);
    }
    else 
    {
        Serial.print(CLOCK_HIGH);
    }
    Serial.println("MHz");

    // Print RTC and GPIO informations if found
    foundRTC = autoSetRTC();                        // Check if RTC is present and initialize it as needed
    if (moduleGPIO) 
    {
        Serial.println(F("IOS: Found GPE Option"));
    }
    
    // Print CP/M Autoexec on cold boot status
    Serial.print(F("IOS: CP/M Autoexec is "));
    if (EEPROM.read(autoexecFlagAddr) > 1) 
    {
        EEPROM.update(autoexecFlagAddr, 0); // Reset AUTOEXEC flag to OFF if invalid
    }
    autoexecFlag = EEPROM.read(autoexecFlagAddr);   // Read the previous stored AUTOEXEC flag
    if (autoexecFlag) 
    {
        Serial.println("ON");
    }
    else 
    {
        Serial.println("OFF");
    }
    
// ----------------------------------------
// BOOT SELECTION AND SYS PARAMETERS MENU
// ----------------------------------------

    // Boot selection and system parameters menu if requested
    mountSD(&filesysSD); mountSD(&filesysSD);       // Try to muont the SD volume
    bootMode = EEPROM.read(bootModeAddr);           // Read the previous stored boot mode
    
    // Enter in the boot selection menu if USER key was pressed at startup 
    //   or an invalid bootMode code was read from internal EEPROM
    if ((bootSelection == 1 ) || (bootMode > maxBootMode))
    {
        while (Serial.available() > 0)                // Flush input serial Rx buffer
        {
            Serial.read();
        }
        Serial.println();
        Serial.println(F("MIOS: Select boot mode or system parameters:"));
        Serial.println();
        
        // Previous valid boot mode read, so enable '0' selection    
        if (bootMode <= maxBootMode)
        {
            minBootChar = '0';
            Serial.print(F(" 0: No change ("));
            Serial.print(bootMode + 1);
            Serial.println(")");
        }
        Serial.println(F(" 1: Basic"));   
        Serial.println(F(" 2: Forth"));
        Serial.print(F(" 3: Load OS from "));
        printOsName(diskSet);
        Serial.println(F("\r\n 4: Autoboot"));
        Serial.println(F(" 5: iLoad"));
        Serial.print(F(" 6: Change Z80 clock speed (->"));
        if (clockMode) Serial.print(CLOCK_HIGH);
        else Serial.print(CLOCK_LOW);
        Serial.println("MHz)");
        Serial.print(" 7: Toggle CP/M Autoexec (->");
        if (!autoexecFlag) Serial.print("ON");
        else Serial.print("OFF");
        Serial.println(")");
        Serial.print(" 8: Change ");
        printOsName(diskSet);
        Serial.println();
        Serial.println(" M: Start Monitor" );
        

        // If RTC module is present add a menu choice
        if (foundRTC)
        {
            Serial.println(" 9: Change RTC time/date");
            maxSelChar = '9';
        }

        // Ask a choice
        Serial.println();
        timeStamp = millis();
        Serial.print("Enter your choice >");
        do
        {
            blinkIOSled(&timeStamp);
            inChar = Serial.read();
            if ( inChar == 'M' ) break;
        } while ((inChar < minBootChar) || (inChar > maxSelChar));
        
        Serial.print(inChar);
        Serial.println("  Ok");

        // Make the selected action for the system parameters choice
        switch (inChar)
        {
            case '6':                                   // Change the clock speed of the Z80 CPU
                clockMode = !clockMode;                   // Toggle Z80 clock speed mode (High/Low)
                EEPROM.update(clockModeAddr, clockMode);  // Save it to the internal EEPROM
                break;

            case '7':                                   // Toggle CP/M AUTOEXEC execution on cold boot
                autoexecFlag = !autoexecFlag;             // Toggle AUTOEXEC execution status
                EEPROM.update(autoexecFlagAddr, autoexecFlag); // Save it to the internal EEPROM
                break;

            case '8':                                   // Change current Disk Set
                Serial.println(F("\r\nPress CR to accept, ESC to exit or any other key to change"));
                iCount = diskSet;
                do
                {
                    // Print the OS name of the next Disk Set
                    iCount = (iCount + 1) % maxDiskSet;
                    Serial.print("\r ->");
                    printOsName(iCount);
                    Serial.print(F("                 \r"));
                    while (Serial.available() > 0) 
                    {
                        Serial.read();   // Flush serial Rx buffer
                    }
                    
                    while(Serial.available() < 1) 
                    {
                        blinkIOSled(&timeStamp);  // Wait a key
                    }
                    
                    inChar = Serial.read();
                } while ((inChar != 13) && (inChar != 27)); // Continue until a CR or ESC is pressed
                
                Serial.println();
                Serial.println();
                if (inChar == 13)                         // Set and store the new Disk Set if required
                {
                   diskSet = iCount;
                   EEPROM.update(diskSetAddr, iCount);
                }
                break;

            case '9':                                   // Change RTC Date/Time
                ChangeRTC();                              // Change RTC Date/Time if requested
                break;

            case 'm':
            case 'M':
                monitor();
                break;
        } // switch
    
        // Save selected boot program if changed
        bootMode = inChar - '1';                      // Calculate bootMode from inChar
        if (bootMode <= maxBootMode) 
        {
            EEPROM.update(bootModeAddr, bootMode); // Save to the internal EEPROM if required
        }
        else 
        {
            bootMode = EEPROM.read(bootModeAddr);    // Reload boot mode if '0' or > '5' choice selected
        }
    } // if

    // Print current Disk Set and OS name (if OS boot is enabled)
    if (bootMode == 2)
    {
        Serial.print(F("IOS: Current "));
        printOsName(diskSet);
        Serial.println();
    }

// ----------------------------------------
// Z80 PROGRAM LOAD
// ----------------------------------------

    // Get the starting address of the program to load and boot, and its size if stored in the flash
    switch (bootMode)
    {
        case 0:                                       // Load Basic from SD
            fileNameSD = BASICFN;
            BootStrAddr = BASSTRADDR;
            Z80IntEnFlag = 1;                         // Enable INT_ signal generation (Z80 M1 INT I/O)
            break;
    
        case 1:                                       // Load Forth from SD
            fileNameSD = FORTHFN;
            BootStrAddr = FORSTRADDR;
            break;

        case 2:                                       // Load an OS from current Disk Set on SD
            switch (diskSet)
            {
                case 0:                               // CP/M 2.2
                    fileNameSD = CPMFN;
                    BootStrAddr = CPMSTRADDR;
                    break;

                case 1:                               // QP/M 2.71
                    fileNameSD = QPMFN;
                    BootStrAddr = QPMSTRADDR;
                    break;

                case 2:                               // CP/M 3.0
                    fileNameSD = CPM3FN;
                    BootStrAddr = CPM3STRADDR;
                    break;

                case 3:                               // UCSD Pascal
                    fileNameSD = UCSDFN;
                    BootStrAddr = UCSDSTRADDR;
                    break;
            }
            break;
    
            case 3:                                   // Load AUTOBOOT.BIN from SD (load an user executable binary file)
                fileNameSD = AUTOFN;
                BootStrAddr = AUTSTRADDR;
                break;
    
            case 4:                                   // Load iLoad from flash
                BootImage = (byte *) pgm_read_word (&flashBootTable[0]); 
                BootImageSize = sizeof(boot_A_);
                BootStrAddr = boot_A_StrAddr;
                break;
    }
    
    digitalWrite(WAIT_RES_, HIGH);                  // Set WAIT_RES_ HIGH (Led LED_0 ON)
  
    // Load a JP instruction if the boot program starting addr is > 0x0000
    if (BootStrAddr > 0x0000)                       // Check if the boot program starting addr > 0x0000
    {
        // Inject a "JP <BootStrAddr>" instruction to jump at boot starting address
        loadHL(0x0000);                             // HL = 0x0000 (used as pointer to RAM)
        writeByteToRAM(JP_nn);                       // Write the JP opcode @ 0x0000;
        writeByteToRAM(lowByte(BootStrAddr));        // Write LSB to jump @ 0x0001
        writeByteToRAM(highByte(BootStrAddr));       // Write MSB to jump @ 0x0002
        //
        // DEBUG ----------------------------------
        if (debug)
        {
            Serial.print("DEBUG: Injected JP 0x");
            Serial.println(BootStrAddr, HEX);
        }
        // DEBUG END ------------------------------
        //
    }

    // Execute the load of the selected file on SD or image on flash
    loadHL(BootStrAddr);                            // Set Z80 HL = boot starting address (used as pointer to RAM);
    //
    // DEBUG ----------------------------------
    if (debug)
    {
        Serial.print("DEBUG: Flash BootImageSize = ");
        Serial.println(BootImageSize);
        Serial.print("DEBUG: BootStrAddr = ");
        Serial.println(BootStrAddr, HEX);    
    }
    // DEBUG END ------------------------------
    //
    
    // Load from SD
    if (bootMode < maxBootMode)
    {
        // Mount a volume on SD
        if (mountSD(&filesysSD))
        {
            // Error mounting. Try again
            errCodeSD = mountSD(&filesysSD);
            if (errCodeSD)
            {
                // Error again. Repeat until error disappears (or the user forces a reset)
                do
                {
                    printErrSD(0, errCodeSD, NULL);
                    waitKey(SD_ERROR_RETRY);                                // Wait a key to repeat
                    mountSD(&filesysSD);                      // New double try
                    errCodeSD = mountSD(&filesysSD);
                } while (errCodeSD);
            }
        }

        // Open the selected file to load
        errCodeSD = openSD(fileNameSD);
        if (errCodeSD)
        {
            // Error opening the required file. Repeat until error disappears (or the user forces a reset)
            do
            {
                printErrSD(1, errCodeSD, fileNameSD);
                waitKey(SD_ERROR_RETRY);                                  // Wait a key to repeat
                errCodeSD = openSD(fileNameSD);
                if (errCodeSD != 3)
                {
                    // Try to do a two mount operations followed by an open
                    mountSD(&filesysSD);
                    mountSD(&filesysSD);
                    errCodeSD = openSD(fileNameSD);
                }
            } while (errCodeSD);
        }
        
        // Read the selected file from SD and load it into RAM until an EOF is reached
        Serial.print("IOS: Loading boot program (");
        Serial.print(fileNameSD);
        Serial.print(")...");

        // If an error occurs repeat until error disappears (or the user forces a reset)
        do
        {
            // Read a "segment" of a SD sector and load it into RAM
            do
            {
                errCodeSD = readSD(bufferSD, &numReadBytes);  // Read current "segment" (32 bytes) of the current SD serctor
                
                // Load the read "segment" into RAM
                for (iCount = 0; iCount < numReadBytes; iCount++)
                {
                    writeByteToRAM(bufferSD[iCount]);        // Write current data byte into RAM
                }
            } while ((numReadBytes == 32) && (!errCodeSD));   // If numReadBytes < 32 -> EOF reached
            
            if (errCodeSD)
            {
                printErrSD(2, errCodeSD, fileNameSD);
                waitKey(SD_ERROR_RETRY);                  // Wait a key to repeat
                seekSD(0);                                // Reset the sector pointer
            }
        } while (errCodeSD);
    }
    else
    {
        // Load from flash
        Serial.print("IOS: Loading boot program...");
        // Write boot program into external RAM
        for (word i = 0; i < BootImageSize; i++)
        {
            writeByteToRAM(pgm_read_byte(BootImage + i));  // Write current data byte into RAM
        }
    }
    
    Serial.println(" Done");

// ----------------------------------------
// Z80 BOOT - Registers as per Atmel1284(p)
// ----------------------------------------
    digitalWrite(RESET_, LOW);              // Activate the RESET_ signal

    // Initialize CLK @ 4/8MHz (@ Fosc = 16MHz). Z80 clock_freq = (Atmega_clock) / ((OCR2 + 1) * 2)
    ASSR &=    ~(1 << AS2);                 // Set Timer2 clock from system clock
    TCCR2B |=   (1 << CS20);                // Set Timer2 clock to "no prescaling"
    TCCR2B &= ~((1 << CS21) | (1 << CS22));
    TCCR2A |=   (1 << WGM21);               // Set Timer2 CTC mode
    TCCR2A &=  ~(1 << WGM20);
    TCCR2A |=   (1 << COM2A0);              // Set "toggle OC2 on compare match"
    TCCR2A &=  ~(1 << COM2A1);
    OCR2A = clockMode;                      // Set the compare value to toggle OC2 (0 = low or 1 = high)
    pinMode(CLK, OUTPUT);                   // Set OC2 as output and start to output the clock
    
    Serial.println("IOS: Z80 is running from now");
    Serial.println();

    // Flush serial Rx buffer
    while (Serial.available() > 0) 
    {
        Serial.read();
    }

    // Leave the Z80 CPU running
    delay(1);                                       // Just to be sure...
    digitalWrite(RESET_, HIGH);                     // Release Z80 from reset and let it run
}

// ------------------------------------------------------------------------------
// Main Processing Loop
// ------------------------------------------------------------------------------
void loop() 
{
    if (!digitalRead(WAIT_))
    { // I/O operation requested
        if (!digitalRead(WR_))
        {// I/O WRITE operation requested
            // ----------------------------------------
            // VIRTUAL I/O WRITE OPERATIONS ENGINE
            // ----------------------------------------
            ioAddress = digitalRead(AD0);               // Read Z80 address bus line AD0 (PC2)
            ioData = PINA;                              // Read Z80 data bus D0-D7 (PA0-PA7)
            if (ioAddress)                              // Check the I/O address (only AD0 is checked!)
            {
                // .........................................................................................................
                //
                // AD0 = 1 (I/O write address = 0x01). STORE OPCODE.
                //
                // Store (write) an "I/O operation code" (Opcode) and reset the exchanged bytes counter.
                //
                // NOTE 1: An Opcode can be a write or read Opcode, if the I/O operation is read or write.
                // NOTE 2: the STORE OPCODE operation must always precede an EXECUTE WRITE OPCODE or EXECUTE READ OPCODE 
                //         operation.
                // NOTE 3: For multi-byte read opcode (as DATETIME) read sequentially all the data bytes without to send
                //         a STORE OPCODE operation before each data byte after the first one.
                // .........................................................................................................
                //
                // Currently defined Opcodes for I/O write operations:
                //
                //   Opcode     Name            Exchanged bytes
                // -------------------------------------------------
                // Opcode 0x00  USER LED        1
                // Opcode 0x01  SERIAL TX       1
                // Opcode 0x03  GPIOA Write     1
                // Opcode 0x04  GPIOB Write     1
                // Opcode 0x05  IODIRA Write    1
                // Opcode 0x06  IODIRB Write    1
                // Opcode 0x07  GPPUA Write     1
                // Opcode 0x08  GPPUB Write     1
                // Opcode 0x09  SELDISK         1
                // Opcode 0x0A  SELTRACK        2
                // Opcode 0x0B  SELSECT         1  
                // Opcode 0x0C  WRITESECT       512
                // Opcode 0x0D  SETBANK         1
                // Opcode 0xFF  No operation    1
                //
                //
                // Currently defined Opcodes for I/O read operations:
                //
                //   Opcode     Name            Exchanged bytes
                // -------------------------------------------------
                // Opcode 0x80  USER KEY        1
                // Opcode 0x81  GPIOA Read      1
                // Opcode 0x82  GPIOB Read      1
                // Opcode 0x83  SYSFLAGS        1
                // Opcode 0x84  DATETIME        7
                // Opcode 0x85  ERRDISK         1
                // Opcode 0x86  READSECT        512
                // Opcode 0x87  SDMOUNT         1
                // Opcode 0xFF  No operation    1
                //
                // See the following lines for the Opcodes details.
                // 
                // .........................................................................................................     
                ioOpcode = ioData;                        // Store the I/O operation code (Opcode)
                ioByteCnt = 0;                            // Reset the exchanged bytes counter
            }
            else
            {
                // .........................................................................................................
                //
                // AD0 = 0 (I/O write address = 0x00). EXECUTE WRITE OPCODE.
                //
                // Execute the previously stored I/O write opcode with the current data.
                // The code of the I/O write operation (Opcode) must be previously stored with a STORE OPCODE operation.
                // .........................................................................................................
                //
                // Execute the requested I/O WRITE Opcode. The 0xFF value is reserved as "No operation".
                switch (ioOpcode)
                {
                    // USER LED:      
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                              x  x  x  x  x  x  x  0    USER Led off
                    //                              x  x  x  x  x  x  x  1    USER Led on
                    case  0x00:
                        if (ioData & B00000001)
                        {
                            digitalWrite(USER, LOW); 
                        }
                        else 
                        {
                            digitalWrite(USER, HIGH);
                        }
                        break;

                    // SERIAL TX:     
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    ASCII char to be sent to serial
                    case  0x01:
                        Serial.write(ioData);
                        break;

                    // GPIOA Write (GPE Option):
                    //
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    GPIOA value (see MCP23017 datasheet)
                    case  0x03:
                        if (moduleGPIO) 
                        {
                            Wire.beginTransmission(GPIOEXP_ADDR);
                            Wire.write(GPIOA_REG);                // Select GPIOA
                            Wire.write(ioData);                   // Write value
                            Wire.endTransmission();
                        }
                        break;

                    // GPIOB Write (GPE Option): 
                    //   
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    GPIOB value (see MCP23017 datasheet)
                    case  0x04:
                        if (moduleGPIO) 
                        {
                            Wire.beginTransmission(GPIOEXP_ADDR);
                            Wire.write(GPIOB_REG);                // Select GPIOB
                            Wire.write(ioData);                   // Write value
                            Wire.endTransmission();
                        }
                        break;

                    // IODIRA Write (GPE Option):
                    //
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    IODIRA value (see MCP23017 datasheet)
                    case  0x05:
                        if (moduleGPIO) 
                        {
                            Wire.beginTransmission(GPIOEXP_ADDR);
                            Wire.write(IODIRA_REG);               // Select IODIRA
                            Wire.write(ioData);                   // Write value
                            Wire.endTransmission();
                        }
                        break;

                    // IODIRB Write (GPE Option):
                    //
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    IODIRB value (see MCP23017 datasheet)
                    case  0x06:
                        if (moduleGPIO) 
                        {
                            Wire.beginTransmission(GPIOEXP_ADDR);
                            Wire.write(IODIRB_REG);               // Select IODIRB
                            Wire.write(ioData);                   // Write value
                            Wire.endTransmission();
                        }
                        break;

                    // GPPUA Write (GPE Option):
                    //
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    GPPUA value (see MCP23017 datasheet)
                    case  0x07:
                        if (moduleGPIO) 
                        {
                            Wire.beginTransmission(GPIOEXP_ADDR);
                            Wire.write(GPPUA_REG);                // Select GPPUA
                            Wire.write(ioData);                   // Write value
                            Wire.endTransmission();
                        }
                        break;

                    // GPPUB Write (GPIO Exp. Mod. ):
                    //
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    GPPUB value (see MCP23017 datasheet)
                    case  0x08:
                        if (moduleGPIO) 
                        {
                            Wire.beginTransmission(GPIOEXP_ADDR);
                            Wire.write(GPPUB_REG);                // Select GPPUB
                            Wire.write(ioData);                   // Write value
                            Wire.endTransmission();
                        }
                        break;

                    // DISK EMULATION
                    // SELDISK - select the emulated disk number (binary). 100 disks are supported [0..99]:
                    //
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    DISK number (binary) [0..99]
                    //
                    //
                    // Opens the "disk file" corresponding to the selected disk number, doing some checks.
                    // A "disk file" is a binary file that emulates a disk using a LBA-like logical sector number.
                    // Every "disk file" must have a dimension of 8388608 bytes, corresponding to 16384 LBA-like logical sectors
                    //  (each sector is 512 bytes long), corresponding to 512 tracks of 32 sectors each (see SELTRACK and 
                    //  SELSECT opcodes).
                    // Errors are stored into "errDisk" (see ERRDISK opcode).
                    //
                    //
                    // ...........................................................................................
                    //
                    // "Disk file" filename convention:
                    //
                    // Every "disk file" must follow the syntax "DSsNnn.DSK" where
                    //
                    //    "s" is the "disk set" and must be in the [0..9] range (always one numeric ASCII character)
                    //    "nn" is the "disk number" and must be in the [00..99] range (always two numeric ASCII characters)
                    //
                    // ...........................................................................................
                    //          
                    //
                    // NOTE 1: The maximum disks number may be lower due the limitations of the used OS (e.g. CP/M 2.2 supports
                    //         a maximum of 16 disks)
                    // NOTE 2: Because SELDISK opens the "disk file" used for disk emulation, before using WRITESECT or READSECT
                    //         a SELDISK must be performed at first.
                    case  0x09:
                        if (ioData <= maxDiskNum)               // Valid disk number
                        {
                            // Set the name of the file to open as virtual disk, and open it
                            diskName[2] = diskSet + 48;           // Set the current Disk Set
                            diskName[4] = (ioData / 10) + 48;     // Set the disk number
                            diskName[5] = ioData - ((ioData / 10) * 10) + 48;
                            diskErr = openSD(diskName);           // Open the "disk file" corresponding to the given disk number
                        }
                        else 
                        {
                            diskErr = 16;                      // Illegal disk number
                        }
                        break;

                    // DISK EMULATION
                    // SELTRACK - select the emulated track number (word splitted in 2 bytes in sequence: DATA 0 and DATA 1):
                    //
                    //                I/O DATA 0:  D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    Track number (binary) LSB [0..255]
                    //
                    //                I/O DATA 1:  D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    Track number (binary) MSB [0..1]
                    //
                    //
                    // Stores the selected track number into "trackSel" for "disk file" access.
                    // A "disk file" is a binary file that emulates a disk using a LBA-like logical sector number.
                    // The SELTRACK and SELSECT operations convert the legacy track/sector address into a LBA-like logical 
                    //  sector number used to set the logical sector address inside the "disk file".
                    // A control is performed on both current sector and track number for valid values. 
                    // Errors are stored into "diskErr" (see ERRDISK opcode).
                    //
                    //
                    // NOTE 1: Allowed track numbers are in the range [0..511] (512 tracks)
                    // NOTE 2: Before a WRITESECT or READSECT operation at least a SELSECT or a SELTRAK operation
                    //         must be performed
                    case  0x0A:
                        if (!ioByteCnt)
                        {
                            // LSB
                            trackSel = ioData;
                        }
                        else
                        {
                            // MSB
                            trackSel = (((word) ioData) << 8) | lowByte(trackSel);
                            if ((trackSel < 512) && (sectSel < 32))
                            {
                                // Sector and track numbers valid
                                diskErr = 0;                      // No errors
                            }
                            else
                            {
                                // Sector or track invalid number
                                if (sectSel < 32) 
                                {
                                    diskErr = 17;     // Illegal track number
                                }
                                else 
                                {
                                    diskErr = 18;                  // Illegal sector number
                                }
                            }
                            ioOpcode = 0xFF;                      // All done. Set ioOpcode = "No operation"
                        }
                        ioByteCnt++;
                        break;

                    // DISK EMULATION
                    // SELSECT - select the emulated sector number (binary):
                    //
                    //                  I/O DATA:  D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    Sector number (binary) [0..31]
                    //
                    //
                    // Stores the selected sector number into "sectSel" for "disk file" access.
                    // A "disk file" is a binary file that emulates a disk using a LBA-like logical sector number.
                    // The SELTRACK and SELSECT operations convert the legacy track/sector address into a LBA-like logical 
                    //  sector number used to set the logical sector address inside the "disk file".
                    // A control is performed on both current sector and track number for valid values. 
                    // Errors are stored into "diskErr" (see ERRDISK opcode).
                    //
                    //
                    // NOTE 1: Allowed sector numbers are in the range [0..31] (32 sectors)
                    // NOTE 2: Before a WRITESECT or READSECT operation at least a SELSECT or a SELTRAK operation
                    //         must be performed
                    case  0x0B:
                        sectSel = ioData;
                        if ((trackSel < 512) && (sectSel < 32))
                        {
                            // Sector and track numbers valid
                            diskErr = 0;                        // No errors
                        }
                        else
                        {
                            // Sector or track invalid number
                            if (sectSel < 32) 
                            {
                                diskErr = 17;     // Illegal track number
                            }
                            else 
                            {
                                diskErr = 18;                  // Illegal sector number
                            }
                        }
                        break;

                    // DISK EMULATION
                    // WRITESECT - write 512 data bytes sequentially into the current emulated disk/track/sector:
                    //
                    //                 I/O DATA 0: D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    First Data byte
                    //
                    //                      |               |
                    //                      |               |
                    //                      |               |                 <510 Data Bytes>
                    //                      |               |
                    //
                    //               I/O DATA 511: D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    512th Data byte (Last byte)
                    //
                    //
                    // Writes the current sector (512 bytes) of the current track/sector, one data byte each call. 
                    // All the 512 calls must be always performed sequentially to have a WRITESECT operation correctly done. 
                    // If an error occurs during the WRITESECT operation, all subsequent write data will be ignored and
                    //  the write finalization will not be done.
                    // If an error occurs calling any DISK EMULATION opcode (SDMOUNT excluded) immediately before the WRITESECT 
                    //  opcode call, all the write data will be ignored and the WRITESECT operation will not be performed.
                    // Errors are stored into "diskErr" (see ERRDISK opcode).
                    //
                    // NOTE 1: Before a WRITESECT operation at least a SELTRACK or a SELSECT must be always performed
                    // NOTE 2: Remember to open the right "disk file" at first using the SELDISK opcode
                    // NOTE 3: The write finalization on SD "disk file" is executed only on the 512th data byte exchange, so be 
                    //         sure that exactly 512 data bytes are exchanged.
                    case  0x0C:
                        if (!ioByteCnt)
                        {
                            // First byte of 512, so set the right file pointer to the current emulated track/sector first
                            if ((trackSel < 512) && (sectSel < 32) && (!diskErr))
                            {
                                // Sector and track numbers valid and no previous error; set the LBA-like logical sector
                                diskErr = seekSD((trackSel << 5) | sectSel);  // Set the starting point inside the "disk file"
                                                                  //  generating a 14 bit "disk file" LBA-like 
                                                                  //  logical sector address created as TTTTTTTTTSSSSS
                            }
                        }

                        if (!diskErr)
                        {
                            // No previous error (e.g. selecting disk, track or sector)
                            tempByte = ioByteCnt % 32;            // [0..31]
                            bufferSD[tempByte] = ioData;          // Store current exchanged data byte in the buffer array
                            if (tempByte == 31)
                            {
                                // Buffer full. Write all the buffer content (32 bytes) into the "disk file"
                                diskErr = writeSD(bufferSD, &numWriBytes);
                                if (numWriBytes < 32) 
                                {
                                    diskErr = 19; // Reached an unexpected EOF
                                }

                                if (ioByteCnt >= 511)
                                {
                                    // Finalize write operation and check result (if no previous error occurred)
                                    if (!diskErr) 
                                    {
                                        diskErr = writeSD(NULL, &numWriBytes);
                                    }
                                    ioOpcode = 0xFF;                  // All done. Set ioOpcode = "No operation"
                                }
                            }
                        }
                        ioByteCnt++;                            // Increment the counter of the exchanged data bytes
                        break;

                    // BANKED RAM
                    // SETBANK - select the Os RAM Bank (binary):
                    //
                    //                  I/O DATA:  D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    Os Bank number (binary) [0..2]
                    //
                    //
                    // Set a 32kB RAM bank for the lower half of the Z80 address space (from 0x0000 to 0x7FFF).
                    // The upper half (from 0x8000 to 0xFFFF) is the common fixed bank.
                    // Allowed Os Bank numbers are from 0 to 2.
                    //
                    // Please note that there are three kinds of Bank numbers (see the A040618 schematic):
                    //
                    // * the "Os Bank" number is the bank number managed (known) by the Os;
                    // * the "Logical Bank" number is the bank seen by the Atmega32a (through BANK1 and BANK0 address lines);
                    // * the "Physical Bank" number is the real bank addressed inside the RAM chip (RAM_A16 and RAM_A15 RAM 
                    //   address lines).
                    //
                    // The following tables shows the relations:
                    //
                    //
                    //  Os Bank | Logical Bank |  Z80 Address Bus    |   Physical Bank   |            Notes
                    //  number  | BANK1 BANK0  |        A15          |  RAM_A16 RAM_A15  |
                    // ------------------------------------------------------------------------------------------------
                    //     X    |   X     X    |         1           |     0       1     |  Phy Bank 1 (common fixed)
                    //     -    |   0     0    |         0           |     0       1     |  Phy Bank 1 (common fixed)
                    //     0    |   0     1    |         0           |     0       0     |  Phy Bank 0 (Logical Bank 1)
                    //     2    |   1     0    |         0           |     1       1     |  Phy Bank 3 (Logical Bank 2)
                    //     1    |   1     1    |         0           |     1       0     |  Phy Bank 2 (Logical Bank 3)
                    //
                    //
                    //
                    //      Physical Bank      |    Logical Bank     |   Physical Bank   |   Physical RAM Addresses
                    //          number         |       number        |  RAM_A16 RAM_A15  |
                    // ------------------------------------------------------------------------------------------------
                    //            0            |         1           |     0       0     |   From 0x00000 to 0x07FFF 
                    //            1            |         0           |     0       1     |   From 0x08000 to 0x0FFFF
                    //            2            |         3           |     1       0     |   From 0x01000 to 0x17FFF
                    //            3            |         2           |     1       1     |   From 0x18000 to 0x1FFFF
                    //
                    //
                    // Note that the Logical Bank 0 can't be used as switchable Os Bank bacause it is the common 
                    //  fixed bank mapped in the upper half of the Z80 address space (from 0x8000 to 0xFFFF).
                    //
                    //
                    // NOTE: If the Os Bank number is greater than 2 no selection is done.
                    case  0x0D:
                        switch (ioData)
                        {
                            // Set physical bank 0 (logical bank 1)
                            case 0:                               // Os bank 0
                                digitalWrite(BANK0, HIGH);
                                digitalWrite(BANK1, LOW);
                                break;

                            // Set physical bank 2 (logical bank 3)
                            case 1:                               // Os bank 1
                                digitalWrite(BANK0, HIGH);
                                digitalWrite(BANK1, HIGH);
                                break;  

                            // Set physical bank 3 (logical bank 2)
                            case 2:                               // Os bank 2
                                digitalWrite(BANK0, LOW);
                                digitalWrite(BANK1, HIGH);
                                break;  
                        }
                        break;

                } // switch
                
                if ((ioOpcode != 0x0A) && (ioOpcode != 0x0C)) 
                {
                    ioOpcode = 0xFF;    // All done for the single byte opcodes. 
                                        //  Set ioOpcode = "No operation"
                }
            }

            // Control bus sequence to exit from a wait state (M I/O write cycle)
            digitalWrite(BUSREQ_, LOW);                 // Request for a DMA
            digitalWrite(WAIT_RES_, LOW);               // Reset WAIT FF exiting from WAIT state
            digitalWrite(WAIT_RES_, HIGH);              // Now Z80 is in DMA, so it's safe set WAIT_RES_ HIGH again
            digitalWrite(BUSREQ_, HIGH);                // Resume Z80 from DMA
        }
        else if (!digitalRead(RD_))
        {
            // I/O READ operation requested

// ----------------------------------------
// VIRTUAL I/O READ OPERATIONS ENGINE
// ----------------------------------------
            ioAddress = digitalRead(AD0);             // Read Z80 address bus line AD0 (PC2)
            ioData = 0;                               // Clear input data buffer
            if (ioAddress)                            // Check the I/O address (only AD0 is checked!)
            {
                // .........................................................................................................
                //
                // AD0 = 1 (I/O read address = 0x01). SERIAL RX.
                //
                // Execute a Serial I/O Read operation.
                // .........................................................................................................
                //
                //
                // SERIAL RX:     
                //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                //                            ---------------------------------------------------------
                //                             D7 D6 D5 D4 D3 D2 D1 D0    ASCII char read from serial
                //
                // NOTE 1: If there is no input char, a value 0xFF is forced as input char.
                // NOTE 2: The INT_ signal is always reset (set to HIGH) after this I/O operation.
                // NOTE 3: This is the only I/O that do not require any previous STORE OPCODE operation (for fast polling).
                // NOTE 4: A "RX buffer empty" flag and a "Last Rx char was empty" flag are available in the SYSFLAG opcode 
                //         to allow 8 bit I/O.
                //
                ioData = 0xFF;
                if (Serial.available() > 0)
                {
                    ioData = Serial.read();
                    LastRxIsEmpty = 0;                // Reset the "Last Rx char was empty" flag
                }
                else 
                {
                    LastRxIsEmpty = 1;             // Set the "Last Rx char was empty" flag
                }
                digitalWrite(INT_, HIGH);
            }
            else
            {
                // .........................................................................................................
                //
                // AD0 = 0 (I/O read address = 0x00). EXECUTE READ OPCODE.
                //
                // Execute the previously stored I/O read operation with the current data.
                // The code of the I/O operation (Opcode) must be previously stored with a STORE OPCODE operation.
                //
                // NOTE: For multi-byte read opcode (as DATETIME) read sequentially all the data bytes without to send
                //       a STORE OPCODE operation before each data byte after the first one.
                // .........................................................................................................
                //
                // Execute the requested I/O READ Opcode. The 0xFF value is reserved as "No operation".
                switch (ioOpcode)
                {
                    // USER KEY:      
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                              0  0  0  0  0  0  0  0    USER Key not pressed
                    //                              0  0  0  0  0  0  0  1    USER Key pressed
                    case  0x80:
                        tempByte = digitalRead(USER);         // Save USER led status
                        pinMode(USER, INPUT_PULLUP);          // Read USER Key
                        ioData = !digitalRead(USER);
                        pinMode(USER, OUTPUT); 
                        digitalWrite(USER, tempByte);         // Restore USER led status
                        break;

                    // GPIOA Read (GPE Option):
                    //
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    GPIOA value (see MCP23017 datasheet)
                    //
                    // NOTE: a value 0x00 is forced if the GPE Option is not present
                    case  0x81:
                        if (moduleGPIO) 
                        {
                            // Set MCP23017 pointer to GPIOA
                            Wire.beginTransmission(GPIOEXP_ADDR);
                            Wire.write(GPIOA_REG);
                            Wire.endTransmission();
                            // Read GPIOA
                            Wire.beginTransmission(GPIOEXP_ADDR);
                            Wire.requestFrom(GPIOEXP_ADDR, 1);
                            ioData = Wire.read();
                        }
                        break;

                    // GPIOB Read (GPE Option):
                    //
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    GPIOB value (see MCP23017 datasheet)
                    //
                    // NOTE: a value 0x00 is forced if the GPE Option is not present
                    case  0x82:
                        if (moduleGPIO) 
                        {
                            // Set MCP23017 pointer to GPIOB
                            Wire.beginTransmission(GPIOEXP_ADDR);
                            Wire.write(GPIOB_REG);
                            Wire.endTransmission();
                            // Read GPIOB
                            Wire.beginTransmission(GPIOEXP_ADDR);
                            Wire.requestFrom(GPIOEXP_ADDR, 1);
                            ioData = Wire.read();
                        }
                        break;

                    // SYSFLAGS (Various system flags for the OS):
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                              X  X  X  X  X  X  X  0    AUTOEXEC not enabled
                    //                              X  X  X  X  X  X  X  1    AUTOEXEC enabled
                    //                              X  X  X  X  X  X  0  X    DS3231 RTC not found
                    //                              X  X  X  X  X  X  1  X    DS3231 RTC found
                    //                              X  X  X  X  X  0  X  X    Serial RX buffer empty
                    //                              X  X  X  X  X  1  X  X    Serial RX char available
                    //                              X  X  X  X  0  X  X  X    Previous RX char valid
                    //                              X  X  X  X  1  X  X  X    Previous RX char was a "buffer empty" flag
                    //
                    // NOTE: Currently only D0-D3 are used
                    case  0x83:
                        ioData = autoexecFlag | (foundRTC << 1) | ((Serial.available() > 0) << 2) | ((LastRxIsEmpty > 0) << 3);
                        break;

                    // DATETIME (Read date/time and temperature from the RTC. Binary values): 
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                I/O DATA 0   D7 D6 D5 D4 D3 D2 D1 D0    seconds [0..59]     (1st data byte)
                    //                I/O DATA 1   D7 D6 D5 D4 D3 D2 D1 D0    minutes [0..59]
                    //                I/O DATA 2   D7 D6 D5 D4 D3 D2 D1 D0    hours   [0..23]
                    //                I/O DATA 3   D7 D6 D5 D4 D3 D2 D1 D0    day     [1..31]
                    //                I/O DATA 4   D7 D6 D5 D4 D3 D2 D1 D0    month   [1..12]
                    //                I/O DATA 5   D7 D6 D5 D4 D3 D2 D1 D0    year    [0..99]
                    //                I/O DATA 6   D7 D6 D5 D4 D3 D2 D1 D0    tempC   [-128..127] (7th data byte)
                    //
                    // NOTE 1: If RTC is not found all read values wil be = 0
                    // NOTE 2: Overread data (more then 7 bytes read) will be = 0
                    // NOTE 3: The temperature (Celsius) is a byte with two complement binary format [-128..127]
                    case  0x84:
                        if (foundRTC)
                        {
                            if (ioByteCnt == 0) 
                            {
                                readRTC(&seconds, &minutes, &hours, &day, &month, &year, &tempC); // Read from RTC
                            }
                            
                            if (ioByteCnt < 7)
                            {
                                // Send date/time (binary values) to Z80 bus
                                switch (ioByteCnt)
                                {
                                    case 0: ioData = seconds; break;
                                    case 1: ioData = minutes; break;
                                    case 2: ioData = hours; break;
                                    case 3: ioData = day; break;
                                    case 4: ioData = month; break;
                                    case 5: ioData = year; break;
                                    case 6: ioData = tempC; break;
                                }
                                ioByteCnt++;
                            }
                            else 
                            {
                                ioOpcode = 0xFF;              // All done. Set ioOpcode = "No operation"
                            }
                        }
                        else 
                        {
                            ioOpcode = 0xFF;                 // Nothing to do. Set ioOpcode = "No operation"
                        }
                        break;

                    // DISK EMULATION
                    // ERRDISK - read the error code after a SELDISK, SELSECT, SELTRACK, WRITESECT, READSECT 
                    //           or SDMOUNT operation
                    //
                    //                I/O DATA:    D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    DISK error code (binary)
                    //
                    //
                    // Error codes table:
                    //
                    //    error code    | description
                    // ---------------------------------------------------------------------------------------------------
                    //        0         |  No error
                    //        1         |  DISK_ERR: the function failed due to a hard error in the disk function, 
                    //                  |   a wrong FAT structure or an internal error
                    //        2         |  NOT_READY: the storage device could not be initialized due to a hard error or 
                    //                  |   no medium
                    //        3         |  NO_FILE: could not find the file
                    //        4         |  NOT_OPENED: the file has not been opened
                    //        5         |  NOT_ENABLED: the volume has not been mounted
                    //        6         |  NO_FILESYSTEM: there is no valid FAT partition on the drive
                    //       16         |  Illegal disk number
                    //       17         |  Illegal track number
                    //       18         |  Illegal sector number
                    //       19         |  Reached an unexpected EOF
                    //
                    //
                    //
                    //
                    // NOTE 1: ERRDISK code is referred to the previous SELDISK, SELSECT, SELTRACK, WRITESECT or READSECT
                    //         operation
                    // NOTE 2: Error codes from 0 to 6 come from the PetitFS library implementation
                    // NOTE 3: ERRDISK must not be used to read the resulting error code after a SDMOUNT operation 
                    //         (see the SDMOUNT opcode)
                    case  0x85:
                        ioData = diskErr;
                        break;

                    // DISK EMULATION
                    // READSECT - read 512 data bytes sequentially from the current emulated disk/track/sector:
                    //
                    //                 I/O DATA:   D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                 I/O DATA 0  D7 D6 D5 D4 D3 D2 D1 D0    First Data byte
                    //
                    //                      |               |
                    //                      |               |
                    //                      |               |                 <510 Data Bytes>
                    //                      |               |
                    //
                    //               I/O DATA 127  D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    512th Data byte (Last byte)
                    //
                    //
                    // Reads the current sector (512 bytes) of the current track/sector, one data byte each call. 
                    // All the 512 calls must be always performed sequentially to have a READSECT operation correctly done. 
                    // If an error occurs during the READSECT operation, all subsequent read data will be = 0.
                    // If an error occurs calling any DISK EMULATION opcode (SDMOUNT excluded) immediately before the READSECT 
                    //  opcode call, all the read data will be will be = 0 and the READSECT operation will not be performed.
                    // Errors are stored into "diskErr" (see ERRDISK opcode).
                    //
                    // NOTE 1: Before a READSECT operation at least a SELTRACK or a SELSECT must be always performed
                    // NOTE 2: Remember to open the right "disk file" at first using the SELDISK opcode
                    case  0x86:
                        if (!ioByteCnt)
                        {
                            // First byte of 512, so set the right file pointer to the current emulated track/sector first
                            if ((trackSel < 512) && (sectSel < 32) && (!diskErr))
                            {
                                // Sector and track numbers valid and no previous error; set the LBA-like logical sector
                                diskErr = seekSD((trackSel << 5) | sectSel);  // Set the starting point inside the "disk file"
                                                                    //  generating a 14 bit "disk file" LBA-like 
                                                                    //  logical sector address created as TTTTTTTTTSSSSS
                            }
                        }

                        if (!diskErr)
                        {
                            // No previous error (e.g. selecting disk, track or sector)
                            tempByte = ioByteCnt % 32;          // [0..31]
                            if (!tempByte)
                            {
                                // Read 32 bytes of the current sector on SD in the buffer (every 32 calls, starting with the first)
                                diskErr = readSD(bufferSD, &numReadBytes); 
                                if (numReadBytes < 32) 
                                {
                                    diskErr = 19;    // Reached an unexpected EOF
                                }
                            }
                            if (!diskErr) 
                            {
                                ioData = bufferSD[tempByte];// If no errors, exchange current data byte with the CPU
                            }
                        }
                        if (ioByteCnt >= 511) 
                        {
                            ioOpcode = 0xFF;                    // All done. Set ioOpcode = "No operation"
                        }
                        ioByteCnt++;                          // Increment the counter of the exchanged data bytes
                        break;

                    // DISK EMULATION
                    // SDMOUNT - mount a volume on SD, returning an error code (binary):
                    //
                    //                 I/O DATA 0: D7 D6 D5 D4 D3 D2 D1 D0
                    //                            ---------------------------------------------------------
                    //                             D7 D6 D5 D4 D3 D2 D1 D0    error code (binary)
                    //
                    //
                    //
                    // NOTE 1: This opcode is "normally" not used. Only needed if using a virtual disk from a custom program
                    //         loaded with iLoad or with the Auto-boot mode (e.g. ViDiT). Can be used to handle SD hot-swapping
                    // NOTE 2: For error codes explanation see ERRDISK opcode
                    // NOTE 3: Only for this disk opcode, the resulting error is read as a data byte without using the 
                    //         ERRDISK opcode
                    case  0x87:
                        ioData = mountSD(&filesysSD);
                        break;          
                } // switch
                
                if ((ioOpcode != 0x84) && (ioOpcode != 0x86)) 
                {
                    ioOpcode = 0xFF;  // All done for the single byte opcodes. 
                                  //  Set ioOpcode = "No operation"
                }
            }
            
            DDRA = 0xFF;                              // Configure Z80 data bus D0-D7 (PA0-PA7) as output
            PORTA = ioData;                           // Current output on data bus

            // Control bus sequence to exit from a wait state (M I/O read cycle)
            digitalWrite(BUSREQ_, LOW);               // Request for a DMA
            digitalWrite(WAIT_RES_, LOW);             // Now is safe reset WAIT FF (exiting from WAIT state)
            delayMicroseconds(2);                     // Wait 2us just to be sure that Z80 read the data and go HiZ
            DDRA = 0x00;                              // Configure Z80 data bus D0-D7 (PA0-PA7) as input with pull-up
            PORTA = 0xFF;
            digitalWrite(WAIT_RES_, HIGH);            // Now Z80 is in DMA (HiZ), so it's safe set WAIT_RES_ HIGH again
            digitalWrite(BUSREQ_, HIGH);              // Resume Z80 from DMA
        }
        else
        {
            // INTERRUPT operation setting IORQ_ LOW, so nothing to do
// ----------------------------------------
// VIRTUAL INTERRUPT
// ----------------------------------------
            // Nothing to do
            //
            // DEBUG ----------------------------------
            if (debug > 2) 
            {
                Serial.println();
                Serial.println("DEBUG: INT operation (nothing to do)");
            }
            // DEBUG END ------------------------------
            //

            // Control bus sequence to exit from a wait state (M interrupt cycle)
            digitalWrite(BUSREQ_, LOW);               // Request for a DMA
            digitalWrite(WAIT_RES_, LOW);             // Reset WAIT FF exiting from WAIT state
            digitalWrite(WAIT_RES_, HIGH);            // Now Z80 is in DMA, so it's safe set WAIT_RES_ HIGH again
            digitalWrite(BUSREQ_, HIGH);              // Resume Z80 from DMA
        }
    }
} // end of loop

// end of source file
