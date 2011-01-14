//***************************************************************************
//
//  File........: msec_timer.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: A timer to keep track of the number of milliseconds
//                that have passed.
//
//***************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <inttypes.h>
#include "msec_timer.h"
#include "LCD_LIB.h"
#include "main.h"

/*
 * Initialise the timer on timer2
 */
void msec_timer_init(void)
{
	TCCR2 = 0x00;
	TCCR2 &= ~((1<<CS20)|(1<<CS21)|(1<<CS22));
	TIMSK |= (1<<TOIE2);
	TCNT2 = 6;
}

/*
 * Start a counter to fire an interrupt after 1 millisecond
 */
void msec_timer_start(void)
{
	TCNT2 = 6;	//set time to overflow after 250 ticks
	TCCR2 |= (1<<CS20)|(1<<CS21)|(0<<CS22);	// set ticks to be every 4 useconds
}

/*
 * Stop the timer
 */
void msec_timer_stop(void)
{
	// TODO: What is this actually doing?
	TCCR2 &= ~((1<<CS20)|(1<<CS21)|(1<<CS22));
}

/*
 * Timer overflow interupt. Signals that a millisecond has passed.
 */
SIGNAL(SIG_OVERFLOW2)
{
	TCNT2 = 6;  // reset timer counter to overflow in 250 ticks
	milliseconds++;
	
	// TODO: Why isn't this reset to zero?
	if(milliseconds >= 999)
		milliseconds = 999;
}
