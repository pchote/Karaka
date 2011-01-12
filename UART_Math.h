//***************************************************************************
//
//  File........: UART_Math.h
//
//  Author(s)...: Johnny McClymont
//
//  Target(s)...: ATmega128
//
//  Compiler....: AVR-GCC 3.3.1; avr-libc 1.0
//
//  Description.: ATMega128 Rhino Driver Math Routines for MARVIN
//
//  Revisions...: 1.0
//
//  YYYYMMDD - VER. - COMMENT                                       - SIGN.
//
//  20070822 - 1.0  - Created                                       - J.McClymont
//
//***************************************************************************

unsigned char convert_ASCII_to_HEX(int ASCII_Char);
char hexToAscii(char first);
int Dec2Hex(unsigned char MSB, unsigned char LSB);
unsigned char sendDecimal(int number, unsigned char places, unsigned char my_ptr);


