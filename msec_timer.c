//***************************************************************************
//
//  File........: msec_timer.c
//  Description.: A timer to keep track of the number of milliseconds
//                that have passed using timer2.
//  Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include "msec_timer.h"

/*
 * Initialise the timer on timer2
 * The timer is not started until the first time pulse is recieved from the gps
 */
void msec_timer_init(void)
{
	// Enable the timer2 overflow interrupt and set initial ticks
	TIMSK |= (1<<TOIE2);
	TCNT2 = MSEC_TIMER_TICKS;
	
	// Set the prescaler to 1/64; each tick = 4us.
	// Also starts the timer counting
	TCCR2 = (0<<CS22)|(1<<CS21)|(1<<CS20);
}

/*
 * Reset the timer and start timing from zero
 */
void msec_timer_reset(void)
{
	TCNT2 = MSEC_TIMER_TICKS;
	milliseconds = 0;
}

/*
 * Timer overflow interupt. Signals that a millisecond has passed.
 */
SIGNAL(SIG_OVERFLOW2)
{
	// Reset timer
	TCNT2 = MSEC_TIMER_TICKS;
	
	// Cap the milliseconds counter at 999. The gps listener will reset
	// the timer to 0 when it recieves a `second boundary' pulse.
	if (milliseconds < 999)
		milliseconds++;
}
