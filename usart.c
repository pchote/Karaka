//***************************************************************************
//
//  File........: usart.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: ATMeag128 USART routines
//
//***************************************************************************

#include <avr/io.h>
#include "UART_Math.h"
#include "usart.h"
#include "main.h"
#include "Command_Layer.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
	
/*
 * Initialise the USART
 * TODO: Give a helpful message
 */
void USART_Init(unsigned int baudrate)
{
	// Set baud rate
	UBRR0H = (unsigned char)(baudrate>>8);
	UBRR0L = (unsigned char)baudrate;

	// Enable 2x speed
	UCSR0A = (1<<U2X0);

	// Enable receiver and transmitter. Enable Receive interrupt, disable transmit interrupt.
	UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0)|(0<<UDRIE0);

	// Async. mode, 8N1 (8 data bits, No parity, 1 stop bit)
	UCSR0C = (0<<UMSEL0)|(0<<UPM00)|(0<<USBS0)|(3<<UCSZ00)|(0<<UCPOL0);
	checking_DLE_stuffing_flag = 0;
	startBit_Rcvd = 0;
	command_cntr = 0;
}

/*
 * Send one byte through the USART
 */
void Usart_Tx(char data)
{
    while (!(UCSR0A & (1<<UDRE0)));
    UDR0 = data;
}

/*
 * Receive one byte through the USART
 */
char Usart_Rx(void)
{
	char temp;
    while (!(UCSR0A & (1<<RXC)));
	temp= UDR0;
	
    return temp;
}

/*
 * Send a string through the USART
 */
void sendmsg (const char *s)
{
	unsigned char qcntr, sndcntr;	//indexes into the queue array holding the msg to send via UART
	unsigned char queue[100];		//character queue array holding msg to send via UART
	uint8_t i;
	qcntr = 0;		//preset indices
	sndcntr = 0;	

	for (i = 0; pgm_read_byte(&s[i]) && i < 100; i++)
		queue[qcntr++] = pgm_read_byte(&s[i]);

	while (qcntr != sndcntr)
		Usart_Tx(queue[sndcntr++]);
}

/*
 * Interrupt service routine for Data received from UART
 * Receives one byte of data from the USART. Determines
 * command char received and execute code as required.
 */
SIGNAL(SIG_UART0_RECV)
{
	userCommand = Usart_Rx();
	if ((userCommand == 0x10) && (startBit_Rcvd == 0))	//check if received byte is physical layer packet start byte
	{		
		startBit_Rcvd = 1; 
	}
	else if (startBit_Rcvd == 1) //if already processing packet continue to scan for packet end byte
	{	
		command_Packet[command_cntr++] = userCommand;		//save received byte to packet buffer
		if (userCommand == 0x10)  
		{
			if (checking_DLE_stuffing_flag == 1)
			{
				checking_DLE_stuffing_flag = 0;
				command_cntr--;		//write over 2nd DLE byte in GPS_Packet
			}
			else
			{
				checking_DLE_stuffing_flag = 1;
			}
		}
		else if (userCommand == 0x03)
		{
			if (checking_DLE_stuffing_flag == 1)  //this is true if the last =byte received was an odd numbered DLE
			{
				command_cntr--;		//remove the ETX byte
				command_cntr--;		//remove the DLE byte
				startBit_Rcvd = 0;
				Command_processPacket();		//process this packet
				command_cntr = 0;
			}
		}
		else 
		{
			checking_DLE_stuffing_flag = 0;
		}	
	}
}

/* 
 * Read a float (4 Bytes) from PROGMEM
 * the idea using a union is from Joerg Wunsch, found
 * on the avr-gcc mailing-list, marked as "public domain",
 * modifications by Martin Thomas, KL, .de
 */
static inline float pgm_read_float_hlp(const float *addr)
{
	union
	{
		int i[2];	// uint16_t 
		float f;
	} u;
	
	u.i[0] = pgm_read_word((PGM_P)addr);
	u.i[1] = pgm_read_word((PGM_P)addr+2);
	return u.f;
}
