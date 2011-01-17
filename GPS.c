//***************************************************************************
//
//  File........: GPS.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Trimble GPS listener
//
//***************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "usart1.h"
#include "main.h"
#include "display.h"
#include "GPS.h"
#include "UART_Math.h"
#include "command.h"
	
/*
 * Initialise the Trimble gps listener
 */
void GPS_Init(void)
{	
	USART1_Init(207); 	// set baudrate to 9600
	
	//initialise the clocks to zero
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
 * Process a packet from the Trimble GPS
 */
void GPS_processPacket(void)
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
				GPS_sendPacketMask();	//else resend SETUP packet
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
void GPS_sendPacketMask(void)
{
	unsigned char thisPacket[7] = {0x10, 0x8E, 0xA5, 0x01, 0x00, 0x10, 0x03};
	for (int i = 0; i<7; i++)
	{
		Usart1_Tx(thisPacket[i]);
	}
}
