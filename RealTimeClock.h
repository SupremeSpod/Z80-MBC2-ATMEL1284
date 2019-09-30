/*
 * RealTimeClock.h
 *
 * Created: 26/09/2019 09:29:30
 *  Author: mriley1
 */ 


#ifndef REALTIMECLOCK_H_
#define REALTIMECLOCK_H_
#ifdef __cplusplus
extern "C" {
    #endif

// ------------------------------------------------------------------------------
// Global Externs
// ------------------------------------------------------------------------------
extern byte             foundRTC;                   // Set to 1 if RTC is found, 0 otherwise
extern byte             seconds, minutes, hours, day, month, year;
extern byte             tempC;                      // Temperature (Celsius) encoded in twos complement integer format
extern unsigned long    timeStamp;                  // Timestamp for led blinking
// extern byte             tempByte;                   // Temporary variable (buffer)

// ------------------------------------------------------------------------------
// Function Prototypes
// ------------------------------------------------------------------------------
void readRTC(byte *second, byte *minute, byte *hour, byte *day, byte *month, byte *year, byte *tempC);
void writeRTC(byte second, byte minute, byte hour, byte day, byte month, byte year);
byte autoSetRTC();
void ChangeRTC();

#ifdef __cplusplus
}
#endif


#endif /* REALTIMECLOCK_H_ */