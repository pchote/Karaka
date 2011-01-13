//***************************************************************************
//
//  File........: usart.h
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: ATMega128 USART routines
//
//***************************************************************************

#ifndef KARAKA_USART_H
#define KARAKA_USART_H

void USART_Init(unsigned int baudrate);
void Usart_Tx(char);
char Usart_Rx(void);
void sendmsg (const char *s);
char startBit_Rcvd;
unsigned char userCommand;;		//buffer for incoming byte
unsigned char checking_DLE_stuffing_flag;		//flag to check if packet has DLE stuffed

#endif
