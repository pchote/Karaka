//***************************************************************************
//
//  File........: main.h
//  Description.: ATMega128 USB Timer interface module
//  Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <string.h>

#ifndef KARAKA_MAIN_H
#define KARAKA_MAIN_H

#define FALSE (0)
#define TRUE (!(FALSE))

#define SWHIGH	1
#define SWLOW	0
#define SWREV	1

#define RXD			    PINE0   // UART 0 RECEIVE
#define TXD				PINE1   // UART 0 TRANSMIT
#define TXD1			PIND3	// UART 1 TRANSMIT
#define RXD1			PIND2	// UART 1 RECEIVE

#define GPS_PULSE		PIND0	// Input for GPS Pulse

#define CCD_PULSE		PINA0	// Output for CCD pulse
#define UP				PINA1	// Input for UP button
#define LEFT			PINA2	// Input for Left button
#define DOWN			PINA3	// Input for Down button
#define RIGHT			PINA4	// Input for Right button
#define CENTER			PINA5	// Input for Center button
#define SWITCH_CHANGE	PINE4	// Input to indicate a switch change has occurred


#define LCD_ENABLE		PINF2	// Output to enable LCD
#define LCD_READ_WRITE	PINF1	// Output to select reading or writing from LCD (Read active LOW)
#define LCD_REG_SELECT	PINF0	// Output to select register

#define LCD_DATA		PORTC	// Output data port for LCD

unsigned char status_register;
unsigned char control_register;
unsigned int exposure_total;
unsigned int exposure_count;
unsigned char error_state;

unsigned char exposure_syncing;

int command_cntr;

unsigned char ascii_to_nibble(unsigned char a);
unsigned char nibble_to_ascii(unsigned char n);

#endif
