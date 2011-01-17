//***************************************************************************
//
//  File........: display.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: LCD display routines
//
//***************************************************************************

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "display.h"
#include "main.h"
#include "UART_Math.h"
#include "usart1.h"
#include "Command_Layer.h"

/*
 * Initialise the LCD
 */
void display_init(void)
{
	Delay(50000);
	display_write_control(0x38, 500);
	display_write_control(CLEARLCD, 500);
	display_write_control(CURSORON, 500);
	display_write_control(CURSORHOME, 500);
	display_write_control(0x06, 500);
	
	Delay(500);
	display_write_string(PSTR("GPS KARAKA UNIT"));
	display_write_control(NEWLINE,500);
	display_write_string(PSTR("Kia Ora"));
	Delay(65000);
	Delay(65000);
	cursor_ptr = 0;
	putHeader = 0;
	
	// Initialise the lcd update timer on timer1
	TCCR1A = 0x00;
	TIMSK |= (1<<TOIE1);
	TCNT1 = DISPLAY_TIMER_TICKS;
	TCCR1B |= (1<<CS10)|(0<<CS11)|(1<<CS12);	//set tick time to 64uS
}

/*
 * Interrupt signal handler for timer1.
 * This signals that the lcd should be updated
 */
SIGNAL(SIG_OVERFLOW1)
{
	// Reset the counter
	TCNT1 = DISPLAY_TIMER_TICKS;	
	display_set_state(GPS_state);
	
	// Have we lost contact with the GPS?
	if(GPS_state != SYNCING && check_GPS_present++ > 16)  
	{
		gps_usart_state = SYNCING_PACKETS;
		GPS_state = SYNCING;
		error_state |= GPS_SERIAL_LOST;
		error_state = error_state & 0xFE;
		display_reset_header();
		check_GPS_present = 0;
	}
}

void display_write_header(const char *msg)
{
	if (putHeader == 0)
	{
		display_write_control(CURSORHOME, 50);
		display_write_control(CLEARLCD, 500);
		display_write_string(msg);
		display_write_control(NEWLINE,50);
		putHeader = 1;
	}
}

/*
 * Returns the LCD cursor to the start of the display
 */
void display_reset_header(void)
{
	putHeader = 0;
}

/*
 * Set the message display mode for the LCD 
 */
void display_set_state(unsigned char LCD_state)
{	
	switch (LCD_state)
	{
		case SYNCING:
			display_write_header(PSTR("SYNCING TO GPS"));
			display_write_byte('.');
			if (cursor_ptr++ == 15)
			{
				cursor_ptr = 0;
				display_write_control(NEWLINE,50);
				display_write_string(PSTR("                "));
				display_write_control(NEWLINE,50);
			}
		break;
		
		case SETUP_GPS:
			display_write_header(PSTR("SETTING UP GPS"));
			display_write_byte('.');
			
			if (cursor_ptr++ == 15)
			{
				cursor_ptr = 0;
				display_write_control(NEWLINE,50);
				display_write_string(PSTR("                "));
				display_write_control(NEWLINE,50);
			}
		break;
		
		case CHECK_GPS_TIME_VALID:
			display_write_header(PSTR("CHECK GPS LOCK"));
			display_write_byte('.');
			
			if (cursor_ptr++ == 15)
			{
				cursor_ptr = 0;
				display_write_control(NEWLINE,50);
				display_write_string(PSTR("                "));
				display_write_control(NEWLINE,50);
			}
		break;
		
		case GPS_TIME_GOOD:
			display_write_header(PSTR("UTC TIME"));
			display_write_number(UTCtime_lastPulse.hours,2);
			display_write_byte(':');
			display_write_number(UTCtime_lastPulse.minutes,2);
			display_write_byte(':');
			display_write_number(UTCtime_lastPulse.seconds,2);
			display_write_byte(' ');
			display_write_byte('[');
			display_write_number(Pulse_Counter - Current_Count,4);
			display_write_byte(']');
			display_write_control(NEWLINE,50);	
		break;
	}
}

/*
 * Send a control byte to the LCD
 */
void display_write_control(unsigned char value, int time)
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
void display_write_byte(unsigned char value)
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
void display_write_string(const char *s)
{
	// Read the message from flash memory into a buffer
	unsigned char queue[16];
	unsigned char qcntr;
	for (qcntr = 0; pgm_read_byte(&s[qcntr]) && qcntr < 16; qcntr++)
		queue[qcntr] = pgm_read_byte(&s[qcntr]);
	
	// Send the characters to the display
	unsigned char sndcntr = 0;
	while (sndcntr < qcntr)
		display_write_byte(queue[sndcntr++]);
}

/*
 * Display an integer with a given number of digits
 */
void display_write_number(int number, unsigned char places)
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
		display_write_byte(hexToAscii(p));
	}
}
