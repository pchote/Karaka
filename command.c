//***************************************************************************
//
//	File........: command.c
//	Description.: Responds to user commands over usb
//	Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//	This file is part of Karaka, which is free software. It is made available
//	to you under the terms of version 3 of the GNU General Public License, as
//	published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include "command.h"
#include "main.h"
#include "gps.h"

static unsigned char usart_packet_type;
static unsigned char usart_packet_length;
static unsigned char usart_packet[256];

// Note: 256 size is used to allow overflow -> circular buffer
// If buffer size is changed you will need to add explicit overflow checks
static unsigned char usart_input_buffer[256];
static unsigned char usart_input_read;
static volatile unsigned char usart_input_write;

static unsigned char usart_output_buffer[256];
static volatile unsigned char usart_output_read;
static volatile unsigned char usart_output_write;

/*
 * Initialise the command parser
 */
void command_init(void)
{	
	// Set TXD (port E, pin 1) as an output
	DDRE |= (1<<DDE1);
	
	// Set the baudrate prescaler
	// scale = (f_cpu / (16*baud)) - 1
	unsigned int baudrate = 16; // has a 2.5% error
	UBRR0H = (unsigned char)(baudrate>>8);
	UBRR0L = (unsigned char)baudrate;


	// Enable receiver and transmitter. Enable Receive interrupt, disable transmit interrupt.
	// Set USART capabilities
	// RXEN0 = 1: enable recieve
	// TXEN0 = 1: enable transmit
	// RXCIE0 = 1: enable recieve interrupt
	UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);

	// Set the data frame format
	// UMSEL0 = 0: set async operation
	// UPM00 = 0: no parity
	// USBS0 = 0: 1 stop bit
	// UCSZ00 = 3: 8 data bits
	// UCPOL0 = 0: ???
	UCSR0C = (0<<UMSEL0)|(0<<UPM00)|(0<<USBS0)|(3<<UCSZ00)|(0<<UCPOL0);
	
	// Double the prescaler frequency
	UCSR0A = (1<<U2X0);

	usart_input_write = 0;
	usart_input_read = 0;
	usart_output_write = 0;
	usart_output_read = 0;
	
	usart_packet_type = UNKNOWN_PACKET;
}

/*
 * Calculate a checksum by XORing bytes 
 */
static unsigned char checksum(unsigned char *data, unsigned char length)
{
	unsigned char csm = data[0];
	for (unsigned char i = 1; i < length; i++)
		csm ^= data[i];
	return csm;
}

/*
 * Add a byte to the send queue and start sending data if necessary
 */
static void queue_send_byte(unsigned char b)
{
	// TODO: check to ensure we don't overwrite data we haven't sent yet
	usart_output_buffer[usart_output_write++] = b;
	
	// Enable Transmit data register empty interrupt if necessary to send bytes down the line
	cli();
	if ((UCSR0B & (1<<UDRIE0)) == 0)
		UCSR0B |= (1<<UDRIE0);
	sei();
}


/*
 * Queue a data packet to the aquisition pc
 */
static void queue_data(unsigned char type, unsigned char *data, unsigned char length)
{
	queue_send_byte(DLE);
	queue_send_byte(length);
	queue_send_byte(type);
	for (unsigned char i = 0; i < length; i++)
	{
		queue_send_byte(data[i]);
		if (data[i] == DLE)
			queue_send_byte(DLE);
	}
	// send the checksum of the *unpadded* data
	queue_send_byte(checksum(data, length));
	queue_send_byte(DLE);
	queue_send_byte(ETX);
}

/*
 * Queue a timestamp packet to the acquisition pc
 */
void send_timestamp()
{
	unsigned char data[] =
	{
		gps_last_timestamp.hours,
		gps_last_timestamp.minutes,
		gps_last_timestamp.seconds,
		gps_last_timestamp.day,
		gps_last_timestamp.month,
		gps_last_timestamp.year & 0x00FF,
		gps_last_timestamp.year >> 8,
		gps_last_timestamp.locked,
		exposure_count
	};
	queue_data(CURRENTTIME, data, 9);
}

