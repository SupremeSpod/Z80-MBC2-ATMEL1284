/*
 * SDCard.h
 *
 * Created: 26/09/2019 09:57:42
 *  Author: mriley1
 */ 


#ifndef SDCARD_H_
#define SDCARD_H_

#ifdef __cplusplus
extern "C" {
    #endif

// ------------------------------------------------------------------------------
// Externals
// ------------------------------------------------------------------------------
extern byte numReadBytes;                 // Number of read bytes after a readSD() call
extern FATFS         filesysSD;                  // Filesystem object (PetitFS library)
extern byte          bufferSD[32];               // I/O buffer for SD disk operations (store a "segment" of a SD sector).
//  Each SD sector (512 bytes) is divided into 16 segments (32 bytes each)
extern const char *  fileNameSD;                 // Pointer to the string with the currently used file name
extern byte          autobootFlag;               // Set to 1 if "autoboot.bin" must be executed at boot, 0 otherwise
extern byte          autoexecFlag;               // Set to 1 if AUTOEXEC must be executed at CP/M cold boot, 0 otherwise
extern byte          errCodeSD;                  // Temporary variable to store error codes from the PetitFS
// Disk emulation on SD
extern char          diskName[11];  // String used for virtual disk file name
extern char          OsName[11];// String used for file holding the OS name
extern word          trackSel;                   // Store the current track number [0..511]
extern byte          sectSel;                    // Store the current sector number [0..31]
extern byte          diskErr;       // SELDISK, SELSECT, SELTRACK, WRITESECT, READSECT or SDMOUNT resulting
//  error code
extern byte          numWriBytes;                // Number of written bytes after a writeSD() call
extern byte          diskSet;                    // Current "Disk Set"


// ------------------------------------------------------------------------------
// Function Prototypes
// ------------------------------------------------------------------------------
byte mountSD(FATFS* fatFs);
byte openSD(const char* fileName);
byte readSD(void* buffSD, byte* numReadBytes);
byte writeSD(void* buffSD, byte* numWrittenBytes);
byte seekSD(word sectNum);
void printErrSD(byte opType, byte errCode, const char* fileName);

#ifdef __cplusplus
}
#endif

#endif /* SDCARD_H_ */