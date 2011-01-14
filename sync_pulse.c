//***************************************************************************
//
//  File........: sync_pulse.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Sends a 512us "sync" pulse to the camera 
//                via pin XXXX using timer0
//
//***************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <inttypes.h>
#include "main.h"

/*
 * Initialise the timer on timer0
 */
void sync_pulse_init(void)
{
	TCCR0 = 0x00;
	TCNT0 = 0X00;
	
	// Enable timer 0 overflow interrupt
	TIMSK |= (1<<TOIE0);
}

/*
 * Trigger a sync pulse to the camera
 * Sets timer0 to overflow after 512us
 */
void sync_pulse_trigger(void)
{
	// Set the Sync output HIGH.
	// TODO: Why is this GPS_PULSE? Shouldn't it be CCD_PULSE?
	PORTA &= ~(1<<GPS_PULSE);

	// Overflow after 8 ticks
	// TODO: Why is this TCNT2? Shouldn't it be TCNT0?
	TCNT2 = 248;
	
	// Set tick length to 64us
	TCCR0 = (1<<CS00)|(1<<CS01)|(1<<CS02);
}

/*
 * Interrupt signal handler for the timer0 interrupt.
 * This signals the end the output sync pulse.
 */
SIGNAL(SIG_OVERFLOW0)
{
	TCNT0 = 248;  //reset timer counter to overflow in 8 ticks
	if (pulse_timer-- == 1)
	{
		// Set the Sync pulse LOW to end the pulse.
		PORTA |= (1<<CCD_PULSE);
		
		// Turn off the timer clock
		TCCR0 &= ~((1<<CS00)|(1<<CS01)|(1<<CS02));
	}
}
