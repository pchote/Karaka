//***************************************************************************
//
//  File        : download.c
//  Copyright   : 2009-2012 Johnny McClymont, Paul Chote
//  Description : Triggers camera frame downloads by changing the level of PA0
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include "download.h"
#include "main.h"

/*
 * Initialise the timer on timer0
 */
void download_init()
{
    // Set PD5 as an output for INTG, initially high
    DDRD |= _BV(DDD5);
    PORTD &= 0xDF;

    // Enable timer0 overflow interrupt
    TIMSK0 |= _BV(TOIE0);

    // Disable the timer until it is needed
    TCCR0B = 0x00;
}

/*
 * Trigger a sync pulse to the camera
 * Pulls PA0 pin LOW and sets
 * timer0 to overflow after 512us
 */
void trigger_download()
{
    // Pull PD5 output low to start the download
    PORTD |= _BV(PD5);

    // Set the prescaler to 1/1024; each tick = 64us.
    // Also starts the timer counting
    TCCR0B = _BV(CS02)|_BV(CS00);

    // Set timer0 to overflow after 8 ticks (0.512 ms)
    TCNT0 = 248;
}

/*
 * timer0 overflow interrupt
 * Pulls PA0 pin HIGH and disables timer0
 * to disable the download trigger pulse
 */
ISR(TIMER0_OVF_vect)
{
    // Restore PD5 output high
    PORTD &= ~_BV(PD5);
    TCCR0B = 0x00;
}
