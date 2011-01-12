//***************************************************************************
//
//  File........: timer0.c
//
//  Author(s)...: Johnny McClymont
//
//  Target(s)...: ATmega128
//
//  Compiler....: AVR-GCC 3.3.1; avr-libc 1.0
//
//  Description.: ATMega128 timer 0 routines
//
//  Revisions...: 1.0
//
//  YYYYMMDD - VER. - COMMENT                                       - SIGN.
//
//  20070918 - 1.0  - Created                                       - J.McClymont
//
//***************************************************************************

#include "main.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <inttypes.h>


/*****************************************************************************
*
*   Function name : timer0_init
*
*   Returns :       None
*
*   Parameters :    None
*
*   Purpose :       Initialises timer 0
*
*****************************************************************************/

void timer0_init(void)
{
	TCCR0 = 0x00;
	TCNT0 = 0X00;
	TIMSK |= (1<<TOIE0);		//switch on timer 0 overflow interupt
}

/*****************************************************************************
*
*   Function name : start_timer0
*
*   Returns :       None
*
*   Parameters :    None
*
*   Purpose :       starts timer 0
*
*****************************************************************************/

void start_timer0(void)
{
	TCNT2 = 248;	//set time to overflow after 8 ticks
	TCCR0 = (1<<CS00)|(1<<CS01)|(1<<CS02);	//set ticks to be every 64 useconds
}


/*****************************************************************************
*
*   Function name : _stop_timer0
*
*   Returns :       None
*
*   Parameters :    None
*
*   Purpose :      stops timer 0
*
*****************************************************************************/
void stop_timer0(void)
{
	TCCR0 &= ~((1<<CS00)|(1<<CS01)|(1<<CS02));	//turn off the timer clock;
}


/******************************************************************************
*
*   Function name:  SIGNAL(SIG_OVERFLOW0)
*
*   returns:        none
*
*   parameters:     none
*
*   Purpose:        Timer 0 overflow interupt
*
*******************************************************************************/

SIGNAL(SIG_OVERFLOW0)
{
	TCNT0 = 248;  //reset timer counter to overflow in 8 ticks
	if (pulse_timer-- == 1)
	{
		PORTA |= (1<<CCD_PULSE);	//drop ccd pulse to end pulse
		stop_timer0();				//stop timer 0
	}
}
