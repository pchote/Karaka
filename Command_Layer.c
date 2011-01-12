//***************************************************************************
//
//  File........: COMMAND_LAYER.c
//
//  Author(s)...: Johnny McClymont
//
//  Target(s)...: ATmega128
//
//  Compiler....: AVR-GCC 3.3.1; avr-libc 1.0
//
//  Description.: GPS routines
//
//  Revisions...: 1.0
//
//  YYYYMMDD - VER. - COMMENT                                       - SIGN.
//
//  20090606 - 1.0  - Created                                       - J.McClymont
//
//***************************************************************************


#include <avr/io.h>
#include "usart1.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "main.h"
#include "Command_Layer.h"
#include "usart.h"
#include "UART_Math.h"
#include "GPS.h"

	
/*****************************************************************************
*
*   Function name : Command_Init
*
*   Returns :       None
*
*   Parameters :    none
*
*   Purpose :       Initialize the GPS
*
*****************************************************************************/
void Command_Init(void)
{	
	USART_Init(16);		// set baudrate at 115.2 kbs
	error_state = NO_ERROR;
}


/*****************************************************************************
*
*   Function name : Command_processPacket
*
*   Returns :       none
*
*   Parameters :    none
*
*   Purpose :       Parse Command packet
*					
*
*****************************************************************************/
void Command_processPacket(void)
{
	reply_cntr = 0;
	reply_Packet[reply_cntr++] = command_Packet[0];  //put same packet ID in reply packet
	stored_error_state = error_state;
	error_state = NO_ERROR;
	reply_Packet[reply_cntr++] = stored_error_state;  //put error code into reply packet
	switch (command_Packet[0])	//get packet ID
	{
		case DUMMY:
				
		break;
		
		case GET_STATUS:		//
			reply_Packet[reply_cntr++] = hexToAscii((status_register>>4)&0x0f);
			reply_Packet[reply_cntr++] = hexToAscii(status_register&0x0f);
		break;
		
		case SET_CONTROL:
			control_register = command_Packet[2];
			
		case GET_CONTROL:		//
			reply_Packet[reply_cntr++] = hexToAscii((control_register>>4)&0x0f);
			reply_Packet[reply_cntr++] = hexToAscii(control_register&0x0f);
		break;
		
		case GET_UTCTIME:		//  
			if (wait_4_timestamp == 0)
			{	
				reply_cntr = sendDecimal(UTCtime_lastPulse.year, 4, reply_cntr);
				reply_Packet[reply_cntr++] = ':';
				reply_cntr = sendDecimal(UTCtime_lastPulse.month, 2, reply_cntr);
				reply_Packet[reply_cntr++] = ':';
				reply_cntr = sendDecimal(UTCtime_lastPulse.day, 2, reply_cntr);
				reply_Packet[reply_cntr++] = ':';
				reply_cntr = sendDecimal(UTCtime_lastPulse.hours,2, reply_cntr);
				reply_Packet[reply_cntr++] = ':';
				reply_cntr = sendDecimal(UTCtime_lastPulse.minutes,2, reply_cntr);
				reply_Packet[reply_cntr++] = ':';
				reply_cntr = sendDecimal(UTCtime_lastPulse.seconds,2, reply_cntr);
				reply_Packet[reply_cntr++] = ':';
				reply_cntr = sendDecimal(milliseconds,3, reply_cntr);
			}
			else
			{
				stored_error_state |= UTC_ACCESS_ON_UPDATE;
				stored_error_state = stored_error_state & 0xFE;
			}
		break;
		
		case SET_CCD_EXPOSURE:
			cli();
			Pulse_Counter = 0;
			for (int i = 0; i< convert_ASCII_to_HEX(command_Packet[2]); i++)
			{
				Pulse_Counter += 1000;
			}
	
			for (int i = 0; i< convert_ASCII_to_HEX(command_Packet[3]); i++)
			{
				Pulse_Counter += 100;
			}
		
			for (int i = 0; i< convert_ASCII_to_HEX(command_Packet[4]); i++)
			{
				Pulse_Counter += 10;
			}
	
			for (int i = 0; i< convert_ASCII_to_HEX(command_Packet[5]); i++)
			{
				Pulse_Counter += 1;
			}
			Current_Count = 0;
			wait_4_ten_second_boundary = 1;
			sei();
		
		case GET_CCD_EXPOSURE:		//
			reply_cntr = sendDecimal(Pulse_Counter,4, reply_cntr);
		break;
		
		case GET_EOFTIME:		//
			if (wait_4_EOFtimestamp == 0)
			{
				reply_cntr = sendDecimal(UTCtime_endOfFrame.year, 4, reply_cntr);
				reply_Packet[reply_cntr++] = ':';
				reply_cntr = sendDecimal(UTCtime_endOfFrame.month, 2, reply_cntr);
				reply_Packet[reply_cntr++] = ':';
				reply_cntr = sendDecimal(UTCtime_endOfFrame.day, 2, reply_cntr);
				reply_Packet[reply_cntr++] = ':';
				reply_cntr = sendDecimal(UTCtime_endOfFrame.hours,2, reply_cntr);
				reply_Packet[reply_cntr++] = ':';
				reply_cntr = sendDecimal(UTCtime_endOfFrame.minutes,2, reply_cntr);
				reply_Packet[reply_cntr++] = ':';
				reply_cntr = sendDecimal(UTCtime_endOfFrame.seconds,2, reply_cntr);
				reply_Packet[reply_cntr++] = ':';
				reply_cntr = sendDecimal(0,3, reply_cntr);
			}
			else
			{
				stored_error_state |= EOF_ACCESS_ON_UPDATE;
				stored_error_state = stored_error_state & 0xFE;
			}
		break;
		
		case GET_LAST_PACKET:		//
			size = Last_Packet[0];
			for (int i = 1; i < size+1; i++)
			{
				reply_Packet[reply_cntr++] = hexToAscii((Last_Packet[i]>>4)&0x0f);
				reply_Packet[reply_cntr++] = hexToAscii((Last_Packet[i])&0x0f);
			}
		break;
		
		case GET_ERROR_PACKET:		//
			size = Error_Packet[0];
			reply_Packet[reply_cntr++]  = '[';
			reply_Packet[reply_cntr++]  = hexToAscii((Error_Packet[1]>>4)&0x0f);
			reply_Packet[reply_cntr++]  = hexToAscii((Error_Packet[1])&0x0f);
			reply_Packet[reply_cntr++]  = ']';
			for (int i = 2; i < size+2; i++)
			{
				reply_Packet[reply_cntr++]  = hexToAscii((Error_Packet[i]>>4)&0x0f);
				reply_Packet[reply_cntr++]  = hexToAscii((Error_Packet[i])&0x0f);
			}
		break;
		
		default:
			stored_error_state |= PACKETID_INVALID;
			stored_error_state = stored_error_state & 0xFE;
		break;
	}
	reply_Packet[1] = stored_error_state;
	Command_sendPacket();
}



/*****************************************************************************
*
*   Function name : Command_sendPacket()
*
*   Returns :       none
*
*   Parameters :    none
*
*   Purpose :       Send packet to UI software
*					
*
*****************************************************************************/
void Command_sendPacket(void)
{
	Usart_Tx(0x10);
	for (int i = 0; i<reply_cntr; i++)
	{
		Usart_Tx(reply_Packet[i]);
		if (reply_Packet[i] == 0x10)
		{
			Usart_Tx(0x10);
		}
	}
	Usart_Tx(0x10);
	Usart_Tx(0x03);
}




