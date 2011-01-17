//***************************************************************************
//
//  File........: UART_Math.c
//
//  Author(s)...: Johnny McClymont
//
//  Target(s)...: ATmega128
//
//  Compiler....: AVR-GCC 3.3.1; avr-libc 1.0
//
//  Description.: ATMega128 Rhino Driver Module for MARVIN
//
//  Revisions...: 1.0
//
//  YYYYMMDD - VER. - COMMENT                                       - SIGN.
//
//  20070822 - 1.0  - Created                                       - J.McClymont
//
//***************************************************************************

#include "main.h"
#include "usart.h"
#include "Command_Layer.h"





/****************************************************************************************/
/*		Function unsigned char convert_ASCII_to_HEX(int ASCII_Char)		*/
/*											*/
/*	Takes ASCII char and converts to Hexidecimal.  Works on low nibble only, 	*/
/*	nibble must be set to 0.  Allows two ASCII chars received by UART to be 	*/
/*	converted to single hexidecimal byte.	Only works on ASCII chars 0-9		*/
/*	and a-f (note case).								*/
/****************************************************************************************/

unsigned char convert_ASCII_to_HEX(int ASCII_Char){
	
	// TODO: simply return ASCII_Char - 48 if it is in the right range
	unsigned char HEX_Char;
	switch(ASCII_Char){
		case 48:	//'0' 
		HEX_Char = 0x00;
		break;
		
		case 49:	//'1' 
		HEX_Char = 0x01;
		break;
		
		case 50:	//'2'
		HEX_Char = 0x02; 									
		break;
				
		case 51:	//'3'
		HEX_Char = 0x03; 									
		break;
		
		case 52:	//'4' 
		HEX_Char = 0x04;									
		break;
		
		case 53:	//'5'
		HEX_Char = 0x05; 									
		break;
		
		case 54:	//'6' 
		HEX_Char = 0x06;
		break;
		
		case 55:	//'7'
		HEX_Char = 0x07; 
		break;
		
		case 56:	//'8'
		HEX_Char = 0x08;								
		break;
				
		case 57:	//'9'
		HEX_Char = 0x09; 									
		break;
		
		case 97:	//'a' 
		HEX_Char = 0x0A;
		break;
		
		case 98:	//'b'
		HEX_Char = 0x0B; 
		break;
		
		case 99:	//'c' 
		HEX_Char = 0x0C;									
		break;
				
		case 100:	//'d'
		HEX_Char = 0x0D; 									
		break;
		
		case 101:	//'e'
		HEX_Char = 0x0E; 									
		break;
		
		case 102:	//'f' 
		HEX_Char = 0x0F;									
		break;
		
		case 65:	//'A' 
		HEX_Char = 0x0A;
		break;
		
		case 66:	//'B'
		HEX_Char = 0x0B; 
		break;
		
		case 67:	//'C' 
		HEX_Char = 0x0C;									
		break;
				
		case 68:	//'D'
		HEX_Char = 0x0D; 									
		break;
		
		case 69:	//'E'
		HEX_Char = 0x0E; 									
		break;
		
		case 70:	//'F' 
		HEX_Char = 0x0F;									
		break;
		
		default: 
		HEX_Char = 0x00;	//Default Hex value = 8
						
		}
	return HEX_Char;	
}	

/****************************************************************/
/*		Function char hexToAscii(char first)		*/
/*								*/
/*	Takes hexidecimal value and converts it to ascii	*/
/*	equivalent for outputing to UART.  'first' variable 	*/
/*	have high nibble set to 0.  low nibble must contain	*/
/*	number 0-F.						*/
/****************************************************************/

char hexToAscii(char first)
{
	unsigned char return_Char;
	switch(first){
		case 0x00:	//'0' 
		return_Char = 48;
		break;
		
		case 0x01:	//'1' 
		return_Char = 49;
		break;
		
		case 0x02:	//'2'
		return_Char = 50; 									
		break;
				
		case 0x03:	//'3'
		return_Char = 51; 									
		break;
		
		case 0x04:	//'4' 
		return_Char = 52;									
		break;
		
		case 0x05:	//'5'
		return_Char = 53; 									
		break;
		
		case 0x06:	//'6' 
		return_Char = 54;
		break;
		
		case 0x07:	//'7'
		return_Char = 55; 
		break;
		
		case 0x08:	//'8'
		return_Char = 56;								
		break;
				
		case 0x09:	//'9'
		return_Char = 57; 									
		break;
		
		case 0x0A:	//'A' 
		return_Char = 65;
		break;
		
		case 0x0B:	//'B'
		return_Char = 66; 
		break;
		
		case 0x0C:	//'C' 
		return_Char = 67;									
		break;
				
		case 0x0D:	//'D'
		return_Char = 68; 									
		break;
		
		case 0x0E:	//'E'
		return_Char = 69; 									
		break;
		
		case 0x0F:	//'F' 
		return_Char = 70;									
		break;
		
		default: 
		return_Char = 56;  //Default character '8'
						
		}
	return return_Char;	
}	

unsigned char sendDecimal(int number, unsigned char places, unsigned char my_ptr)
{
	unsigned char thousands = 0;
	unsigned char hundreds = 0;
	unsigned char tens = 0;
	unsigned char ones = 0;
	
	while (number >= 0)
	{
		number = number - 1000;
		thousands++;
	}
	thousands--;
	number = number + 1000;
	
	if (places == 0x04)
	{
		reply_Packet[my_ptr++] = hexToAscii(thousands);
		places--;
	}
	
	while (number >= 0)
	{
		number -= 100;
		hundreds++;
	}
	hundreds--;
	number += 100;
	if (places == 3)
	{
		reply_Packet[my_ptr++] = hexToAscii(hundreds);
		places--;
	}
	
	while (number >= 0)
	{
		number -= 10;
		tens++;
	}
	tens--;
	number += 10;
	if (places == 2)
	{
		reply_Packet[my_ptr++] = hexToAscii(tens);
		places--;
	}
	
	while (number >= 0)
	{
		number -= 1;
		ones++;
	}
	ones--;
	number += 1;
	if (places == 1)
	{
		reply_Packet[my_ptr++] = hexToAscii(ones);
		places--;
	}
	return my_ptr;
	
}

int Dec2Hex(unsigned char MSB, unsigned char LSB)
{
	int retval = 0;
	char thousands = (MSB>>4)&0x0F;
	for (int i = 0; i< thousands; i++)
	{
		retval += 1000;
	}
	char hundreds = MSB&0x0F;
	for (int i = 0; i< hundreds; i++)
	{
		retval += 100;
	}
	char tens = (LSB>>4)&0x0F;
	for (int i = 0; i< tens; i++)
	{
		retval += 10;
	}
	char ones = LSB&0x0F;
	for (int i = 0; i< ones; i++)
	{
		retval += 1;
	}
	return retval;
}