void send_downloadtimestamp()
{
	unsigned char data[] =
	{
		gps_last_synctime.hours,
		gps_last_synctime.minutes,
		gps_last_synctime.seconds,
		gps_last_synctime.day,
		gps_last_synctime.month,
		gps_last_synctime.year & 0x00FF,
		gps_last_synctime.year >> 8,
		gps_last_synctime.locked
	};
	queue_data(DOWNLOADTIME, data, 8);
}

void send_debug_string(char *string)
{
	queue_data(DEBUG_STRING, (unsigned char *)string, strlen(string));
}

void send_debug_raw(unsigned char *data, unsigned char length)
{
    queue_data(DEBUG_RAW, data, length);
}

/*
 * data register empty interrupt to send a byte down the wire
 */
SIGNAL(SIG_UART0_DATA)
{
	if(usart_output_write != usart_output_read)
		UDR0 = usart_output_buffer[usart_output_read++];
	
	// Ran out of data to send - disable the interrupt
	if(usart_output_write == usart_output_read) 
		UCSR0B &= ~(1<<UDRIE0);
}


/*
 * Data received interrupt
 */
SIGNAL(SIG_UART0_RECV)
{
	usart_input_buffer[usart_input_write++] = UDR0;
}

/*
 * Process any data in the received buffer
 * Parses at most one packet - so must be called frequently
 * Note: this relies on the usart_input_buffer being 256 chars long so that
 * the data pointers automatically overflow at 256 to give a circular buffer
 */
unsigned char usart_process_buffer()
{
	// Take a local copy of usart_input_write as it can be modified by interrupts
	unsigned char temp_write = usart_input_write;
	
	// No new data has arrived
	if (usart_input_read == temp_write)
		return FALSE;
	
	// Sync to the start of a packet if necessary
	for (; usart_packet_type == UNKNOWN_PACKET && usart_input_read != temp_write; usart_input_read++)
	{	
		if ( // Start of timing packet
			usart_input_buffer[usart_input_read - 1] == DLE &&
			// End of previous packet
			usart_input_buffer[usart_input_read - 2] == ETX &&
			usart_input_buffer[usart_input_read - 3] == DLE)
		{
			usart_packet_type = usart_input_buffer[usart_input_read];
			// Rewind to the start of the packet
			usart_input_read -= 1;
			break;
		}
	}
	
	// Write bytes into the packet buffer
	for (; usart_input_read != temp_write; usart_input_read++)
	{
		// Skip the padding byte that occurs after a legitimate DLE
		// Don't loop this: we want to parse the 3rd DLE if you have
		// 4 in a row
		if (usart_input_buffer[usart_input_read] == DLE &&
			usart_input_buffer[usart_input_read - 1] == DLE)
			usart_input_read++;

		usart_packet[usart_packet_length++] = usart_input_buffer[usart_input_read];
	
		// End of packet (data length is the second byte + 6 frame bytes)
		if (usart_packet_length > 2 && usart_packet_length == usart_packet[1] + 6)
		{
			// Check checksum
			if (checksum(&usart_packet[3], usart_packet[1]) == usart_packet[usart_packet_length-3])
			{
				// Handle packet
				switch(usart_packet[2])
				{
					case EXPOSURE:
						cli();
						exposure_total = usart_packet[3];
						exposure_count = exposure_total;
						exposure_syncing = TRUE;
			
						// Enable gps pulse interrupt if exposure != 0
						EIMSK = (!(exposure_total == 0)<<INT0);
						sei();	
					break;
				}
			}
			else
			{
				send_debug_string("Command packet checksum failed");
			}
			
			// Reset for next packet
			usart_packet_type = UNKNOWN_PACKET;
			usart_packet_length = 0;
			return TRUE;
		}
	}
	return FALSE;
}
