/*
 * SDCard.cpp
 *
 * Created: 26/09/2019 09:50:13
 *  Author: mriley1
 */ 

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

 // SD disk and CP/M support variables
 FATFS         filesysSD;                  // Filesystem object (PetitFS library)
 byte          bufferSD[32];               // I/O buffer for SD disk operations (store a "segment" of a SD sector).
 //  Each SD sector (512 bytes) is divided into 16 segments (32 bytes each)
 const char *  fileNameSD;                 // Pointer to the string with the currently used file name
 byte          autobootFlag;               // Set to 1 if "autoboot.bin" must be executed at boot, 0 otherwise
 byte          autoexecFlag;               // Set to 1 if AUTOEXEC must be executed at CP/M cold boot, 0 otherwise
 byte          errCodeSD;                  // Temporary variable to store error codes from the PetitFS
 byte          numReadBytes;               // Number of read bytes after a readSD() call

 // Disk emulation on SD
 char          diskName[11]    = Z80DISK;  // String used for virtual disk file name
 char          OsName[11]      = DS_OSNAME;// String used for file holding the OS name
 word          trackSel;                   // Store the current track number [0..511]
 byte          sectSel;                    // Store the current sector number [0..31]
 byte          diskErr         = 19;       // SELDISK, SELSECT, SELTRACK, WRITESECT, READSECT or SDMOUNT resulting
 //  error code
 byte          numWriBytes;                // Number of written bytes after a writeSD() call
 byte          diskSet;                    // Current "Disk Set"



 // ------------------------------------------------------------------------------
 // SD Disk routines (FAT16 and FAT32 filesystem supported) using the PetitFS library.
 // For more info about PetitFS see here: http://elm-chan.org/fsw/ff/00index_p.html
 // ------------------------------------------------------------------------------


 // ------------------------------------------------------------------------------
 // Mount a volume on SD:
 // *  "fatFs" is a pointer to a FATFS object (PetitFS library)
 // The returned value is the resulting status (0 = ok, otherwise see printErrSD())
 // ------------------------------------------------------------------------------
 byte mountSD(FATFS* fatFs)
 {
     return pf_mount(fatFs);
 }

 // ------------------------------------------------------------------------------
 // Open an existing file on SD:
 // *  "fileName" is the pointer to the string holding the file name (8.3 format)
 // The returned value is the resulting status (0 = ok, otherwise see printErrSD())
 // ------------------------------------------------------------------------------
 byte openSD(const char* fileName)
 {
     return pf_open(fileName);
 }

 // ------------------------------------------------------------------------------
 // Read one "segment" (32 bytes) starting from the current sector (512 bytes) of
 // the opened file on SD:
 // *  "BuffSD" is the pointer to the segment buffer;
 // *  "numReadBytes" is the pointer to the variables that store the number of
 //    read bytes;
 //     if < 32 (including = 0) an EOF was reached).
 // The returned value is the resulting status (0 = ok, otherwise see printErrSD())
 //
 // NOTE1: Each SD sector (512 bytes) is divided into 16 segments (32 bytes each);
 //        to read a sector you need to
 //        to call readSD() 16 times consecutively
 //
 // NOTE2: Past current sector boundary, the next sector will be pointed. So to
 //        read a whole file it is sufficient
 //        call readSD() consecutively until EOF is reached
 // ------------------------------------------------------------------------------
 byte readSD(void* buffSD, byte* numReadBytes)
 {
     UINT  numBytes;
     byte  errcode;
     errcode = pf_read(buffSD, 32, &numBytes);
     *numReadBytes = (byte) numBytes;
     return errcode;
 }

 // ------------------------------------------------------------------------------
 // Write one "segment" (32 bytes) starting from the current sector (512 bytes) of the opened file on SD:
 // *  "BuffSD" is the pointer to the segment buffer;
 // *  "numWrittenBytes" is the pointer to the variables that store the number of written bytes;
 //     if < 32 (including = 0) an EOF was reached.
 // The returned value is the resulting status (0 = ok, otherwise see printErrSD())
 //
 // NOTE1: Each SD sector (512 bytes) is divided into 16 segments (32 bytes each); to write a sector you need to
 //        to call writeSD() 16 times consecutively
 //
 // NOTE2: Past current sector boundary, the next sector will be pointed. So to write a whole file it is sufficient
 //        call writeSD() consecutively until EOF is reached
 //
 // NOTE3: To finalize the current write operation a writeSD(NULL, &numWrittenBytes) must be called as last action
 // ------------------------------------------------------------------------------
 byte writeSD(void* buffSD, byte* numWrittenBytes)
 {
     UINT  numBytes;
     byte  errcode;
     if (buffSD != NULL)
     {
         errcode = pf_write(buffSD, 32, &numBytes);
     }
     else
     {
         errcode = pf_write(0, 0, &numBytes);
     }
     *numWrittenBytes = (byte) numBytes;
     return errcode;
 }

 // ------------------------------------------------------------------------------
 // Set the pointer of the current sector for the current opened file on SD:
 // *  "sectNum" is the sector number to set. First sector is 0.
 // The returned value is the resulting status (0 = ok, otherwise see printErrSD())
 //
 // NOTE: "secNum" is in the range [0..16383], and the sector addressing is
 //       continuous inside a "disk file";
 //       16383 = (512 * 32) - 1, where 512 is the number of emulated tracks, 32
 //       is the number of emulated sectors
 //
 // ------------------------------------------------------------------------------
 byte seekSD(word sectNum)
 {
     return pf_lseek(((unsigned long) sectNum) << 9);
 }


 // ------------------------------------------------------------------------------
 // ------------------------------------------------------------------------------
 void printErrSD(byte opType, byte errCode, const char* fileName)
 {
     if (errCode)
     {
         Serial.print("\r\nIOS: SD error ");
         Serial.print(errCode);
         Serial.print(" (");
         
         // See PetitFS implementation for the codes
         switch (errCode)
         {
             case 1: Serial.print("DISK_ERR"); break;
             case 2: Serial.print("NOT_READY"); break;
             case 3: Serial.print("NO_FILE"); break;
             case 4: Serial.print("NOT_OPENED"); break;
             case 5: Serial.print("NOT_ENABLED"); break;
             case 6: Serial.print("NO_FILESYSTEM"); break;
             default: Serial.print("UNKNOWN");
         }
         Serial.print(" on ");
         switch (opType)
         {
             case 0: Serial.print("MOUNT"); break;
             case 1: Serial.print("OPEN"); break;
             case 2: Serial.print("READ"); break;
             case 3: Serial.print("WRITE"); break;
             case 4: Serial.print("SEEK"); break;
             default: Serial.print("UNKNOWN");
         }
         Serial.print(" operation");

         if (fileName)
         {
             // Not a NULL pointer, so print file name too
             Serial.print(" - File: ");
             Serial.print(fileName);
         }
         Serial.println(")");
     }
 }

 // end of source file