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

char top_display[17];
char top_cache[17];
char bottom_display[17];
char bottom_cache[17];
void update_display()
{
	// Update the lcd display
	switch (gps_state)
	{
		case GPS_SYNCING:
		case GPS_UNAVAILABLE:
			if (display_last_gps_state != gps_state)
			{
                PGM_P str = gps_state == GPS_SYNCING ? PSTR("SYNCING TO GPS  ") : PSTR("GPS NOT FOUND   ");
                strcpy_P(top_display, str);
			}

			for (int i = 0; i < 16; i++)
                bottom_display[i] = (i < display_cursor) ? '.' : ' ';
            if (display_cursor++ >= 16) display_cursor = 0;
		break;
		case GPS_ACTIVE:
			if (display_last_gps_state != gps_state || gps_last_timestamp.locked != display_gps_was_locked)
			{
                PGM_P str = gps_last_timestamp.locked ? PSTR("UTC TIME: LOCKED") : PSTR("UTC TIME:       ");
                strcpy_P(top_display, str);
                display_gps_was_locked = gps_last_timestamp.locked;
			}

            sprintf(bottom_display, "%02d:%02d:%02d        ",
                    gps_last_timestamp.hours,
                    gps_last_timestamp.minutes,
                    gps_last_timestamp.seconds);

			if (exposure_countdown != 0)
                sprintf((char *)(bottom_display+11), "[%03d]", exposure_countdown);
            break;
	}

    if (strcmp(top_display, top_cache))
    {
        send_instruction(CURSOR_TOP);
        _delay_ms(1.64);
        for (unsigned char i = 0; i < 16 && i < strlen(top_display); i++)
            print_char(top_display[i]);

        strcpy(top_cache, top_display);
    }

    if (strcmp(bottom_display, bottom_cache))
    {
        send_instruction(CURSOR_BOTTOM);
        _delay_us(40);
        for (unsigned char i = 0; i < 16 && i < strlen(bottom_display); i++)
            print_char(bottom_display[i]);

        strcpy(bottom_cache, bottom_display);
    }

    display_last_gps_state = gps_state;
}