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
#include "monitor.h"

/*
 * Enable the camera output line and setup timer0 for setting the pulse length
 */
void download_init()
{
    // Set pin as an output, initially high
    DDRD |= _BV(DDD5);
    PORTD &= ~_BV(PD5);

    // Enable compare interrupt
    TIMSK0 |= _BV(OCIE0A);

    // Set timer to compare match after 0.512 ms
    OCR0A = 79;

    // Set timer to reset to 0 on compare match
    TCCR0A = _BV(WGM01);

    // Disable the timer until it is needed
    TCCR0B = 0;
}

/*
 * Start a download pulse to the camera
 * by pulling the camera output line low
 */
void trigger_download()
{
    // Pull PD5 output low to start the download
    PORTD |= _BV(PD5);
    TCCR0B = _BV(CS01) | _BV(CS00);

    // Trigger a fake download
    if (monitor_simulate_camera)
        simulate_camera_download();
}

/*
 * timer0 compare interrupt
 * Restores camera output line high
 */
ISR(TIMER0_COMPA_vect)
{
    TCCR0B = 0;
    PORTD &= ~_BV(PD5);
}
