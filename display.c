//***************************************************************************
//
//  File........: display.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: LCD display routines
//
//***************************************************************************

#include "display.h"
#include "main.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <inttypes.h>
#include "UART_Math.h"
#include "usart.h"
#include "usart1.h"
#include "Command_Layer.h"

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
	
	// Initialise the lcd update timer on timer1
	TCCR1A = 0x00;
	TCCR1B &= ~((1<<CS10)|(1<<CS11)|(1<<CS12));
	TIMSK |= (1<<TOIE1);
	TCNT1 = 0XF0BD;
	
	start_timer1();
}

/*
 * Start the timer to trigger an overflow interrupt every ~0.5 seconds
 */
void start_timer1(void)
{
	// Start the timer
	TCNT1 = 0XF0BD;	//overflow after 7812 ticks ~ 0.5s
	TCCR1B |= (1<<CS10)|(0<<CS11)|(1<<CS12);	//set tick time to 64uS
}


/*
 * Interrupt signal handler for timer1.
 * This signals that the lcd should be updated
 */
SIGNAL(SIG_OVERFLOW1)
{
	TCCR1B &= ~((1<<CS10)|(1<<CS11)|(1<<CS12));	//stop timer 1 clock
	update_LCD(GPS_state);
	start_timer1();
	
	if (GPS_state != SYNCING)
	{
		if(check_GPS_present++ > 16)  
		{
			gps_usart_state = SYNCING_PACKETS;
			GPS_state = SYNCING;
			error_state |= GPS_SERIAL_LOST;
			error_state = error_state & 0xFE;
			reset_LCD();
			check_GPS_present = 0;
		}
	}
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
			LCD_sendDecimal(Pulse_Counter - Current_Count,4);
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
 * Display a string stored in flash memory
 * The display is limited to 16 characters; longer strings will be truncated
 */
void LCD_sendmsg(const char *s)
{
	// Read the message from flash memory into a buffer
	unsigned char queue[16];
	unsigned char qcntr;
	for (qcntr = 0; pgm_read_byte(&s[qcntr]) && qcntr < 16; qcntr++)
		queue[qcntr] = pgm_read_byte(&s[qcntr]);
	
	// Send the characters to the display
	unsigned char sndcntr = 0;
	while (sndcntr < qcntr)
		LCD_WriteData(queue[sndcntr++]);
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
