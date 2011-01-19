//***************************************************************************
//
//  File........: command.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Responds to user commands over usb
//
//***************************************************************************

#include <avr/interrupt.h>
#include "command.h"
#include "main.h"
#include "gps.h"
#include "msec_timer.h"

	
/*
 * Initialise the command parser
 */
void command_init(void)
{	
	error_state = NO_ERROR;

	// Initialise USART usb connection
	// Baudrate
	unsigned int baudrate = 16;
	UBRR0H = (unsigned char)(baudrate>>8);
	UBRR0L = (unsigned char)baudrate;

	// Enable 2x speed
	UCSR0A = (1<<U2X0);

	// Enable receiver and transmitter. Enable Receive interrupt, disable transmit interrupt.
	UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0)|(0<<UDRIE0);

	// Async. mode, 8N1 (8 data bits, No parity, 1 stop bit)
	UCSR0C = (0<<UMSEL0)|(0<<UPM00)|(0<<USBS0)|(3<<UCSZ00)|(0<<UCPOL0);
	
	command_checking_DLE_stuffing = FALSE;
	command_have_startbit = FALSE;
	command_cntr = 0;
}

/*
 * Process a command packet
 */
void command_process_packet(void)
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
				command_write_number(gps_last_timestamp.year, 4);
				command_reply_packet[command_reply_cntr++] = ':';
				command_write_number(gps_last_timestamp.month, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				command_write_number(gps_last_timestamp.day, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				command_write_number(gps_last_timestamp.hours, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				command_write_number(gps_last_timestamp.minutes, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				command_write_number(gps_last_timestamp.seconds, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				command_write_number(milliseconds, 3);
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
			command_write_number(exposure_total, 4);
		break;
		
		case GET_EOFTIME:
			if (gps_timestamp_stale)
			{
				command_stored_error_state |= EOF_ACCESS_ON_UPDATE;
				command_stored_error_state = command_stored_error_state & 0xFE;
			}			
			else
			{
				command_write_number(gps_last_synctime.year, 4);
				command_reply_packet[command_reply_cntr++] = ':';
				command_write_number(gps_last_synctime.month, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				command_write_number(gps_last_synctime.day, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				command_write_number(gps_last_synctime.hours, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				command_write_number(gps_last_synctime.minutes, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				command_write_number(gps_last_synctime.seconds, 2);
				command_reply_packet[command_reply_cntr++] = ':';
				command_write_number(0,3);
			}
		break;
		
		default:
			command_stored_error_state |= PACKETID_INVALID;
			command_stored_error_state = command_stored_error_state & 0xFE;
		break;
	}
	command_reply_packet[1] = command_stored_error_state;
	command_send_packet();
}

/*
 * Respond to a command packet using buffered data
 */
void command_send_packet(void)
{
	command_transmit_byte(0x10);
	for (unsigned char i = 0; i < command_reply_cntr; i++)
	{
		command_transmit_byte(command_reply_packet[i]);
		if (command_reply_packet[i] == 0x10)
			command_transmit_byte(0x10);
	}
	command_transmit_byte(0x10);
	command_transmit_byte(0x03);
}

void command_write_number(int number, unsigned char places)
{
	// Calculate the divisor for the highest place
	unsigned int div = 1;
	for (unsigned char i = 1; i < places; i++)
		div *= 10;
	
	// Loop over each digit in the number
	for (unsigned char p = 0; div > 0; div /= 10)
	{
		p = number / div;
		number %= div;
		command_reply_packet[command_reply_cntr++] = nibble_to_ascii(p);
	}
}

/*
 * Send one byte through the USART
 */
void command_transmit_byte(unsigned char data)
{
    while (!(UCSR0A & (1<<UDRE0)));
    UDR0 = data;
}

/*
 * Receive one byte through the USART
 */
unsigned char command_receive_byte(void)
{
    while (!(UCSR0A & (1<<RXC)));
    return UDR0;
}

/*
 * Interrupt service routine for Data received from UART
 * Receives one byte of data from the USART. Determines
 * command char received and execute code as required.
 */
SIGNAL(SIG_UART0_RECV)
{
	unsigned char userCommand = command_receive_byte();
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
				command_process_packet();
				command_cntr = 0;
			}
		}
		else 
			command_checking_DLE_stuffing = FALSE;
	}
}


