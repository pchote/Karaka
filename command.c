//***************************************************************************
//
//  File........: command.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Responds to user commands over usb
//
//***************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "command.h"
#include "usart1.h"
#include "main.h"
#include "GPS.h"

	
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
	
	checking_DLE_stuffing_flag = 0;
	startBit_Rcvd = 0;
	command_cntr = 0;
}

/*
 * Process a command packet
 */
void command_process_packet(void)
{
	reply_cntr = 0;
	reply_Packet[reply_cntr++] = command_Packet[0];  //put same packet ID in reply packet
	stored_error_state = error_state;
	error_state = NO_ERROR;
	reply_Packet[reply_cntr++] = stored_error_state;  //put error code into reply packet
	switch (command_Packet[0])	//get packet ID
	{
		case ECHO:
		break;
		
		case GET_STATUS:
			reply_Packet[reply_cntr++] = nibble_to_ascii((status_register>>4)&0x0f);
			reply_Packet[reply_cntr++] = nibble_to_ascii(status_register&0x0f);
		break;
		
		case SET_CONTROL:
			control_register = command_Packet[2];
			
		case GET_CONTROL:
			reply_Packet[reply_cntr++] = nibble_to_ascii((control_register>>4)&0x0f);
			reply_Packet[reply_cntr++] = nibble_to_ascii(control_register&0x0f);
		break;
		
		case GET_UTCTIME:
			if (wait_4_timestamp == 0)
			{	
				sendDecimal(UTCtime_lastPulse.year, 4);
				reply_Packet[reply_cntr++] = ':';
				sendDecimal(UTCtime_lastPulse.month, 2);
				reply_Packet[reply_cntr++] = ':';
				sendDecimal(UTCtime_lastPulse.day, 2);
				reply_Packet[reply_cntr++] = ':';
				sendDecimal(UTCtime_lastPulse.hours, 2);
				reply_Packet[reply_cntr++] = ':';
				sendDecimal(UTCtime_lastPulse.minutes, 2);
				reply_Packet[reply_cntr++] = ':';
				sendDecimal(UTCtime_lastPulse.seconds, 2);
				reply_Packet[reply_cntr++] = ':';
				sendDecimal(milliseconds, 3);
			}
			else
			{
				stored_error_state |= UTC_ACCESS_ON_UPDATE;
				stored_error_state = stored_error_state & 0xFE;
			}
		break;
		
		case SET_CCD_EXPOSURE:
			cli();
			Pulse_Counter = ascii_to_nibble(command_Packet[2])*1000
							+ ascii_to_nibble(command_Packet[3])*100
							+ ascii_to_nibble(command_Packet[4])*10
							+ ascii_to_nibble(command_Packet[5]);
			
			Current_Count = 0;
			wait_4_ten_second_boundary = 1;
			sei();
		
		case GET_CCD_EXPOSURE:
			sendDecimal(Pulse_Counter,4);
		break;
		
		case GET_EOFTIME:
			if (wait_4_EOFtimestamp == 0)
			{
				sendDecimal(UTCtime_endOfFrame.year, 4);
				reply_Packet[reply_cntr++] = ':';
				sendDecimal(UTCtime_endOfFrame.month, 2);
				reply_Packet[reply_cntr++] = ':';
				sendDecimal(UTCtime_endOfFrame.day, 2);
				reply_Packet[reply_cntr++] = ':';
				sendDecimal(UTCtime_endOfFrame.hours, 2);
				reply_Packet[reply_cntr++] = ':';
				sendDecimal(UTCtime_endOfFrame.minutes, 2);
				reply_Packet[reply_cntr++] = ':';
				sendDecimal(UTCtime_endOfFrame.seconds, 2);
				reply_Packet[reply_cntr++] = ':';
				sendDecimal(0,3);
			}
			else
			{
				stored_error_state |= EOF_ACCESS_ON_UPDATE;
				stored_error_state = stored_error_state & 0xFE;
			}
		break;
		
		case GET_LAST_PACKET:
			size = Last_Packet[0];
			for (int i = 1; i < size+1; i++)
			{
				reply_Packet[reply_cntr++] = nibble_to_ascii((Last_Packet[i]>>4)&0x0f);
				reply_Packet[reply_cntr++] = nibble_to_ascii((Last_Packet[i])&0x0f);
			}
		break;
		
		case GET_ERROR_PACKET:
			size = Error_Packet[0];
			reply_Packet[reply_cntr++]  = '[';
			reply_Packet[reply_cntr++]  = nibble_to_ascii((Error_Packet[1]>>4)&0x0f);
			reply_Packet[reply_cntr++]  = nibble_to_ascii((Error_Packet[1])&0x0f);
			reply_Packet[reply_cntr++]  = ']';
			for (int i = 2; i < size+2; i++)
			{
				reply_Packet[reply_cntr++]  = nibble_to_ascii((Error_Packet[i]>>4)&0x0f);
				reply_Packet[reply_cntr++]  = nibble_to_ascii((Error_Packet[i])&0x0f);
			}
		break;
		
		default:
			stored_error_state |= PACKETID_INVALID;
			stored_error_state = stored_error_state & 0xFE;
		break;
	}
	reply_Packet[1] = stored_error_state;
	command_send_packet();
}

/*
 * Respond to a command packet using buffered data
 */
void command_send_packet(void)
{
	Usart_Tx(0x10);
	for (unsigned char i = 0; i < reply_cntr; i++)
	{
		Usart_Tx(reply_Packet[i]);
		if (reply_Packet[i] == 0x10)
			Usart_Tx(0x10);
	}
	Usart_Tx(0x10);
	Usart_Tx(0x03);
}

void sendDecimal(int number, unsigned char places)
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
		reply_Packet[reply_cntr++] = nibble_to_ascii(p);
	}
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
unsigned char Usart_Rx(void)
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
				command_process_packet();
				command_cntr = 0;
			}
		}
		else 
		{
			checking_DLE_stuffing_flag = 0;
		}	
	}
}


