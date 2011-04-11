//***************************************************************************
//
//	File........: download.c
//	Description.: Commands the camera to download a frame by pulling the
//				  CCD_PULSE pin LOW for 512us. Uses Timer0
//	Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//	This file is part of Karaka, which is free software. It is made available
//	to you under the terms of version 3 of the GNU General Public License, as
//	published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include "download.h"
#include "main.h"

/*
 * Initialise the timer on timer0
 */
void init_download(void)
{
	// Enable timer0 overflow interrupt
	TIMSK |= (1<<TOIE0);
	
	// Disable the timer until it is needed
	TCCR0 = 0x00;
}

/*
 * Trigger a sync pulse to the camera
 * Pulls CCD_PULSE pin LOW and sets
 * timer0 to overflow after 512us
 */
void trigger_download(void)
{
	// Set the CCD_PULSE pin LOW to start the download
	PORTA &= ~(1<<CCD_PULSE);
	
	// Set the prescaler to 1/1024; each tick = 64us.
	// Also starts the timer counting
	TCCR0 = (1<<CS02)|(1<<CS01)|(1<<CS00);
	
	// Set timer0 to overflow after 8 ticks (0.512 ms)
	TCNT0 = 248;
}

/*
 * timer0 overflow interrupt
 * Pulls CCD_PULSE pin HIGH and disables timer0
 */
SIGNAL(SIG_OVERFLOW0)
{
	PORTA |= (1<<CCD_PULSE);
	TCCR0 = 0x00;
}