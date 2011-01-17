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
	
	gps_checking_DLE_stuffing = FALSE;
	gps_usart_state = SYNCING_PACKETS;
	gps_sync_state = LOOK_FOR_DLE;
	
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
 * Store a packet that caused an error for later retrieval
 */
void store_error_packet(unsigned char code)
{
	for (int i = 0; i< gps_packet_cntr; i++)
	{
		gps_trimble_error_packet[i+2] = gps_trimble_packet[i];
	}
	gps_trimble_error_packet[0] = gps_packet_cntr;
	gps_trimble_error_packet[1] = code;
}

/*
 * Send a packet mask to the Trimble unit to supress automatic packets
 */
void gps_send_trimble_init(void)
{
	unsigned char thisPacket[7] = {0x10, 0x8E, 0xA5, 0x01, 0x00, 0x10, 0x03};
	for (unsigned char i = 0; i < 7; i++)
		gps_transmit_byte(thisPacket[i]);
}

/*
 * Process a packet from the Trimble GPS
 */
void gps_process_trimble_packet(void)
{
	gps_processing_packet = TRUE;
	unsigned char packet_ptr = gps_packet_cntr;
	for (int i = 0; i< packet_ptr; i++)
	{
		gps_last_trimble_packet[i+1] = gps_trimble_packet[i];
	}
	gps_last_trimble_packet[0] = packet_ptr;
	
	switch (gps_state)
	{
		case SETUP_GPS:
			if ((gps_trimble_packet[0] == 0x8E) && (gps_trimble_packet[1] == 0xA5))  //check that a valid reply to our SETUP packet was received
				gps_state = CHECK_GPS_TIME_VALID;	//change state to check time from GPS is valid
			else
				gps_send_trimble_init();	//else resend SETUP packet
		break;
		
		case CHECK_GPS_TIME_VALID:
			switch (gps_trimble_packet[0])		//get packet ID
			{
				case 0x8F:		//if packet ID is an extended packet ID
					switch(gps_trimble_packet[1])		//get thesubcode of the extended packet ID
					{ 
						case 0xAB:		//Primary Timing Packet
							if (packet_ptr != 18)	//make sure we have a correct amount of bytes
							{
								error_state |= PACKET_8FAB_LENGTH_WRONG;
								store_error_packet(PACKET_8FAB_LENGTH_WRONG);
								error_state = error_state & 0xFE;
							}
							if (packet_ptr >= 10)
							{
								if (gps_trimble_packet[10] == 0x03)		//check to ensure time is good
								{	
									gps_state = GPS_TIME_GOOD;  //if time good then change state to run main application
									gps_last_timestamp.seconds = gps_trimble_packet[11];
									gps_last_timestamp.minutes = gps_trimble_packet[12];
									gps_last_timestamp.hours = gps_trimble_packet[13];
									gps_last_timestamp.day = gps_trimble_packet[14];
									gps_last_timestamp.month = gps_trimble_packet[15];
									gps_last_timestamp.year = ((gps_trimble_packet[16]<<8)&0xFF00) | (gps_trimble_packet[17]& 0x00FF);

									if(wait_4_timestamp)
										wait_4_timestamp = FALSE;
								}
							}	
						break;
					}
				break;
			}
		break;

		case GPS_TIME_GOOD:
			switch (gps_trimble_packet[0])	//get packet ID
			{
				case 0x41:
			
				break;
		
				case 0x8F:	//if packet ID is an extended packet ID
					switch(gps_trimble_packet[1])		//get thesubcode of the extended packet ID
					{ 
						case 0xAB:		//Primary Timing Packet
							if (packet_ptr != 18)	//make sure we have a correct amount of bytes
							{
								error_state |= PACKET_8FAB_LENGTH_WRONG;
								store_error_packet(PACKET_8FAB_LENGTH_WRONG);
								error_state = error_state & 0xFE;
							}
							if (packet_ptr >= 10)
							{	
								if (gps_trimble_packet[10] != 0x03)	//check to ensure time is still good
								{
									error_state |= GPS_TIME_NOT_LOCKED;
									store_error_packet(GPS_TIME_NOT_LOCKED);
									error_state = error_state & 0xFE;
								}
								
								// Store the UTC bytes to UTC clock
								gps_last_timestamp.seconds = gps_trimble_packet[11];
								gps_last_timestamp.minutes = gps_trimble_packet[12];
								gps_last_timestamp.hours = gps_trimble_packet[13];
								gps_last_timestamp.day = gps_trimble_packet[14];
								gps_last_timestamp.month = gps_trimble_packet[15];
								gps_last_timestamp.year = ((gps_trimble_packet[16]<<8)&0xFF00) | (gps_trimble_packet[17]& 0x00FF);

								// If this is a synctime and CCD pulse wasn't set halfway thru processing packet
								if (gps_record_synctime && !nextPacketisEOF)
								{
									// Store the UTC bytes to EOF clock
									gps_last_synctime.seconds = gps_last_timestamp.seconds;
									gps_last_synctime.minutes = gps_last_timestamp.minutes;
									gps_last_synctime.hours = gps_last_timestamp.hours;
									gps_last_synctime.day = gps_last_timestamp.day;
									gps_last_synctime.month = gps_last_timestamp.month;
									gps_last_synctime.year = gps_last_timestamp.year;
									if(!synctime_ready)
										synctime_ready = TRUE;
									
									gps_record_synctime = FALSE;
								}
								
								if (wait_4_ten_second_boundary)  //if we are waiting for a 10 second boundary
								{
									switch(gps_last_timestamp.seconds)		//check latest time stamp from GPS
									{
										case 0:		//if seconds count is 9,19,29,39,49,59 then next pulse will
										case 10:	//be on the ten second boundary.
										case 20:
										case 30:
										case 40:
										case 50:
											wait_4_ten_second_boundary = FALSE;
										break;
										
										default:
											wait_4_ten_second_boundary = TRUE;
										break;
									}
								}
								
								if(wait_4_timestamp)
									wait_4_timestamp = FALSE;
							}	
						break;
					}
				break;
			
			}
		break;
	}
	if (nextPacketisEOF)
		nextPacketisEOF = FALSE;	//check to see if end of frame pulse came while processing packet

	gps_processing_packet = FALSE;
}

