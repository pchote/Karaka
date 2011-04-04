//***************************************************************************
//
//  File........: display.c
//  Description.: LCD display routines
//  Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "display.h"
#include "main.h"
#include "command.h"
#include "gps.h"

/*
 * Wait for a specified number of usec
 */
static void wait_usec(unsigned int usec)
{
	while (usec--)
		for (unsigned char i = 0; i < 16; i++)  
			asm volatile ("nop"::);
}

/*
 * Send a control byte to the LCD
 */
static void write_raw(unsigned char value, int time)
{
	LCD_DATA = value;
	PORTF = (0<<LCD_REG_SELECT)|(0<<LCD_READ_WRITE);
	PORTF |= (1<<LCD_ENABLE);
	wait_usec(time);
	PORTF &= ~(1<<LCD_ENABLE);
	wait_usec(time);
}

/*
 * Send a data byte to the LCD
 */
static void write_byte(unsigned char value)
{
	LCD_DATA = value;
	PORTF = (1<<LCD_REG_SELECT)|(0<<LCD_READ_WRITE);
	PORTF |= (1<<LCD_ENABLE);
	// Wait for operation to complete
    wait_usec(10);
	PORTF &= ~(1<<LCD_ENABLE);
}

/*
 * Display a string stored in flash memory
 * The display is limited to 16 characters; longer strings will be truncated
 */
static void write_string(const char *s)
{
	// Read the message from flash memory into a buffer
	unsigned char queue[16];
	unsigned char qcntr;
	for (qcntr = 0; pgm_read_byte(&s[qcntr]) && qcntr < 16; qcntr++)
		queue[qcntr] = pgm_read_byte(&s[qcntr]);
	
	// Send the characters to the display
	unsigned char sndcntr = 0;
	while (sndcntr < qcntr)
		write_byte(queue[sndcntr++]);
}

/*
 * Display an integer with a given number of digits
 */
static void write_number(int number, unsigned char places)
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
		write_byte(nibble_to_ascii(p));
	}
}

/*
 * Write a string to the header line of the LCD
 */
static void write_header(const char *msg)
{
	write_raw(CURSOR_HOME, 50);
	write_raw(DISPLAY_CLEAR, 500);
	write_string(msg);
	write_raw(NEWLINE,10);
}

/*
 * Initialise the LCD
 */
void display_init(void)
{
	wait_usec(50000);
	write_raw(INITIALIZE, 40);
	write_raw(DISPLAY_CLEAR, 1640);
	write_raw(INITIALIZEB, 500);
	write_raw(CURSOR_HOME, 500);
	write_raw(0x06, 500);
	
	display_cursor = 0;
    display_gps_was_locked = FALSE;
	display_last_gps_state = -1;
}

void update_display()
{
	// Update the lcd display
	switch (gps_state)
	{
		case SYNCING:
			if (display_last_gps_state != gps_state)
				write_header(PSTR("SYNCING TO GPS"));
			
			write_byte('.');
			if (display_cursor++ == 15)
			{
				display_cursor = 0;
				write_raw(NEWLINE,10);
				write_string(PSTR("                "));
				write_raw(NEWLINE,10);
			}
		break;
		
		case NO_GPS:
			if (display_last_gps_state != gps_state)
				write_header(PSTR("GPS NOT FOUND"));
			
			write_byte('.');
			
			if (display_cursor++ == 15)
			{
				display_cursor = 0;
				write_raw(NEWLINE,10);
				write_string(PSTR("                "));
				write_raw(NEWLINE,10);
			}
		break;
		
		case GPS_TIME_GOOD:
			if (display_last_gps_state != gps_state || gps_last_timestamp.locked != display_gps_was_locked)
			{
                write_header(gps_last_timestamp.locked ? PSTR("UTC TIME: LOCKED") : PSTR("UTC TIME:"));
                display_gps_was_locked = gps_last_timestamp.locked;
			}
			
			write_number(gps_last_timestamp.hours,2);
			write_byte(':');
			write_number(gps_last_timestamp.minutes,2);
			write_byte(':');
			write_number(gps_last_timestamp.seconds,2);
			write_byte(' ');
			write_byte(' ');
			write_byte('[');
			write_number(exposure_total - exposure_count, 4);
			write_byte(']');
			write_raw(NEWLINE,10);	
		break;
	}
	display_last_gps_state = gps_state;
}