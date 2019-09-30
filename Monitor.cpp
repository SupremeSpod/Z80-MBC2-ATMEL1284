/*
 * Monitor.cpp
 *
 * Created: 26/09/2019
 *  Author: Mark Riley
 */ 



// ------------------------------------------------------------------------------
//
//  Libraries
//
// ------------------------------------------------------------------------------

#include <avr/pgmspace.h>                 // Needed for PROGMEM
#include "Wire.h"                         // Needed for I2C bus
#include <EEPROM.h>                       // Needed for internal EEPROM R/W
#include "PetitFS.h"                      // Light handler for FAT16 and FAT32 filesystem on SD
#include "integer.h"
#include "Monitor.h"
#include "Generic.h"

// Used by assemble and disassemble
const char *table_r[8]        = { "B",  "C",  "D",  "E", "H", "L", "(HL)", "A"};
const char *table_rp[4]       = { "BC", "DE", "HL", "SP" };
const char *table_rp2[4]      = { table_rp[0], table_rp[1], table_rp[2], "AF" };
const char *table_cc[8]       = { "NZ", "Z", "NC", "C", "PO", "PE", "P", "M"};
const char *table_alu[8]      = { "ADD A", "ADC A", "SUB", "SBC A,", "AND", "XOR", "OR",  "CP"};
const char *table_rot[8]      = { "RLC",   "RRC",   "RL",  "RR", "SLA", "SRA", "SLL", "SRL"};
const char *table_accFlags[8] = { "RLCA",  "RRCA",  "RLA", "RRA", "DAA", "CPL", "SCF", "CCF" };
const char *table_assrt[8]    = { "NOP", "EX AF,AF\'", "DJNZ ", "JR ", table_assrt[3], table_assrt[3], table_assrt[3], table_assrt[3]};
const char *table_assrt2[8]   = { "JP nn", "CB", "OUT (n),A", "IN A,(n)", "EX (SP),HL", "EX DE,HL", "DI", "EI"};
const char *table_ind[2][4]   = { { "LD (BC), A", "LD (DE), A", "LD (nn), HL", "LD (nn), A" },
                                  { "LD A, (BC)", "LD A, (DE)", "LD HL, (nn)", "LD A, (nn)" }};
const char *table_im[8]       = { "0", "0/1", "1", "2", table_im[0], table_im[1], table_im[2], table_im[3]};
const char *table_bli[8][4]   = { {"LDI",  "CPI",  "INI",  "OUTI"},
                                  {"LDD",  "CPD",  "IND",  "OUTD"},
                                  {"LDIR", "CPIR", "INIR", "OTIR"},
                                  {"LDDR", "CPDR", "INDR", "OTDR"} };
const char *table_bitops[8]   = { table_assrt[0], "BIT", "RES", "SET" };
const char *table_EDassrt[8]  = { "LD I,A", "LD R,A", "LD A,I", "LD A,R", "RRD", "RLD", table_assrt[0], table_assrt[0] };
const char *table_incdec[2]   = { "INC ", "DEC " };
const char *table_rbrs[4]     = { "ROT", table_bitops[1], table_bitops[2], table_bitops[3] };
const char *sixteenBitValue   = "nn";


byte Z80IntEnFlag   = 0;         // Z80 INT_ enable flag (0 = No INT_ used, 1 = INT_ used for I/O)

// Z80 Instruction encoding and decoding - drawn from http://www.z80.info/decoding.htm#cb
byte X;     // mask %11000000
byte Y;     // mask %00111000
byte Z;     // mask %00000111
byte P;     // mask %00110000
byte Q;     // mask %00001000



enum INSTRUCTION_TYPE { LOAD = 0, BLOCK, ALU, ROTATENSHIFT, BIT, PROGRAMFLOW, IO, SINGLE, CPU };
enum ADDRESSING_MODE { IMM = 0, IEX,   MPZ, REL, EXT, IND, REG, IMP, RIN, BAD };


