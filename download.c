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
#include "fakecamera.h"

/*
 * Enable the camera output line and setup timer0 for setting the pulse length
 */
void download_init_hardware()
{
    // Set PD5 as an output, initially high
    DDRD |= _BV(DDD5);
    PORTD &= ~_BV(PD5);

    // Enable timer0 overflow interrupt
    TIMSK0 |= _BV(TOIE0);

    // Disable the timer until it is needed
    TCCR0B = 0x00;
}

/*
 * Start a download pulse to the camera
 * by pulling the camera output line low
 */
void trigger_download()
{
    // Pull PD5 output low to start the download
    PORTD |= _BV(PD5);

    // Set the timer tick to 64us
    TCCR0B = _BV(CS02)|_BV(CS00);

    // Set timer to overflow after 8 ticks (0.512 ms)
    TCNT0 = 248;

    // Trigger a fake download
    fake_camera_download();
}

/*
 * timer0 overflow interrupt
 * Restores camera output line high
 */
ISR(TIMER0_OVF_vect)
{
    PORTD &= ~_BV(PD5);
    TCCR0B = 0x00;
}
