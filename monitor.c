//***************************************************************************
//
//    File........: monitor.c
//    Description.: ATMega128 USB timer card.     Interface between GPS module, 
//                  CCD camera, and Laptop
//    Copyright...: 2011 Paul Chote
//
//    This file is part of Karaka, which is free software. It is made available
//    to you under the terms of version 3 of the GNU General Public License, as
//    published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include "monitor.h"
#include "main.h"
#include "command.h"

static volatile char debounce_waiting;

/*
 * Initialise the timer on timer0
 */
void monitor_init(void)
{
    debounce_waiting = FALSE;
    monitor_level_high = FALSE;
    monitor_mode = MONITOR_WAIT;

    // Enable timer2 overflow interrupt
    TIMSK |= _BV(TOIE2);

    // Disable the timer until it is needed
    TCCR2 = 0x00;

    // Enable pullup resistor
    PORTE |= _BV(PE4);
}

/*
 * Check the status of the not-scan output on the camera
 * LOW indicates that the camera is currently downloading a frame, or is not undertaking an exposure sequence
 * HIGH indicates that the camera is actively exposing and is safe to disable
 *
 * The signal is noisy when LOW, so wait for 0.5ms and double-check
 * that the level has indeed changed before acting
 */
void monitor_tick()
{
    if (!debounce_waiting && monitor_level_high != bit_is_clear(PINE, PE4))
    {
        debounce_waiting = TRUE;

        // Set the prescaler to 1/1024; each tick = 64us.
        // Also starts the timer counting
        TCCR2 = _BV(CS02)|_BV(CS00);

        // Set timer2 to overflow after 8 ticks (0.512 ms)
        TCNT2 = 248;
    }
}

/*
 * timer2 overflow interrupt
 * Checks whether the level is still changed, and triggers
 * the appropriate actions for the level change.
 */
SIGNAL(SIG_OVERFLOW2)
{
    debounce_waiting = FALSE;
    unsigned char high = bit_is_clear(PINE, PE4);
    if (monitor_level_high != high)
    {
        monitor_level_high = high;

        if (monitor_level_high)
        {
            switch (monitor_mode)
            {
                case MONITOR_START:
                    // Start the timer countdown
                    countdown_mode = COUNTDOWN_SYNCING;

                    monitor_mode = MONITOR_ACQUIRE;
                    send_debug_string("Sequence initialized");
                break;
                case MONITOR_ACQUIRE:
                    send_downloadcomplete();
                break;
                case MONITOR_STOP:
                    send_stopexposure();
                    monitor_mode = MONITOR_WAIT;
                break;
            }
        }
    }

    // Disable timer
    TCCR2 = 0x00;
}
