//***************************************************************************
//
//  File........: gps.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Parses the serial stream from a Trimble GPS unit
//                and extracts timestamp information
//
//***************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "main.h"
#include "display.h"
#include "gps.h"
#include "command.h"
	
/*
 * Initialise the gps listener on USART1
 */
void gps_init(void)
{
	// Enable 2x speed
	UCSR1A = (1<<U2X1);

	// Enable transmitter.  .
	UCSR1B = (1<<RXEN1)|(1<<TXEN1)|(1<<RXCIE1)|(0<<UDRIE1);

	// Async. mode, 8N1 (8 data bits, No parity, 1 stop bit)
	UCSR1C = (0<<UMSEL1)|(0<<UPM01)|(0<<USBS1)|(3<<UCSZ10)|(0<<UCPOL1);
	
	// Set baud rate to 9600
	UBRR1H = 0x00;
	UBRR1L = 0xCF;
	
	// Initialise the clocks to zero
	gps_last_timestamp.seconds = 0;
	gps_last_timestamp.minutes = 0;
	gps_last_timestamp.hours = 0;
	gps_last_timestamp.day = 0;
	gps_last_timestamp.month = 0;
	gps_last_timestamp.year = 0;
	
	gps_last_synctime.seconds = 0;
	gps_last_synctime.minutes = 0;
	gps_last_synctime.hours = 0;
	gps_last_synctime.day = 0;
	gps_last_synctime.month = 0;
	gps_last_synctime.year = 0;
	
	gps_record_synctime = FALSE;
	gps_state = SYNCING;	//set GPS state to syncing, as we need to sync with incoming packets
	//gps_send_magellan_init();
}

/*
 * Send one byte through USART1
 */
void gps_transmit_byte(unsigned char data)
{
	while (!(UCSR1A & (1<<UDRE1)));	
	UDR1 = data;
	
	// Wait until transmission is complete.
	while (!(UCSR1A & (1<<TXC1)));
}

/*
 * Receive one byte through USART1
 */
unsigned char gps_receive_byte(void)
{
	while (!(UCSR1A & (1<<RXC1)));
	return UDR1;
}

/*
 * Send Magellan initialisation strings
 */
void gps_send_magellan_init(void)
{
	// Enable binary timing and status packets
	char *msg = PSTR("$PMGLI,00,A00,2,B\r\n$PMGLI,00,H00,2,B");
	for (int i = 0; i < 36; i++)
		gps_transmit_byte(msg[i]);
}

void gps_timeout(void)
{
	gps_state = SYNCING;
	error_state |= GPS_SERIAL_LOST;
	error_state = error_state & 0xFE;
	gps_timeout_count = 0;
}

/* 
 * Interrupt service routine for Data received from UART
 * Receives one byte of data from the USART. Determines
 * command char received and execute code as required.
 */
SIGNAL(SIG_UART1_RECV)
{
	// Signal that the gps is alive
	gps_timeout_count = 0;
	
	unsigned char incomingbyte = gps_receive_byte();
	
	// Binary data packets must start with `$$'
	if ((gps_packet_cntr == 0 && incomingbyte != '$') ||
		(gps_packet_cntr == 1 && incomingbyte != '$'))
	{
		gps_packet_cntr = 0;
		gps_state = SYNCING;
		return;
	}
	
	// TODO: Use the other gps_states sanely
	gps_state = GPS_TIME_GOOD;
	
	// Store the packet
	gps_packet[gps_packet_cntr++] = incomingbyte;
	
	// End of command is given by a linefeed
	if (incomingbyte == 0x0A)
	{
		gps_processing_packet = TRUE;

		// Check the command checksum.
		//   gps_packet_cntr points at the next memory loc.
		//   gps_packet_cntr - 1 is linefeed
		//   gps_packet_cntr - 2 is the checksum
		unsigned char csm = gps_packet[2];
		for (int i = 3; i < gps_packet_cntr-2; i++)
			csm ^= gps_packet[i];

		// Process the packet if the checksum is valid
		if (csm == gps_packet[gps_packet_cntr-2])
		{
			// status packet - check that time is valid
			if (gps_packet[2] == 'H')
			{
				//error_state |= GPS_TIME_NOT_LOCKED;
				//error_state = error_state & 0xFE;					
			}
			
			// time packet
			else if (gps_packet[2] == 'A')
			{
				gps_last_timestamp.hours = gps_packet[4];
				gps_last_timestamp.minutes = gps_packet[5];
				gps_last_timestamp.seconds = gps_packet[6];
				gps_last_timestamp.day = gps_packet[7];
				gps_last_timestamp.month = gps_packet[8];
				gps_last_timestamp.year = ((gps_packet[9] << 8) & 0xFF00) | (gps_packet[10] & 0x00FF);
				
				
				// TODO: Reimplement wait for 10 second boundary
				if(wait_4_timestamp)
					wait_4_timestamp = FALSE;
					
				if (gps_record_synctime == RECORD_THIS_PACKET)
				{
					gps_last_synctime.seconds = gps_last_timestamp.seconds;
					gps_last_synctime.minutes = gps_last_timestamp.minutes;
					gps_last_synctime.hours = gps_last_timestamp.hours;
					gps_last_synctime.day = gps_last_timestamp.day;
					gps_last_synctime.month = gps_last_timestamp.month;
					gps_last_synctime.year = gps_last_timestamp.year;

					gps_record_synctime = SYNCTIME_VALID;
				}
			}
		}
		
		// Reset for the next packet
		gps_packet_cntr = 0;
		
		gps_processing_packet = FALSE;
		if (gps_record_synctime == RECORD_NEXT_PACKET)
			gps_record_synctime = RECORD_THIS_PACKET;
	}
}