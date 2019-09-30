/*
 * Monitor.h
 *
 * Created: 26/09/2019
 *  Author: Mark Riley
 */ 


#ifndef MONITOR_H_
#define MONITOR_H_

#ifdef __cplusplus
extern "C" {
    #endif

// ------------------------------------------------------------------------------
//
// Hardware definitions for A040618 (Z80-MBC2) - Base system
//
// ------------------------------------------------------------------------------
//        MBC2 Name  Arduino      Location    Purpose
#define   D0            24    // PA0 pin 40   Z80 data bus
#define   D1            25    // PA1 pin 39
#define   D2            26    // PA2 pin 38
#define   D3            27    // PA3 pin 37
#define   D4            28    // PA4 pin 36
#define   D5            29    // PA5 pin 35
#define   D6            30    // PA6 pin 34
#define   D7            31    // PA7 pin 33

#define   LED_IOS        0    // PB0 pin 1    Led LED_IOS is ON if HIGH
#define   WAIT_RES_      0    // PB0 pin 1    Reset the Wait FF
#define   INT_           1    // PB1 pin 2    Z80 control bus
#define   RAM_CE2        2    // PB2 pin 3    RAM Chip Enable (CE2). Active HIGH. Used only during boot
#define   WAIT_          3    // PB3 pin 4    Z80 WAIT
#define   SS_            4    // PB4 pin 5    SD SPI
#define   MOSI           5    // PB5 pin 6    SD SPI
#define   MISO           6    // PB6 pin 7    SD SPI
#define   SCK            7    // PB7 pin 8    SD SPI
#define   AD0           18    // PC2 pin 24   Z80 A0
#define   WR_           19    // PC3 pin 25   Z80 WR
#define   RD_           20    // PC4 pin 26   Z80 RD
#define   MREQ_         21    // PC5 pin 27   Z80 MREQ
#define   RESET_        22    // PC6 pin 28   Z80 RESET
#define   MCU_RTS_      23    // PC7 pin 29   * RESERVED - NOT USED *
#define   MCU_CTS_      10    // PD2 pin 16   * RESERVED - NOT USED *
#define   BANK1         11    // PD3 pin 17   RAM Memory bank address (High)
#define   BANK0         12    // PD4 pin 18   RAM Memory bank address (Low)
#define   BUSREQ_       14    // PD6 pin 20   Z80 BUSRQ
#define   CLK           15    // PD7 pin 21   Z80 CLK
#define   SCL_PC0       16    // PC0 pin 22   IOEXP connector (I2C)
#define   SDA_PC1       17    // PC1 pin 23   IOEXP connector (I2C)
#define   USER          13    // PD5 pin 19   Led USER and key (led USER is ON if LOW)


// used to extract data to help decode instructions
#define X_MASK  0xC0
#define Y_MASK  0x38
#define Z_MASK  0x07
#define P_MASK  0x30
#define Q_MASK  0x08

// register encoding
#define A B00000111
#define B B00000000
#define C B00000001
#define D B00000010
#define E B00000011
#define H B00000100
#define L B00000101


// Z80 intrinsics
const byte    LD_HL        =  0x36;       // Opcode of the Z80 instruction: LD(HL), n
const byte    INC_HL       =  0x23;       // Opcode of the Z80 instruction: INC HL
const byte    LD_HLnn      =  0x21;       // Opcode of the Z80 instruction: LD HL, nn
const byte    JP_nn        =  0xC3;       // Opcode of the Z80 instruction: JP nn
const byte    LD_A_HL      =  0x7E;       // Opcode of the Z80 instruction: LD A,(HL)
const byte    LD_HL_A      =  0x77;       // Opcode of the Z80 instruction: LD (HL),A


// ------------------------------------------------------------------------------
// Externs
// ------------------------------------------------------------------------------
extern byte Z80IntEnFlag;

// ------------------------------------------------------------------------------
// Function Prototypes
// ------------------------------------------------------------------------------
void    singlePulsesResetZ80();
void    loadHL(word value);
void    pulseClock(byte numPulse);
byte    readByteFromRAM( word address );
word    read16bitFromRAM( word address );
void    writeByteToRAM(byte value);
void    write16bitToRAM( word address, word value );
void    setXYZPQ( byte item );
byte    decodeUnprefixed( String *rtnString, uint16_t &address );
byte    decodeCB( String *rtnString, uint16_t &address );
byte    decodeED( String *rtnString, uint16_t &address );
byte    decodeDDFD( String *rtnString, uint16_t address, char *IndexRegister );
String  *disassemble( uint16_t &address );
byte    assemble( String *instruction, uint16_t address );
void    monitor();

#ifdef __cplusplus
}
#endif

#endif /* MONITOR_H_ */