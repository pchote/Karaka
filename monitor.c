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

#if HARDWARE_VERSION < 3
    #define MONITOR_TIMSK TIMSK
    #define MONITOR_TCCR TCCR2
    #define MONITOR_PORT PORTE
    #define MONITOR_PINREG PINE
    #define MONITOR_PIN PE4

    #define MONITOR_TIMER_SCALE _BV(CS02)|_BV(CS00)
#else
    #define MONITOR_TIMSK TIMSK2
    #define MONITOR_TCCR TCCR2B
    #define MONITOR_PORT PORTD
    #define MONITOR_PINREG PIND
    #define MONITOR_PIN PD6

    #define MONITOR_TIMER_SCALE _BV(CS02)|_BV(CS01)|_BV(CS00);
#endif

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
    MONITOR_TIMSK |= _BV(TOIE2);

    // Disable the timer until it is needed
    MONITOR_TCCR = 0x00;

    // Enable pullup resistor on monitor input
    MONITOR_PORT |= _BV(MONITOR_PIN);
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
    if (!debounce_waiting && monitor_level_high != bit_is_clear(MONITOR_PINREG, MONITOR_PIN))
    {
        debounce_waiting = TRUE;

        // Set the prescaler to 1/1024; each tick = 64us.
        // Also starts the timer counting
        MONITOR_TCCR = MONITOR_TIMER_SCALE;

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
    uint8_t high = bit_is_clear(MONITOR_PINREG, MONITOR_PIN);
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
    MONITOR_TCCR = 0;
}
