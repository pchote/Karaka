//***************************************************************************
//
//  File........: timer2.c
//
//  Author(s)...: Johnny McClymont
//
//  Target(s)...: ATmega128
//
//  Compiler....: AVR-GCC 3.3.1; avr-libc 1.0
//
//  Description.: ATMega128 timer 2 routines
//
//  Revisions...: 1.0
//
//  YYYYMMDD - VER. - COMMENT                                       - SIGN.
//
//  20090605 - 1.0  - Created                                       - J.McClymont
//
//***************************************************************************

#include "timer2.h"
#include "LCD_LIB.h"
#include "main.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <inttypes.h>


/*****************************************************************************
*
*   Function name : timer1_init
*
*   Returns :       None
*
*   Parameters :    None
*
*   Purpose :       Initialises timer 1
*
*****************************************************************************/

void timer2_init(void)
{
	TCCR2 = 0x00;
	TCCR2 &= ~((1<<CS20)|(1<<CS21)|(1<<CS22));
	TIMSK |= (1<<TOIE2);
	TCNT2 = 6;
}

void start_timer2(void)
{
	TCNT2 = 6;	//set time to overflow after 250 ticks
	TCCR2 |= (1<<CS20)|(1<<CS21)|(0<<CS22);	//set ticks to be every 4 useconds
}

void stop_timer2(void)
{
	TCCR2 &= ~((1<<CS20)|(1<<CS21)|(1<<CS22));
}


/******************************************************************************
*
*   Function name:  SIGNAL(SIG_OVERFLOW2)
*
*   returns:        none
*
*   parameters:     none
*
*   Purpose:        Timer 2 overflow interupt
*
*******************************************************************************/

SIGNAL(SIG_OVERFLOW2)
{
	TCNT2 = 6;  //reset timer counter to overflow in 250 ticks
	milliseconds++;
	if(milliseconds >= 999) milliseconds = 999;
}
