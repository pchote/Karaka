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
#include <stdio.h>

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
	PORTC = value;
	PORTF = _BV(LCD_ENABLE);
	wait_usec(time);
	PORTF &= ~_BV(LCD_ENABLE);
	wait_usec(time);
}

/*
 * Send a data byte to the LCD
 */
static void write_byte(unsigned char value)
{
	PORTC = value;
	PORTF = _BV(LCD_REG_SELECT)|_BV(LCD_ENABLE);
	// Wait for operation to complete
    wait_usec(10);
	PORTF &= ~_BV(LCD_ENABLE);
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
 * Write a string to the header line of the LCD
 */
static void write_header(const char *msg)
{
	write_raw(CURSOR_HOME, 50);
	write_raw(DISPLAY_CLEAR, 500);
	write_string(msg);
	write_raw(NEWLINE,10);
}

static void write_array(const char *s, unsigned char len)
{
    for (unsigned char i = 0; i < len; i++)
        write_byte(s[i]);
}

/*
 * Initialise the LCD
 */
void display_init(void)
{
	// Set all of PORTC as data output
	DDRC |= 0xFF;
	// Set status pins as output
	DDRF |= _BV(LCD_ENABLE)|_BV(LCD_READ_WRITE)|_BV(LCD_REG_SELECT);
	
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

            char buf[16];
            sprintf(buf, "%02d:%02d:%02d   ",
                    gps_last_timestamp.hours,
                    gps_last_timestamp.minutes,
                    gps_last_timestamp.seconds);
            write_array(buf, 11);

			if (exposure_countdown != 0)
			{
                sprintf(buf, "[%03d]", exposure_countdown);
                write_array(buf, 5);
			}
			else
                write_string(PSTR("     "));

			write_raw(NEWLINE,10);	
		break;
	}
	display_last_gps_state = gps_state;
}