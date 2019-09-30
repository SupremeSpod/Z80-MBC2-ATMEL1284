/*
 * Generic.h
 *
 * Created: 26/09/2019
 *  Author: Mark Riley
 */ 


#ifndef GENERIC_H_
#define GENERIC_H_

#ifdef __cplusplus
extern "C" {
    #endif


// ------------------------------------------------------------------------------
// Externals
// ------------------------------------------------------------------------------
extern char inChar;  // Input char from serial

// ------------------------------------------------------------------------------
// Function Prototypes
// ------------------------------------------------------------------------------
void    printBinaryByte(byte value);
void    serialEvent();
void    blinkIOSled(unsigned long *timestamp);
void    printDateTime(byte readSourceFlag);
void    print2digit(byte data);
byte    isLeapYear(byte yearXX);
void    waitKey(const String *prompt );
void    printOsName(byte currentDiskSet);
byte    decToBcd(byte val);
byte    bcdToDec(byte val);

#ifdef __cplusplus
}
#endif


#endif /* GENERIC_H_ */