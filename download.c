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

#if CPU_TYPE == CPU_ATMEGA128
#   define DOWNLOAD_PORT PORTA
#   define DOWNLOAD_DDR DDRA
#   define DOWNLOAD_PIN PA0
#   define DOWNLOAD_DD DDA0
#   define DOWNLOAD_TIMSK TIMSK
#   define DOWNLOAD_TCCR TCCR0
#   define DOWNLOAD_STOP_TIMER (TCCR0 = _BV(WGM01))
#   define DOWNLOAD_START_TIMER (TCCR0 = _BV(WGM01)|_BV(CS02))
#   define DOWNLOAD_OC_BIT OCIE0
#   define DOWNLOAD_OCR OCR0
#elif CPU_TYPE == CPU_ATMEGA1284p
#   define DOWNLOAD_PORT PORTD
#   define DOWNLOAD_DDR DDRD
#   define DOWNLOAD_PIN PD5
#   define DOWNLOAD_DD DDD5
#   define DOWNLOAD_TIMSK TIMSK0
#   define DOWNLOAD_TCCR TCCR0A
#   define DOWNLOAD_STOP_TIMER (TCCR0B = 0)
#   define DOWNLOAD_START_TIMER (TCCR0B = _BV(CS01)|_BV(CS00))
#   define DOWNLOAD_OC_BIT OCIE0A
#   define DOWNLOAD_OCR OCR0A
#else
#   error Unknown CPU type
#endif

/*
 * Enable the camera output line and setup timer0 for setting the pulse length
 */
void download_init()
{
    // Set pin as an output, initially high
    DOWNLOAD_DDR |= _BV(DOWNLOAD_DD);
    DOWNLOAD_PORT &= ~_BV(DOWNLOAD_PIN);

    // Enable compare interrupt
    DOWNLOAD_TIMSK |= _BV(DOWNLOAD_OC_BIT);

    // Set timer to compare match after 0.512 ms
#if CPU_MHZ == 16
    DOWNLOAD_OCR = 127;
#elif CPU_MHZ == 10
    DOWNLOAD_OCR = 79;
#else
#   error Unknown CPU Frequency
#endif

    // Set timer to reset to 0 on compare match
    DOWNLOAD_TCCR = _BV(WGM01);

    // Disable the timer until it is needed
    DOWNLOAD_STOP_TIMER;
}

/*
 * Start a download pulse to the camera
 * by pulling the camera output line low
 */
void trigger_download()
{
    // Pull PD5 output low to start the download
    DOWNLOAD_PORT |= _BV(DOWNLOAD_PIN);
    DOWNLOAD_START_TIMER;

    // Trigger a fake download
    if (monitor_simulate_camera)
        simulate_camera_download();
}

/*
 * timer0 compare interrupt
 * Restores camera output line high
 */
#if CPU_TYPE == CPU_ATMEGA128
ISR(TIMER0_COMP_vect)
#elif CPU_TYPE == CPU_ATMEGA1284p
ISR(TIMER0_COMPA_vect)
#else
#   error Unknown CPU type
#endif
{
    DOWNLOAD_STOP_TIMER;
    DOWNLOAD_PORT &= ~_BV(DOWNLOAD_PIN);
}
