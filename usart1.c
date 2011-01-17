//***************************************************************************
//
//  File........: usart.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: ATMeag128 USART routines
//
//***************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "usart1.h"
#include "usart.h"
#include "main.h"
#include "GPS.h"
	
/*
 * Initialise the USART
 * TODO: Give a helpful description
 */
void USART1_Init(unsigned int baudrate)
{
	// Set baud rate
	UBRR1H = (unsigned char)(baudrate>>8);
	UBRR1L = (unsigned char)baudrate;

	// Enable 2x speed
	UCSR1A = (1<<U2X1);

	// Enable transmitter.  .
	UCSR1B = (1<<RXEN1)|(1<<TXEN1)|(1<<RXCIE1)|(0<<UDRIE1);

	// Async. mode, 8N1 (8 data bits, No parity, 1 stop bit)
	UCSR1C = (0<<UMSEL1)|(0<<UPM01)|(0<<USBS1)|(3<<UCSZ10)|(0<<UCPOL1);
	UBRR1H = (unsigned char)(baudrate>>8);
	UBRR1L = (unsigned char)baudrate;	
	checking_DLE_stuffing = 0;
	gps_usart_state = SYNCING_PACKETS;
	sync_state = LOOK_FOR_DLE;
}

/*
 * Send a single byte through the USART1
 */
void Usart1_Tx(char data)
{
	while (!(UCSR1A & (1<<UDRE1)));	
	UDR1 = data;
	// Wait until transmission is complete.
	while (!(UCSR1A & (1<<TXC1)));
}

/*
 * Recieve a single byte from the USART1
 */
char Usart1_Rx(void)
{
	unsigned char temp;
    while (!(UCSR1A & (1<<RXC1)));  //wait here until there is data in the receive buffer
	temp= UDR1;

    return temp;
}

/* 
 * Interrupt service routine for Data received from UART
 * Receives one byte of data from the USART. Determines
 * command char received and execute code as required.
 */
SIGNAL(SIG_UART1_RECV)
{
	check_GPS_present = 0;
	incomingbyte = Usart1_Rx();
	if (gps_usart_state == CAPTURE_PACKETS)
	{
		if ((incomingbyte == 0x10) && (GPS_startBit_Rcvd == 0))	//check if received byte is physical layer packet start byte
		{		
			GPS_startBit_Rcvd = 1; 
		}
		else if (GPS_startBit_Rcvd == 1) //if already processing packet continue to scan for packet end byte
		{	
			GPS_Packet[packet_cntr++] = incomingbyte;		//save received byte to packet buffer
			if (incomingbyte == 0x10)  
			{
				if (checking_DLE_stuffing == 1)
				{
					checking_DLE_stuffing = 0;
					packet_cntr--;		//write over 2nd DLE byte in GPS_Packet
				}
				else
				{
					checking_DLE_stuffing = 1;
				}
			}
			else if (incomingbyte == 0x03)
			{
				if (checking_DLE_stuffing == 1)  //this is true if the last =byte received was an odd numbered DLE
				{
					packet_cntr--;		//remove the ETX byte
					packet_cntr--;		//remove the DLE byte
					GPS_startBit_Rcvd = 0;
					GPS_processPacket();		//process this packet
					packet_cntr = 0;
				}
			}
			else 
			{
				checking_DLE_stuffing = 0;
			}	
		}
	}
	
	/*
	The uC tries to sync to GPS packets by looking for the following byte string
	<DLE><ETX><DLE><some char besides DLE>
	We know we have located a valid packet start byte if we find a complete <DLE><ETX> packet terminator
	NOTE.  The leading <DLE> could of followed a <DLE> and be part of the data string of a packet but
			if we then receive a single <DLE> followed by a byte that's not DLE or ETX then we must have
			the start of a new packet, thus we know where we are.
	*/
	else if (gps_usart_state == SYNCING_PACKETS)		//if we are trying to sync to GPS packets
	{
		GPS_state = SYNCING;
		switch(sync_state)
		{
			case LOOK_FOR_DLE:
				if (incomingbyte == 0x10) 
				{
					sync_state = LOOK_FOR_ETX;
				}
			break;
			
			case LOOK_FOR_ETX:
				if (incomingbyte == 0x03) 
				{
					sync_state = LOOK_FOR_2ND_DLE;
				}
				else
				{
					sync_state = LOOK_FOR_DLE;
				}
			break;
			
			case LOOK_FOR_2ND_DLE:
				if (incomingbyte == 0x10) 
				{
					sync_state = LOOK_FOR_NO_DLE;
				}
				else
				{
					sync_state = LOOK_FOR_DLE;
				}
			break;
			
			case LOOK_FOR_NO_DLE:
				if (incomingbyte != 0x10) 
				{
					GPS_Packet[packet_cntr++] = incomingbyte;  //save the first byte of this new packet to the buffer
					GPS_startBit_Rcvd = 1;		//set flag to indicate we are recording a valid packet
					gps_usart_state = CAPTURE_PACKETS;		//change state as we are now synced to packets
					GPS_sendPacketMask();		//send SETUP packet to switch off most automatic packets
					//sendmsg(PSTR("SYNCED>"));
					GPS_state = CHECK_GPS_TIME_VALID;	//change state to check time from GPS is valid
					
					reset_LCD();		//reset the LCD
				}
				sync_state = LOOK_FOR_DLE;
			break;
			
			default:
				sync_state = LOOK_FOR_DLE;
			break;
		}
	}
}