typedef struct TOKEN_STRUCT{ char *token;
                             INSTRUCTION_TYPE type;
                             ADDRESSING_MODE  mode;
                             byte cycles; };

typedef struct LABEL_STRUCT { char *name; word address; LABEL_STRUCT *next;};

// this token list 
const char *tokens[] = {};

// ------------------------------------------------------------------------------
// Generate <numPulse> clock pulses on the Z80 clock pin.
// The steady clock level is LOW, e.g. one clock pulse is a 0-1-0 transition
// ------------------------------------------------------------------------------
void pulseClock(byte numPulse)
{
    byte    i;
    for (i = 0; i < numPulse; i++)
    {
        // Send one impulse (1-0) on the CLK output
        digitalWrite(CLK, HIGH);
        digitalWrite(CLK, LOW);
    }
}


// ------------------------------------------------------------------------------
// Load a given byte to RAM using a sequence of two Z80 instructions forced on
// the data bus.
// The RAM_CE2 signal is used to force the RAM in HiZ, so the Atmega can write
// the needed instruction/data on the data bus. Controlling the clock signal and
// knowing exactly how many clocks pulse are required it is possible to control
// the whole loading process.
// In the following "T" are the T-cycles of the Z80 (See the Z80 datasheet).
// The two instruction are "LD (HL), n" and "INC (HL)".
// ------------------------------------------------------------------------------
void writeByteToRAM(byte value)
{
     // Execute the LD(HL),n instruction (T = 4+3+3). See the Z80 datasheet and manual.
     // After the execution of this instruction the <value> byte is loaded in the memory address pointed by HL.
     pulseClock(1);                      // Execute the T1 cycle of M1 (Opcode Fetch machine cycle)
     digitalWrite(RAM_CE2, LOW);         // Force the RAM in HiZ (CE2 = LOW)
     DDRA = 0xFF;                        // Configure Z80 data bus D0-D7 (PA0-PA7) as output
     PORTA = LD_HL;                      // Write "LD (HL), n" opcode on data bus
     pulseClock(2);                      // Execute T2 and T3 cycles of M1
     DDRA = 0x00;                        // Configure Z80 data bus D0-D7 (PA0-PA7) as input...
     PORTA = 0xFF;                       // ...with pull-up
     pulseClock(2);                      // Complete the execution of M1 and execute the T1 cycle of the following
     // Memory Read machine cycle
     DDRA = 0xFF;                        // Configure Z80 data bus D0-D7 (PA0-PA7) as output
     PORTA = value;                      // Write the byte to load in RAM on data bus
     pulseClock(2);                      // Execute the T2 and T3 cycles of the Memory Read machine cycle
     DDRA = 0x00;                        // Configure Z80 data bus D0-D7 (PA0-PA7) as input...
     PORTA = 0xFF;                       // ...with pull-up
     digitalWrite(RAM_CE2, HIGH);        // Enable the RAM again (CE2 = HIGH)
     pulseClock(3);                      // Execute all the following Memory Write machine cycle

     // Execute the INC(HL) instruction (T = 6). See the Z80 datasheet and manual.
     // After the execution of this instruction HL points to the next memory address.
     pulseClock(1);                      // Execute the T1 cycle of M1 (Opcode Fetch machine cycle)
     digitalWrite(RAM_CE2, LOW);         // Force the RAM in HiZ (CE2 = LOW)
     DDRA = 0xFF;                        // Configure Z80 data bus D0-D7 (PA0-PA7) as output
     PORTA = INC_HL;                     // Write "INC(HL)" opcode on data bus
     pulseClock(2);                      // Execute T2 and T3 cycles of M1
     DDRA = 0x00;                        // Configure Z80 data bus D0-D7 (PA0-PA7) as input...
     PORTA = 0xFF;                       // ...with pull-up
     digitalWrite(RAM_CE2, HIGH);        // Enable the RAM again (CE2 = HIGH)
     pulseClock(3);                      // Execute all the remaining T cycles
}


