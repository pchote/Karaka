//***************************************************************************
//
//	File........: download.c
//	Description.: Commands the camera to download a frame by pulling the
//				  PA0 pin LOW for 512us. Uses Timer0
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
void download_init(void)
{
	// Set Port A, pin 0 as an output
    DDRA |= _BV(DDA0);
    
    // Start with all pins high
    PORTA = 0x00;

	// Enable timer0 overflow interrupt
	TIMSK |= _BV(TOIE0);
	
	// Disable the timer until it is needed
	TCCR0 = 0x00;
}

/*
 * Trigger a sync pulse to the camera
 * Pulls PA0 pin LOW and sets
 * timer0 to overflow after 512us
 */
void trigger_download(void)
{
	// Pull PA0 output low to start the download
	PORTA |= _BV(PA0);

	// Set the prescaler to 1/1024; each tick = 64us.
	// Also starts the timer counting
	TCCR0 = _BV(CS02)|_BV(CS01)|_BV(CS00);
	
	// Set timer0 to overflow after 8 ticks (0.512 ms)
	TCNT0 = 248;
}

/*
 * timer0 overflow interrupt
 * Pulls PA0 pin HIGH and disables timer0
 * to disable the download trigger pulse
 */
SIGNAL(SIG_OVERFLOW0)
{
	// Restore PA0 output high
	PORTA &= ~_BV(PA0);
	TCCR0 = 0x00;
}
