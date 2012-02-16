//***************************************************************************
//
//  File        : fakecamera.h
//  Copyright   : 2012 Paul Chote
//  Description : Provides a fake camera status level to connect to the
//                monitor input for testing without a camera present
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "command.h"

void fake_camera_init_state()
{
    // Disable counter until we need it
    TCCR3B = 0;

    // Set output high
    // (output buffer inverts the logic level, so set logic low to give output high)
    PORTD &= ~_BV(PD7);
}

static void set_output_low(uint16_t timer_cnt)
{
    PORTD |= _BV(PD7);

    cli();
    // Define timer tick as 64us
    TCCR3B = _BV(CS30)|_BV(CS32);
    TCNT3 = timer_cnt;
    sei();
}
/*
 * Set fake camera output line and enable timer3 for delaying
 */
void fake_camera_init_hardware()
{
    DDRD |= _BV(DDD7);
    TIMSK3 |= _BV(TOIE3);
}

/*
 * Set output low, delay for 4.19 seconds, then high
 */
void fake_camera_startup()
{
    set_output_low(0x0000);
}

/*
 * Set output low, delay for 1 second, then high
 */
void fake_camera_shutdown()
{
    set_output_low(0xC2F6);
}

/*
 * Set output low, delay for 3.2 seconds, then high
 */
void fake_camera_download()
{
    set_output_low(0x3CAF);
}

/*
 * Timer3 overflow interrupt
 * Return level high and stop timer
 */
ISR(TIMER3_OVF_vect)
{
    PORTD &= ~_BV(PD7);
    TCCR3B = 0;
}
