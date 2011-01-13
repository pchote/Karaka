//***************************************************************************
//
//  File........: LCD_LIB.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: LCD routines
//
//***************************************************************************

#include "LCD_LIB.h"
#include "main.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <inttypes.h>
#include "UART_Math.h"
#include "usart.h"

/*
 * Initialise the LCD
 */
void LCD_init(void)
{
	Delay(50000);
	LCD_WriteControl(0x38, 500);
	LCD_WriteControl(CLEARLCD, 500);
	LCD_WriteControl(CURSORON, 500);
	
	LCD_WriteControl(CURSORHOME, 500);
	
	LCD_WriteControl(0x06, 500);
	
	Delay(500);
	LCD_sendmsg(PSTR("GPS KARAKA UNIT"));
	LCD_WriteControl(NEWLINE,500);
	LCD_sendmsg(PSTR("Kia Ora"));
	Delay(65000);
	Delay(65000);
	cursor_ptr = 0;
	putHeader = 0;
}

/*
 * Set the message display mode for the LCD 
 */
void update_LCD(unsigned char LCD_state)
{	
	switch (LCD_state)
	{
		case SYNCING:
			if (putHeader == 0)
			{
				LCD_WriteControl(CURSORHOME, 50);
				LCD_WriteControl(CLEARLCD, 500);
				LCD_sendmsg(PSTR("SYNCING TO GPS"));
				sendmsg(PSTR("SYNCING TO GPS>"));
				LCD_WriteControl(NEWLINE,50);
				putHeader = 1;
			}
			
			LCD_WriteData('.');
			if (cursor_ptr++ == 15)
			{
				cursor_ptr = 0;
				LCD_WriteControl(NEWLINE,50);
				LCD_sendmsg(PSTR("                "));
				LCD_WriteControl(NEWLINE,50);
			}
		break;
		
		case SETUP_GPS:
			if (putHeader == 0)
			{
				LCD_WriteControl(CURSORHOME, 50);
				LCD_WriteControl(CLEARLCD, 500);
				LCD_sendmsg(PSTR("SETTING UP GPS"));
				LCD_WriteControl(NEWLINE,50);
				putHeader = 1;
			}
			
			LCD_WriteData('.');
			
			if (cursor_ptr++ == 15)
			{
				cursor_ptr = 0;
				LCD_WriteControl(NEWLINE,50);
				LCD_sendmsg(PSTR("                "));
				LCD_WriteControl(NEWLINE,50);
			}
		break;
		
		case CHECK_GPS_TIME_VALID:
			if (putHeader == 0)
			{
				LCD_WriteControl(CURSORHOME, 50);
				LCD_WriteControl(CLEARLCD, 500);
				LCD_sendmsg(PSTR("CHECK GPS LOCK"));
				sendmsg(PSTR("CHECKING GPS TIME>"));
				LCD_WriteControl(NEWLINE,50);
				putHeader = 1;
			}
			
			LCD_WriteData('.');
			
			if (cursor_ptr++ == 15)
			{
				cursor_ptr = 0;
				LCD_WriteControl(NEWLINE,50);
				LCD_sendmsg(PSTR("                "));
				LCD_WriteControl(NEWLINE,50);
			}
		break;
		
		case GPS_TIME_GOOD:
			if (putHeader == 0)
			{
				LCD_WriteControl(CURSORHOME, 50);
				LCD_WriteControl(CLEARLCD, 500);
				LCD_sendmsg(PSTR("UTC TIME"));
				sendmsg(PSTR("GPS TIME GOOD>"));
				LCD_WriteControl(NEWLINE,50);
				putHeader = 1;
			}
			LCD_sendDecimal(UTCtime_lastPulse.hours,2);
			LCD_WriteData(':');
			LCD_sendDecimal(UTCtime_lastPulse.minutes,2);
			LCD_WriteData(':');
			LCD_sendDecimal(UTCtime_lastPulse.seconds,2);
			LCD_WriteData(' ');
			LCD_WriteData('[');
			LCD_sendDecimal((Pulse_Counter-Current_Count),4);
			LCD_WriteData(']');
			LCD_WriteControl(NEWLINE,50);	
		break;
	}
}

/*
 * Send a control byte to the LCD
 */
void LCD_WriteControl(unsigned char value, int time)
{
	LCD_DATA = value;
	PORTF = (0<<LCD_REG_SELECT)|(0<<LCD_READ_WRITE);
	PORTF |= (1<<LCD_ENABLE);
	Delay(time);
	PORTF &= ~(1<<LCD_ENABLE);
	Delay(time);

}

/*
 * Send a data byte to the LCD
 */
void LCD_WriteData(unsigned char value)
{
	LCD_DATA = value;
	PORTF = (1<<LCD_REG_SELECT)|(0<<LCD_READ_WRITE);
	PORTF |= (1<<LCD_ENABLE);
	Delay(5);
	PORTF &= ~(1<<LCD_ENABLE);
	//Delay(5);
}

/*
 * Send a string to the LCD
 */
void LCD_sendmsg(const char *s)
{
	unsigned char qcntr,sndcntr;	//indexes into the queue array holding the msg to send via UART
	unsigned char queue[16];		//character queue array holding msg to send via UART
	uint8_t i;
	qcntr = 0;		//preset indices
	sndcntr = 0;	
	for (i = 0; pgm_read_byte(&s[i]) && i < 16; i++)
		{
		queue[qcntr++] = pgm_read_byte(&s[i]);
		}
		
	while (qcntr != sndcntr) LCD_WriteData(queue[sndcntr++]); //send the god damn msg
}

/*
 * Send a decimal number to the LCD
 */
void LCD_sendDecimal(int number, unsigned char places)
{
	unsigned char thousands = 0;
	unsigned char hundreds = 0;
	unsigned char tens = 0;
	unsigned char ones = 0;

	// TODO: Use integer division and the modulus operator. This is just ridiculous.
	while (number >= 0)
	{
		number = number - 1000;
		thousands++;
	}
	thousands--;
	number = number + 1000;
	
	if (places == 0x04)
	{
		LCD_WriteData(hexToAscii(thousands));
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
		LCD_WriteData(hexToAscii(hundreds));
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
		LCD_WriteData(hexToAscii(tens));
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
		LCD_WriteData(hexToAscii(ones));
		places--;
	}
}
 
/*
 * Returns the LCD cursor to the start of the display
 */
void reset_LCD(void)
{
	putHeader = 0;
}
