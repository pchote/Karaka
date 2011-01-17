//***************************************************************************
//
//  File........: msec_timer.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: A timer to keep track of the number of milliseconds
//                that have passed using timer2.
//
//***************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <inttypes.h>
#include "msec_timer.h"
#include "main.h"

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
