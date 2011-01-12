//***************************************************************************
//
//  File........: usart.h
//
//  Author(s)...: Johnny McClymont
//
//  Target(s)...: ATmega128
//
//  Compiler....: AVR-GCC 3.3.1; avr-libc 1.0
//
//  Description.: ATMega128 USART routines
//
//  Revisions...: 1.0
//
//  YYYYMMDD - VER. - COMMENT                                       - SIGN.
//
//  20070824 - 1.0  - Created                                       - J.McClymont
//
//***************************************************************************

void USART_Init(unsigned int baudrate);
void Usart_Tx(char);
char Usart_Rx(void);
void sendmsg (const char *s);
char startBit_Rcvd;
unsigned char userCommand;;		//buffer for incoming byte
unsigned char checking_DLE_stuffing_flag;		//flag to check if packet has DLE stuffed

