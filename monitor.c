//***************************************************************************
//
//  File        : monitor.c
//  Copyright   : 2011-2012 Paul Chote
//  Description : Monitors the level of the cameras _SCAN_ output connected to PE4
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include "monitor.h"
#include "main.h"
#include "command.h"

static volatile char debounce_waiting;

volatile uint8_t monitor_level_high = FALSE;
volatile uint8_t monitor_mode = MONITOR_WAIT;

/*
 * (re-)Set initial monitor state
 */
void monitor_init_state()
{
    debounce_waiting = FALSE;
    monitor_level_high = FALSE;
    monitor_mode = MONITOR_WAIT;
}

/*
 * Initialize camera state input and timer0 for debouncing input
 */
void monitor_init_hardware()
{
    // Enable timer2 overflow interrupt
    TIMSK2 |= _BV(TOIE2);

    // Disable the timer until it is needed
    TCCR2B = 0x00;

    // Enable pullup resistor on monitor input
    PORTD |= _BV(PD6);
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
    if (!debounce_waiting && monitor_level_high != bit_is_clear(PIND, PD6))
    {
        debounce_waiting = TRUE;

        // Set the prescaler to 1/1024; each tick = 64us.
        // Also starts the timer counting
        TCCR2B = _BV(CS02)|_BV(CS01)|_BV(CS00);

        // Set timer2 to overflow after 8 ticks (0.512 ms)
        TCNT2 = 248;
    }
}

/*
 * timer2 overflow interrupt
 * Checks whether the level is still changed, and triggers
 * the appropriate actions for the level change.
 */
ISR(TIMER2_OVF_vect)
{
    debounce_waiting = FALSE;
    uint8_t high = bit_is_clear(PIND, PD6);
    if (monitor_level_high != high)
    {
        monitor_level_high = high;

        if (monitor_level_high)
        {
            switch (monitor_mode)
            {
                case MONITOR_START:
                    countdown_mode = COUNTDOWN_SYNCING;
                    monitor_mode = MONITOR_ACQUIRE;
                break;
                case MONITOR_ACQUIRE:
                    interrupt_flags |= FLAG_DOWNLOAD_COMPLETE;
                break;
                case MONITOR_STOP:
                    interrupt_flags |= FLAG_STOP_EXPOSURE;
                    monitor_mode = MONITOR_WAIT;
                break;
            }
        }
    }

    // Disable timer
    TCCR2B = 0x00;
}
