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
	
	
	gps_packet_type = UNKNOWN_PACKET;
	gps_packet_length = 0;
	gps_packet_received = 0;
	
	gps_record_synctime = FALSE;
	gps_state = NO_GPS;
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
	gps_state = NO_GPS;
	error_state |= GPS_SERIAL_LOST;
	
	gps_packet_type = UNKNOWN_PACKET;
	gps_packet_length = 0;
	gps_packet_received = 0;
	
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
	
	unsigned char b = gps_receive_byte();
	
	// Binary data packets must start with `$$'
	if ((gps_packet_received == 0 && b != '$') ||
		(gps_packet_received == 1 && b != '$'))
	{
		gps_packet_received = 0;
		gps_state = SYNCING;
		return;
	}
	
	if (gps_packet_received == 2)
	{
		switch (b)
		{
			case 'A':
				gps_packet_type = MAGELLAN_TIME_PACKET;
				gps_packet_length = 13;
			break;
			case 'H':
				gps_packet_type = MAGELLAN_STATUS_PACKET;
				gps_packet_length = 16;
			break;
			default:
				gps_packet_type = UNKNOWN_PACKET;
				gps_packet_length = 0;
				gps_packet_received = 0;
			break;
		}
	}
	
	// Store the packet
	gps_packet[gps_packet_received++] = b;
	
	if (gps_packet_received == gps_packet_length)
	{
		// TODO: Is this still necessary?
		gps_processing_packet = TRUE;
		
		// Check that the packet is valid.
		// A valid packet will have the final byte as a linefeed (0x0A)
		// and the second-to-last byte will be a checksum which will match
		// the XORed bytes between the $$ and checksum.
		//   gps_packet_received - 1 is the linefeed
		//   gps_packet_received - 2 is the checksum byte
		if (b == 0x0A)
		{
			unsigned char csm = gps_packet[2];
			for (int i = 3; i < gps_packet_received-2; i++)
				csm ^= gps_packet[i];

			// Verify the checksum
			if (csm == gps_packet[gps_packet_received-2])
			{
				switch (gps_packet_type)
				{
					case MAGELLAN_STATUS_PACKET:
						//error_state |= GPS_TIME_NOT_LOCKED;
						//error_state = error_state & 0xFE;
						
						// TODO: Handle locked/not locked
					break;
					case MAGELLAN_TIME_PACKET:
						gps_last_timestamp.hours = gps_packet[4];
						gps_last_timestamp.minutes = gps_packet[5];
						gps_last_timestamp.seconds = gps_packet[6];
						gps_last_timestamp.day = gps_packet[7];
						gps_last_timestamp.month = gps_packet[8];
						gps_last_timestamp.year = ((gps_packet[9] << 8) & 0xFF00) | (gps_packet[10] & 0x00FF);
						
						// Mark that we have a valid timestamp
						gps_timestamp_stale = FALSE;
						gps_state = GPS_TIME_GOOD;
						
						// Synchronize the exposure countdown with the 10 second mark
						if (exposure_syncing && (gps_last_timestamp.seconds % 10 == 0))
							exposure_syncing = FALSE;
					
						if (gps_record_synctime == RECORD_THIS_PACKET)
						{
							gps_last_synctime.seconds = gps_last_timestamp.seconds;
							gps_last_synctime.minutes = gps_last_timestamp.minutes;
							gps_last_synctime.hours = gps_last_timestamp.hours;
							gps_last_synctime.day = gps_last_timestamp.day;
							gps_last_synctime.month = gps_last_timestamp.month;
							gps_last_synctime.year = gps_last_timestamp.year;
							gps_record_synctime = FALSE;
						}
					break;
				}
			}
		}
		// Finished parsing packet, or packet is invalid.
		// Reset for the next packet
		gps_packet_received = 0;
		gps_packet_type = UNKNOWN_PACKET;
		
		gps_processing_packet = FALSE;
		if (gps_record_synctime == RECORD_NEXT_PACKET)
			gps_record_synctime = RECORD_THIS_PACKET;
	}
}