void gps_timeout(void)
{
	gps_usart_state = SYNCING_PACKETS;
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
	gps_timeout_count = 0;
	unsigned char incomingbyte = gps_receive_byte();
	if (gps_usart_state == CAPTURE_PACKETS)
	{
		if ((incomingbyte == 0x10) && !gps_received_startbit)	//check if received byte is physical layer packet start byte
			gps_received_startbit = TRUE;
		else if (gps_received_startbit) //if already processing packet continue to scan for packet end byte
		{	
			gps_trimble_packet[gps_packet_cntr++] = incomingbyte;		//save received byte to packet buffer
			if (incomingbyte == 0x10)  
			{
				if (gps_checking_DLE_stuffing)
				{
					gps_checking_DLE_stuffing = FALSE;
					gps_packet_cntr--;		//write over 2nd DLE byte in gps_trimble_packet
				}
				else
					gps_checking_DLE_stuffing = TRUE;
			}
			else if (incomingbyte == 0x03)
			{
				if (gps_checking_DLE_stuffing)  //this is true if the last =byte received was an odd numbered DLE
				{
					gps_packet_cntr--;		//remove the ETX byte
					gps_packet_cntr--;		//remove the DLE byte
					gps_received_startbit = FALSE;
					gps_process_trimble_packet();		//process this packet
					gps_packet_cntr = 0;
				}
			}
			else
				gps_checking_DLE_stuffing = FALSE;
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
	else if (gps_usart_state == SYNCING_PACKETS) //if we are trying to sync to GPS packets
	{
		gps_state = SYNCING;
		switch(gps_sync_state)
		{
			case LOOK_FOR_DLE:
				if (incomingbyte == 0x10)
					gps_sync_state = LOOK_FOR_ETX;
			break;
			
			case LOOK_FOR_ETX:
				if (incomingbyte == 0x03)
					gps_sync_state = LOOK_FOR_2ND_DLE;
				else
					gps_sync_state = LOOK_FOR_DLE;
			break;
			
			case LOOK_FOR_2ND_DLE:
				if (incomingbyte == 0x10)
					gps_sync_state = LOOK_FOR_NO_DLE;
				else
					gps_sync_state = LOOK_FOR_DLE;
			break;
			
			case LOOK_FOR_NO_DLE:
				if (incomingbyte != 0x10) 
				{
					gps_trimble_packet[gps_packet_cntr++] = incomingbyte; //save the first byte of this new packet to the buffer
					gps_received_startbit = TRUE;      //set flag to indicate we are recording a valid packet
					gps_usart_state = CAPTURE_PACKETS; //change state as we are now synced to packets
					gps_send_trimble_init();           //send SETUP packet to switch off most automatic packets
					gps_state = CHECK_GPS_TIME_VALID;  //change state to check time from GPS is valid
				}
				gps_sync_state = LOOK_FOR_DLE;
			break;
			
			default:
				gps_sync_state = LOOK_FOR_DLE;
			break;
		}
	}
}