// ------------------------------------------------------------------------------
// Load "value" word into the HL registers inside the Z80 CPU, using the "LD HL,nn" instruction.
// In the following "T" are the T-cycles of the Z80 (See the Z80 datasheet).
// ------------------------------------------------------------------------------
void loadHL(word value)
{
    // Execute the LD dd,nn instruction (T = 4+3+3), with dd = HL and nn = value. See the Z80 datasheet and manual.
    // After the execution of this instruction the word "value" (16bit) is loaded into HL.
    pulseClock(1);                      // Execute the T1 cycle of M1 (Opcode Fetch machine cycle)
    digitalWrite(RAM_CE2, LOW);         // Force the RAM in HiZ (CE2 = LOW)
    DDRA  = 0xFF;                       // Configure Z80 data bus D0-D7 (PA0-PA7) as output
    PORTA = LD_HLnn;                    // Write "LD HL, n" opcode on data bus
    pulseClock(2);                      // Execute T2 and T3 cycles of M1
    DDRA  = 0x00;                       // Configure Z80 data bus D0-D7 (PA0-PA7) as input...
    PORTA = 0xFF;                       // ...with pull-up
    pulseClock(2);                      // Complete the execution of M1 and execute the T1 cycle of the following
    // Memory Read machine cycle
    DDRA  = 0xFF;                       // Configure Z80 data bus D0-D7 (PA0-PA7) as output
    PORTA = lowByte(value);             // Write first byte of "value" to load in HL
    pulseClock(3);                      // Execute the T2 and T3 cycles of the first Memory Read machine cycle
    // and T1, of the second Memory Read machine cycle
    PORTA = highByte(value);            // Write second byte of "value" to load in HL
    pulseClock(2);                      // Execute the T2 and T3 cycles of the second Memory Read machine cycle
    DDRA  = 0x00;                       // Configure Z80 data bus D0-D7 (PA0-PA7) as input...
    PORTA = 0xFF;                       // ...with pull-up
    digitalWrite(RAM_CE2, HIGH);        // Enable the RAM again (CE2 = HIGH)
 }


// ------------------------------------------------------------------------------
// Reset the Z80 CPU using single pulses clock
// ------------------------------------------------------------------------------
void singlePulsesResetZ80()
{
    digitalWrite(RESET_, LOW);          // Set RESET_ active
    pulseClock(6);                      // Generate twice the needed clock pulses to reset the Z80
    digitalWrite(RESET_, HIGH);         // Set RESET_ not active
    pulseClock(2);                      // Needed two more clock pulses after RESET_ goes HIGH
}


// ------------------------------------------------------------------------------
// put the byte pointed to by HL register pair onto the Data Bus
// ------------------------------------------------------------------------------
byte readByteFromRAM( word address )
{
    byte rtn_val = 0;

    loadHL( address );          // set where we're looking

    // Execute the LD A,(HL) instruction (T = 4+3+3). See the Z80 datasheet and manual.
    // After the execution of this instruction the <value> byte is loaded in the memory address pointed by HL.
    pulseClock(1);              // Execute the T1 cycle of M1 (Opcode Fetch machine cycle)
    digitalWrite(RAM_CE2, LOW); // Force the RAM in HiZ (CE2 = LOW)
    DDRA = 0xFF;                // Configure Z80 data bus D0-D7 (PA0-PA7) as output
    PORTA = LD_A_HL;            // Write "LD A,(HL)" opcode on data bus
    pulseClock(2);              // Execute T2 and T3 cycles of M1
    DDRA = 0x00;                // Configure Z80 data bus D0-D7 (PA0-PA7) as input...
    PORTA = 0xFF;               // ...with pull-up
    pulseClock(2);              // Complete the execution of M1 and execute the T1 cycle of the following

    rtn_val = PORTA;            // see what is currently on the data bus
    pulseClock(2);              // Complete the execution of M1 and execute the T1 cycle of the following

    Serial.printf("Byte data %02X", rtn_val );

    return( rtn_val );
}
 
