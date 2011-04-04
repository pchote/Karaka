//***************************************************************************
//
//  File........: command.c
//  Description.: Responds to user commands over usb
//  Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include "command.h"
#include "main.h"
#include "gps.h"

// Note: 256 size is used to allow overflow -> circular buffer
// If buffer size is changed you will need to add explicit overflow checks
static unsigned char usart_input_buffer[256];
static unsigned char usart_input_read;
static volatile unsigned char usart_input_write;

static unsigned char usart_output_buffer[256];
static volatile unsigned char usart_output_read;
static unsigned char usart_output_write;

/*
 * Initialise the command parser
 */
void command_init(void)
{	
	error_state = NO_ERROR;

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

	command_checking_DLE_stuffing = FALSE;
	command_have_startbit = FALSE;
	command_cntr = 0;
	
    usart_input_write = 0;
    usart_input_read = 0;
    usart_output_write = 0;
    usart_output_read = 0;
}

/*
 * Data received interrupt
 */
SIGNAL(SIG_UART0_RECV)
{
    usart_input_buffer[usart_input_write++] = UDR1;
}

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
 * Max length 255 bytes
 */
void queue_data(unsigned char *data, unsigned char length)
{
    queue_send_byte(DLE);
    for (unsigned char i = 0; i < length; i++)
    {
        queue_send_byte(data[i]);
        if (data[i] == DLE)
            queue_send_byte(DLE);
    }
    queue_send_byte(DLE);
	queue_send_byte(ETX);
}

void send_timestamp(timestamp *t)
{
    unsigned char data[] =
    {
    	t->hours,
    	t->minutes,
    	t->seconds,
    	t->day,
    	t->month,
    	t->year >> 8,
    	t->year & 0x00FF,
    	t->locked
    };
    queue_data(data, 8);
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
 * Process a command packet
 */
/*
static void process_packet(void)
{
	command_reply_cntr = 0;
	command_reply_packet[command_reply_cntr++] = command_packet[0];  //put same packet ID in reply packet
	command_stored_error_state = error_state;
	error_state = NO_ERROR;
	command_reply_packet[command_reply_cntr++] = command_stored_error_state;  //put error code into reply packet
	switch (command_packet[0])	//get packet ID
	{
		case ECHO:
		break;
		
		case GET_STATUS:
			command_reply_packet[command_reply_cntr++] = nibble_to_ascii((status_register>>4)&0x0f);
			command_reply_packet[command_reply_cntr++] = nibble_to_ascii(status_register&0x0f);
		break;
		
		case SET_CONTROL:
			control_register = command_packet[2];
			
		case GET_CONTROL:
			command_reply_packet[command_reply_cntr++] = nibble_to_ascii((control_register>>4)&0x0f);
			command_reply_packet[command_reply_cntr++] = nibble_to_ascii(control_register&0x0f);
		break;
		
		case GET_UTCTIME:
			if (gps_timestamp_stale)
			{
				command_stored_error_state |= UTC_ACCESS_ON_UPDATE;
				command_stored_error_state = command_stored_error_state & 0xFE;
			}
			else
			{	
				if (!gps_last_timestamp.locked)
        		{
        			command_stored_error_state |= GPS_TIME_NOT_LOCKED;
        			command_stored_error_state = command_stored_error_state & 0xFE;					
        		}
        		
				write_number(gps_last_timestamp.year, 4);
				command_reply_packet[command_reply_cntr++] = ':';
				write_number(gps_last_timestamp.month, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				write_number(gps_last_timestamp.day, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				write_number(gps_last_timestamp.hours, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				write_number(gps_last_timestamp.minutes, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				write_number(gps_last_timestamp.seconds, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				write_number(milliseconds, 3);
			}
		break;
		
		case SET_CCD_EXPOSURE:
			cli();
			exposure_total = ascii_to_nibble(command_packet[2])*1000
							+ ascii_to_nibble(command_packet[3])*100
							+ ascii_to_nibble(command_packet[4])*10
							+ ascii_to_nibble(command_packet[5]);
			
			exposure_count = 0;
			exposure_syncing = TRUE;
			sei();
		
		case GET_CCD_EXPOSURE:
			write_number(exposure_total, 4);
		break;
		
		case GET_EOFTIME:
			if (gps_timestamp_stale)
			{
				command_stored_error_state |= EOF_ACCESS_ON_UPDATE;
				command_stored_error_state = command_stored_error_state & 0xFE;
			}
			else
			{
				if (!gps_last_synctime.locked)
        		{
        			command_stored_error_state |= GPS_TIME_NOT_LOCKED;
        			command_stored_error_state = command_stored_error_state & 0xFE;					
        		}
        		
				write_number(gps_last_synctime.year, 4);
				command_reply_packet[command_reply_cntr++] = ':';
				write_number(gps_last_synctime.month, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				write_number(gps_last_synctime.day, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				write_number(gps_last_synctime.hours, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				write_number(gps_last_synctime.minutes, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				write_number(gps_last_synctime.seconds, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				write_number(0,3);
			}
		break;
		
		default:
			command_stored_error_state |= PACKETID_INVALID;
			command_stored_error_state = command_stored_error_state & 0xFE;
		break;
	}
	command_reply_packet[1] = command_stored_error_state;
	send_packet();
}
*/



/*
 * Interrupt service routine for Data received from UART
 * Receives one byte of data from the USART. Determines
 * command char received and execute code as required.
 */
//SIGNAL(SIG_UART0_RECV)
/*
void foo()
{
    return;
	unsigned char userCommand = receive_byte();
	if ((userCommand == 0x10) && !command_have_startbit) //check if received byte is physical layer packet start byte		
		command_have_startbit = TRUE;
	else if (command_have_startbit) //if already processing packet continue to scan for packet end byte
	{	
		command_packet[command_cntr++] = userCommand;		//save received byte to packet buffer
		if (userCommand == 0x10)  
		{
			if (command_checking_DLE_stuffing)
			{
				command_checking_DLE_stuffing = FALSE;
				command_cntr--;		//write over 2nd DLE byte in gps_packet
			}
			else
				command_checking_DLE_stuffing = TRUE;
		}
		else if (userCommand == 0x03)
		{
			if (command_checking_DLE_stuffing)  //this is true if the last =byte received was an odd numbered DLE
			{
				command_cntr--;		//remove the ETX byte
				command_cntr--;		//remove the DLE byte
				command_have_startbit = FALSE;
				process_packet();
				command_cntr = 0;
			}
		}
		else 
			command_checking_DLE_stuffing = FALSE;
	}
}
*/

