//***************************************************************************
//
//  File........: sync_pulse.c
//  Description.: Sends a 512us "sync" pulse to the camera 
//                via PINA0 using timer0
//  Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include "main.h"

/*
 * Initialise the timer on timer0
 */
void sync_pulse_init(void)
{
	// Enable timer 0 overflow interrupt
	TIMSK |= (1<<TOIE0);
	
	// Disable the timer until it is needed
	TCCR0 = 0x00;
}

/*
 * Trigger a sync pulse to the camera
 * Sets timer0 to overflow after 512us
 */
void sync_pulse_trigger(void)
{
	// Set the CCD_PULSE pin LOW to start the sync pulse
	PORTA &= ~(1<<CCD_PULSE);

	// Overflow after 8 ticks (0.512ms)
	TCNT0 = 248;
	
	// Set the prescaler to 1/1024; each tick = 64us.
	// Also starts the timer counting
	TCCR0 = (1<<CS02)|(1<<CS01)|(1<<CS00);
}

/*
 * Interrupt signal handler for the timer0 interrupt.
 * This signals the end the output sync pulse.
 */
SIGNAL(SIG_OVERFLOW0)
{
	// Set the CCD_PULSE pin HIGH to end the pulse.
	PORTA |= (1<<CCD_PULSE);
		
	// Turn off the timer
	TCCR0 &= ~(1<<CS00)|(1<<CS01)|(1<<CS02);
}
