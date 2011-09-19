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

#include "display.h"
#include "main.h"
#include "command.h"
#include "gps.h"

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <util/delay.h>

/*
 * Send a control byte to the LCD
 */
static void send_instruction(unsigned char value)
{
	// Load the instruction into the data output
    PORTC = value;

    // Select instruction register
    PORTF &= ~_BV(LCD_REG_SELECT);

    // Wait for >tAS = 140ns
    _delay_us(1);

    // Toggle enable bit to trigger the download
	PORTF = _BV(LCD_ENABLE);
	_delay_us(40);
	PORTF &= ~_BV(LCD_ENABLE);
}

/*
 * Send a data byte to the LCD
 */
static void print_char(unsigned char value)
{
	// Load the character into the data output
	PORTC = value;

    // Select data register
	PORTF |= _BV(LCD_REG_SELECT);

    // Wait for >tAS = 140ns
    _delay_us(1);

    // Toggle enable bit to trigger the download
    PORTF |= _BV(LCD_ENABLE);
    _delay_us(40);
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
		print_char(queue[sndcntr++]);
}

/*
 * Write a string to the header line of the LCD
 */
static void write_header(const char *msg)
{
	send_instruction(CURSOR_TOP);
    _delay_ms(1.64);
	write_string(msg);
	send_instruction(CURSOR_BOTTOM);
    _delay_us(40);
}

static void write_array(const char *s, unsigned char len)
{
    for (unsigned char i = 0; i < len; i++)
        print_char(s[i]);
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
	
	send_instruction(INITIALIZE);
    _delay_ms(4.1);
	send_instruction(HIDE_CURSOR);
	send_instruction(DISPLAY_CLEAR);
    _delay_ms(1.64);

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
				write_header(PSTR("SYNCING TO GPS  "));
			
			print_char('.');
			if (display_cursor++ == 15)
			{
				display_cursor = 0;
                send_instruction(CURSOR_BOTTOM);
                _delay_us(40);
				write_string(PSTR("                "));
                send_instruction(CURSOR_BOTTOM);
                _delay_us(40);
			}
		break;
		
		case NO_GPS:
			if (display_last_gps_state != gps_state)
				write_header(PSTR("GPS NOT FOUND   "));
			
			print_char('.');
			
			if (display_cursor++ == 15)
			{
				display_cursor = 0;
                send_instruction(CURSOR_BOTTOM);
                _delay_us(40);
				write_string(PSTR("                "));
                send_instruction(CURSOR_BOTTOM);
                _delay_us(40);
			}
		break;
		
		case GPS_TIME_GOOD:
			if (display_last_gps_state != gps_state || gps_last_timestamp.locked != display_gps_was_locked)
			{
                write_header(gps_last_timestamp.locked ? PSTR("UTC TIME: LOCKED") : PSTR("UTC TIME:       "));
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

            send_instruction(CURSOR_BOTTOM);
            _delay_us(40);
            break;
	}
	display_last_gps_state = gps_state;
}