word read16bitFromRAM( word address )
{
    byte HiByte = 0;
    byte LoByte = 0;

    HiByte = readByteFromRAM(address);
    LoByte = readByteFromRAM(address + 1);

    return( (word)(HiByte << 8) + LoByte );
}

void write16bitToRAM( word address, word value )
{

}


// ------------------------------------------------------------------------------
// set the XYZPQ flags
// set the global flags based on the supplied byte
// ------------------------------------------------------------------------------
void setXYZPQ( byte item )
{
    // determine the type of instruction
    // get X,Y,Z,P and Q
    X = item & X_MASK;
    X >>= 6;

    Y = item & Y_MASK;
    Y >>= 3;

    Z = item & Z_MASK;

    P = item & P_MASK;
    P >>= 4;

    Q = item & Q_MASK;
    Q >>= 3;
}

// ------------------------------------------------------------------------------
// decode the unprefixed instructions
// ------------------------------------------------------------------------------
byte decodeUnprefixed( String *rtnString, uint16_t &address )
{
    byte instruction_length = 1;
    byte offsetByte = 0;
    word index  = 0;
    char str[10];

    // set instruction type flags
    setXYZPQ( readByteFromRAM( address ) );

    switch( X )
    {
        case 0:
        {
            switch( Z )
            {
                case 0:
                {
                    rtnString = new String(table_assrt[Y]);
                    if( Y > 1)
                    {
                        if( Y <= 3 )
                        {
                            // get offset byte
                            offsetByte = readByteFromRAM( address + 1);
                            instruction_length++;
                            rtnString->concat( itoa(offsetByte, str, 16) );
                        }
                        else
                        {
                            rtnString->concat(table_cc[Y-4]);
                        }
                    }
                }
                break;

                case 1:
                {
                    if( Q == 0 )
                    {
                        rtnString = new String( "LD " );
                        rtnString->concat( table_rp[P]);
                        rtnString->concat( ",nn" );
                    }
                    else
                    {
                        rtnString = new String( "ADD HL, ");
                         
                        rtnString->concat(table_rp[P]);
                    }
                }
                break;

                case 2:
                {
                    rtnString = new String( table_ind[Q][P]);
                }
                break;
                 
                case 3:
                {
                    rtnString = new String( table_incdec[Q] );
                    rtnString->concat(table_rp[P]);
                }
                break;

                case 4:
                case 5:
                {
                    rtnString = new String( table_incdec[Z-4] );
                    rtnString->concat( table_r[Y]  );
                }
                break;
                 
                case 6:
                {
                    offsetByte = readByteFromRAM( address + 1);
                    instruction_length++;
                    rtnString = new String( "LD " );
                    rtnString->concat( table_r[Y] );
                    rtnString->concat( ", n" );
                    rtnString->replace("n", itoa(offsetByte, str, 16));
                    break;
                }
                case 7:
                {
                    rtnString = new String(table_accFlags[Y]);
                    break;
                }
            }
        }
        break;

        case 1:
        {
            // check for HALT instruction
            if( Y == 6 )
            {
                rtnString = new String( "HALT" );
            }
            else
            {
                rtnString = new String( "LD ra,rb" );
                rtnString->replace("ra", table_r[Y] );
                rtnString->replace("rb", table_r[Z] );
            }
        }
        break;

        case 2:
        {
            rtnString = new String( table_alu[Y] );
            rtnString->concat( " " );
            rtnString->concat( table_r[Z] );
        }
        break;

        case 3:
        {
            switch( Z )
            {
                case 0:
                rtnString = new String( table_cc[Y] );
                break;

                case 1:
                if( Q == 0 )
                {
                    rtnString = new String( "POP ");
                    rtnString->concat( table_rp2[P] );
                }
                break;

                case 2:
                rtnString = new String( "JP cc,nn" );
                break;

                case 3:
                if( Y != 1 )
                {
                    rtnString = new String(table_assrt2[Y]);
                    switch( Y )
                    {
                        case 2:
                        case 3:
                        index = readByteFromRAM(address + 1);
                        instruction_length++;
                        sprintf(str,"%02X",(word)index);
                        rtnString->replace("n", str);
                        break;
                    }
                }
                break;

                case 4:
                rtnString = new String( "CALL cc,nn" );
                break;

                case 5:
                if( Q == 0 )
                {
                    rtnString = new String( "PUSH ");
                    rtnString->concat( table_rp2[P] );
                }
                else
                {
                    if( P == 0)
                    {
                        rtnString = new String( "CALL nn");
                    }
                }
                break;

                case 6:
                rtnString  = new String( table_alu[Y] );
                offsetByte = readByteFromRAM(address + 1);
                instruction_length++;
                sprintf(str,"%02X",(byte)offsetByte);
                rtnString->concat( str );
                break;

                case 7:
                rtnString  = new String( "RST n");
                offsetByte = Y << 3;
                sprintf(str,"%02X",(byte)offsetByte);
                rtnString->replace("n", str);
                break;
            }
        }
        break;
    }

    rtnString->replace( "cc", table_cc[Y]);

    if( rtnString->lastIndexOf(sixteenBitValue))
    {
        index = read16bitFromRAM( address + 1 );
        instruction_length++;
        instruction_length++;
        sprintf(str,"%04X",(word)index);
        rtnString->replace(sixteenBitValue, str);
    }

    return( instruction_length );
}

 // ------------------------------------------------------------------------------
 // decode the CB prefixed instructions
 // ------------------------------------------------------------------------------
 byte decodeCB( String *rtnString, uint16_t &address )
 {
     byte instruction_length = 1;
     char str[10];
     // set instruction type flags
     setXYZPQ( readByteFromRAM( address ) );

     if( X == 0 )
     {
         rtnString = new String( table_rot[Y]);
         rtnString->concat( " r" );
     }
     else
     {
         rtnString = new String( table_bitops[X] );
         rtnString->concat( " " );
         sprintf(str,"%u",Y);
         rtnString->concat( str );
         rtnString->concat( ",r" );
     }

     rtnString->replace("r", table_r[Z]);

     return( instruction_length );
 }


 // ------------------------------------------------------------------------------
 // decode the ED prefixed instructions
 // ------------------------------------------------------------------------------
 byte decodeED( String *rtnString, uint16_t &address )
 {
     byte instruction_length = 1;
     char str[10];
     // set instruction type flags
     setXYZPQ( readByteFromRAM( address ) );

     // check for NOP
     if( ( X == 0 ) || ( X == 3 ))
     {
         rtnString = new String(table_assrt[0]);
     }
     else
     {
         if( X == 1 )
         {
             switch( Z )
             {
                 case 0:
                 {
                     rtnString = new String( "IN ");
                     if( Y != 6 )
                     {
                         rtnString->concat( table_r[Y] );
                         rtnString->concat( "," );
                     }
                     rtnString->concat( "(C)" );
                 }
                 break;

                 case 1:
                 {
                     rtnString = new String( "OUT (C),0" );
                     if( Y != 6 )
                     {
                         rtnString->replace( "0", table_r[Y] );
                     }
                 }
                 break;

                 case 2:
                 {
                     if( Q == 0 )
                     {
                         rtnString = new String( "SB" );
                     }
                     else
                     {
                         rtnString = new String( "AD" );
                     }
                     rtnString->concat( "C HL,");
                     rtnString->concat( table_rp[P]);
                 }
                 break;

                 case 3:
                 {
                     rtnString = new String( "DL " );
                     if( Q == 0 )
                     {
                         rtnString->concat( "(nn), rp[p]" );
                     }
                     else
                     {
                         rtnString->concat( "rp[p],(nn)" );
                     }

                     // insert relevant register pair
                     rtnString->replace("rp[p]", table_rp[P] );
                     instruction_length++;
                     instruction_length++;

                     sprintf(str, "%04X", read16bitFromRAM( address + 1 ) );
                     rtnString->replace( sixteenBitValue, str );
                 }
                 break;

                 case 4:
                 {
                     rtnString = new String( "NEG");
                 }
                 break;

                 case 5:
                 {
                     rtnString = new String( "RETN");
                     if( Y == 1 )
                     {
                         rtnString->replace("N","I");
                     }
                 }
                 break;

                 case 6:
                 {
                     rtnString = new String("IM ");
                     rtnString->concat(table_im[Y]);
                 }
                 break;

                 case 7:
                 {
                     rtnString = new String( table_EDassrt[Y]);
                 }

                 break;
             }
         }
         else if(( X == 2 ) && (Z < 4) && ( Y >= 4))
         {
             rtnString = new String( table_bli[Y-4][Z] );
         }
         else
         {
             rtnString = new String( table_assrt[0] );
         }
     }

     return( instruction_length );
 }


 // ------------------------------------------------------------------------------
 // Decode DD or FD prefixed instructions
 // ------------------------------------------------------------------------------
 byte decodeDDFD( String *rtnString, uint16_t address, char *IndexRegister )
 {
     byte   item;
     char str[16];

     item = readByteFromRAM( address );
     if( ( item == 0xDD ) || ( item == 0xED ) || ( item == 0xDD ) )
     {
         // recursion
         rtnString = disassemble( address );
     }
     else if( item == 0xCB )
     {
         item = readByteFromRAM(address + 1);

         if( X == 1 )
         {

             rtnString = new String( "BIT y,(IX+d)" );
         }
         else
         {
             // check if we need an LD prefix
             if( Z != 6 )
             {
                 rtnString = new String("LD r[z],");
             }
             else
             {
                 rtnString = new String();
             }

             rtnString->concat(table_rbrs[X]);
             rtnString->concat(" y, (IX+d)");
         }

         rtnString->replace("r[z]", table_r[Z]);
         rtnString->replace("rot[y]", table_rot[Y]);

         sprintf( str, "%02X", Y );
         rtnString->replace( "y", str );

         sprintf( str, "%02X", item );
         rtnString->replace( "d", str );
         rtnString->replace( "IX", IndexRegister );
     }
 }
 

 // ------------------------------------------------------------------------------
 // Disassemble a single instruction at given "address"
 // returns a string that must be deleted after use
 // Executed on a we allocate, you delete basis!
 // ------------------------------------------------------------------------------
 String *disassemble( uint16_t &address )
 {
     String *rtnString = NULL;
     bool   complete = false;
     byte   item;

     while( !complete )
     {
         // Get byte
         item = readByteFromRAM( address );

         switch( item )
         {
             case 0xCB:
             address++;
             address += decodeCB( rtnString, address );
             break;
             case 0xED:
             address++;
             address += decodeED( rtnString, address );
             break;
             case 0xDD:
             address++;
             address += decodeDDFD( rtnString, address, "DD" );
             break;
             case 0xFD:
             address++;
             address += decodeDDFD( rtnString, address, "DD" );
             break;
             // no prefix byte
             default:
             address += decodeUnprefixed( rtnString, address );
             break;
         }

         complete = true;
     }
     return( rtnString );
 }

 
 // ------------------------------------------------------------------------------
 // assemble a single Z80 instruction
 // Returns number of bytes consumed
 //     String *instruction - what to encode
 //     uint16_t address    - where to put it
 // ------------------------------------------------------------------------------
 byte assemble( String *instruction, uint16_t address )
 {
     byte rtn_sts = 0;

     // check if this is a "LD"

     return( rtn_sts );
 }

// Main entry point for this functionality
void monitor()
{
    uint16_t address = 0xfd10;
    String *code = disassemble( address );
    if( code != NULL )
    {
        Serial.print("\n\r");
        Serial.print( code->begin());
        delete(code);
    }
}

// end of source file Monitor.cpp