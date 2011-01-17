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
	// Set baud rate
	unsigned int baudrate = 207;
	//UBRR1H = (unsigned char)(baudrate>>8);
	//UBRR1L = (unsigned char)baudrate;

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
	
	// Initialise the clocks to zero
	UTCtime_lastPulse.seconds = 0;
	UTCtime_lastPulse.minutes = 0;
	UTCtime_lastPulse.hours = 0;
	UTCtime_lastPulse.day =0;
	UTCtime_lastPulse.month = 0;
	UTCtime_lastPulse.year = 0;
	UTCtime_endOfFrame.seconds = 0;
	UTCtime_endOfFrame.minutes = 0;
	UTCtime_endOfFrame.hours = 0;
	UTCtime_endOfFrame.day = 0;
	UTCtime_endOfFrame.month = 0;
	UTCtime_endOfFrame.year = 0;
	
	endOfFrame_pulse = 0;	//set to 0 to indicate end of frame pulse not received
	GPS_state = SYNCING;	//set GPS state to syncing, as we need to sync with incoming packets
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
	for (int i = 0; i< packet_cntr; i++)
	{
		Error_Packet[i+2] = GPS_Packet[i];
	}
	Error_Packet[0] = packet_cntr;
	Error_Packet[1] = code;
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
	packet_proccessed = 0;
	unsigned char packet_ptr = packet_cntr;
	for (int i = 0; i< packet_ptr; i++)
	{
		Last_Packet[i+1] = GPS_Packet[i];
	}
	Last_Packet[0] = packet_ptr;
	
	switch (GPS_state)
	{
		case SETUP_GPS:
			if ((GPS_Packet[0] == 0x8E) & (GPS_Packet[1] == 0xA5))  //check that a valid reply to our SETUP packet was received
				GPS_state = CHECK_GPS_TIME_VALID;	//change state to check time from GPS is valid
			else
				gps_send_trimble_init();	//else resend SETUP packet
		break;
		
		case CHECK_GPS_TIME_VALID:
			switch (GPS_Packet[0])		//get packet ID
			{
				case 0x8F:		//if packet ID is an extended packet ID
					switch(GPS_Packet[1])		//get thesubcode of the extended packet ID
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
								if (GPS_Packet[10] == 0x03)		//check to ensure time is good
								{	
									GPS_state = GPS_TIME_GOOD;  //if time good then change state to run main application
									UTCtime_lastPulse.seconds = GPS_Packet[11];
									UTCtime_lastPulse.minutes = GPS_Packet[12];
									UTCtime_lastPulse.hours = GPS_Packet[13];
									UTCtime_lastPulse.day = GPS_Packet[14];
									UTCtime_lastPulse.month = GPS_Packet[15];
									UTCtime_lastPulse.year = ((GPS_Packet[16]<<8)&0xFF00) | (GPS_Packet[17]& 0x00FF);

									if(wait_4_timestamp == 1)
										wait_4_timestamp = 0;
								}
							}	
						break;
					}
				break;
			}
		break;
		
			
		break;
		
		case GPS_TIME_GOOD:
			switch (GPS_Packet[0])	//get packet ID
			{
				case 0x41:
			
				break;
		
				case 0x8F:	//if packet ID is an extended packet ID
					switch(GPS_Packet[1])		//get thesubcode of the extended packet ID
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
								if (GPS_Packet[10] != 0x03)	//check to ensure time is still good
								{
									error_state |= GPS_TIME_NOT_LOCKED;
									store_error_packet(GPS_TIME_NOT_LOCKED);
									error_state = error_state & 0xFE;
								}
								
								//store the UTC bytes to UTC clock
								UTCtime_lastPulse.seconds = GPS_Packet[11];
								UTCtime_lastPulse.minutes = GPS_Packet[12];
								UTCtime_lastPulse.hours = GPS_Packet[13];
								UTCtime_lastPulse.day = GPS_Packet[14];
								UTCtime_lastPulse.month = GPS_Packet[15];
								UTCtime_lastPulse.year = ((GPS_Packet[16]<<8)&0xFF00) | (GPS_Packet[17]& 0x00FF);

								//if this is an EOF time stamp and CCD pulse wasn't set halfway thru processing packet
								if ((endOfFrame_pulse == 1) && (nextPacketisEOF == 0))
								{
									//store the UTC bytes to EOF clock
									UTCtime_endOfFrame.seconds = UTCtime_lastPulse.seconds;
									UTCtime_endOfFrame.minutes = UTCtime_lastPulse.minutes;
									UTCtime_endOfFrame.hours = UTCtime_lastPulse.hours;
									UTCtime_endOfFrame.day = UTCtime_lastPulse.day;
									UTCtime_endOfFrame.month = UTCtime_lastPulse.month;
									UTCtime_endOfFrame.year = UTCtime_lastPulse.year;
									if(wait_4_EOFtimestamp == 1)
									{
										wait_4_EOFtimestamp = 0;
									}	
									endOfFrame_pulse = 0;
								}
								if (wait_4_ten_second_boundary == 1)  //if we are waiting for a 10 second boundary
								{
									switch(UTCtime_lastPulse.seconds)		//check latest time stamp from GPS
									{
										case 0:		//if seconds count is 9,19,29,39,49,59 then next pulse will
										case 10:	//be on the ten second boundary.
										case 20:
										case 30:
										case 40:
										case 50:
											wait_4_ten_second_boundary = 0;
										break;
										
										default:
											wait_4_ten_second_boundary = 1;
										break;
									}
								}
								
								if(wait_4_timestamp == 1)
								{
									wait_4_timestamp = 0;
								}	
							}	
						break;
					}
				break;
			
			}
		break;
	}
	if (nextPacketisEOF == 1) nextPacketisEOF =0;	//check to see if end of frame pulse came while processing packet
	packet_proccessed = 1;
}

/* 
 * Interrupt service routine for Data received from UART
 * Receives one byte of data from the USART. Determines
 * command char received and execute code as required.
 */
SIGNAL(SIG_UART1_RECV)
{
	check_GPS_present = 0;
	incomingbyte = gps_receive_byte();
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
					gps_process_trimble_packet();		//process this packet
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
					gps_send_trimble_init();		//send SETUP packet to switch off most automatic packets
					GPS_state = CHECK_GPS_TIME_VALID;	//change state to check time from GPS is valid
				}
				sync_state = LOOK_FOR_DLE;
			break;
			
			default:
				sync_state = LOOK_FOR_DLE;
			break;
		}
	}
}