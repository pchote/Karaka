//***************************************************************************
//
//  File........: timer1.c
//
//  Author(s)...: Johnny McClymont
//
//  Target(s)...: ATmega128
//
//  Compiler....: AVR-GCC 3.3.1; avr-libc 1.0
//
//  Description.: ATMega128 timer 1 routines
//
//  Revisions...: 1.0
//
//  YYYYMMDD - VER. - COMMENT                                       - SIGN.
//
//  20070903 - 1.0  - Created                                       - J.McClymont
//
//***************************************************************************

#include "timer1.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <inttypes.h>
#include "main.h"
#include "LCD_LIB.h"
#include "usart1.h"
#include "Command_Layer.h"


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

void timer1_init(void)
{
	TCCR1A = 0x00;
	TCCR1B &= ~((1<<CS10)|(1<<CS11)|(1<<CS12));
	TIMSK |= (1<<TOIE1);
	TCNT1 = 0XF0BD;
}

void start_timer1(void)
{
	TCNT1 = 0XF0BD;	//overflow after 7812 ticks ~ 0.5s
	TCCR1B |= (1<<CS10)|(0<<CS11)|(1<<CS12);	//set tick time to 64uS
}

void stop_timer1(void)
{
	TCCR1B &= ~((1<<CS10)|(1<<CS11)|(1<<CS12));		//stop timer 1 clock
}

/******************************************************************************
*
*   Function name:  SIGNAL(SIG_OVERFLOW1)
*
*   returns:        none
*
*   parameters:     none
*
*   Purpose:        Timer 1 overflow interupt
*
*******************************************************************************/

SIGNAL(SIG_OVERFLOW1)
{
	stop_timer1();
	update_LCD(GPS_state);
	start_timer1();
	
	if (GPS_state != SYNCING)
	{
		if(check_GPS_present++ > 16)  
		{
			gps_usart_state = SYNCING_PACKETS;
			GPS_state = SYNCING;
			error_state |= GPS_SERIAL_LOST;
			error_state = error_state & 0xFE;
			reset_LCD();
			check_GPS_present = 0;
		}
	}
}
