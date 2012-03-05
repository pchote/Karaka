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

/*
 * Monitor the status output on the camera (configured to output high when reading out)
 * in order to work around a corruption bug in the camera firmware/libraries
 *
 * If simulation is enabled, the behavior of the status line will be controlled internally
 * using software interrupts. This allows for testing without a camera present and supporting
 * the original timer hardware which lacks the monitor input.
 */

#include <avr/interrupt.h>
#include "monitor.h"
#include "main.h"
#include "command.h"

#if HARDWARE_VERSION < 3
    #define MONITOR_PORT PORTE
    #define MONITOR_PINREG PINE
    #define MONITOR_PIN PE4
    #define MONITOR_DDR DDRE
    #define MONITOR_DD DDE4

    #define MONITOR_TIMSK TIMSK
    #define MONITOR_TCCR TCCR2
    #define MONITOR_TIMER_SCALE _BV(CS02)|_BV(CS00)

    #define SIMULATE_TIMSK ETIMSK
#else
    #define MONITOR_PORT PORTD
    #define MONITOR_PINREG PIND
    #define MONITOR_PIN PD6
    #define MONITOR_DDR DDRD
    #define MONITOR_DD DDD6

    #define MONITOR_TIMSK TIMSK2
    #define MONITOR_TCCR TCCR2B
    #define MONITOR_TIMER_SCALE _BV(CS02)|_BV(CS01)|_BV(CS00);

    #define SIMULATE_TIMSK TIMSK3

#endif

bool monitor_simulate_camera = false;
static volatile bool debounce_waiting = false;

volatile bool monitor_level_high = false;
volatile monitorstate monitor_mode = MONITOR_WAIT;

/*
 * (re-)Set initial monitor state
 */
void monitor_init_state()
{
    debounce_waiting = false;
    monitor_level_high = false;
    monitor_mode = MONITOR_WAIT;

    // Disable timers
    TCCR3B = 0;
    MONITOR_TCCR = 0;

    simulate_camera_enable(false);
}

/*
 * Initialize camera state input and timer0 for debouncing input
 */
void monitor_init_hardware()
{
    // Enable timer2 overflow interrupt
    MONITOR_TIMSK |= _BV(TOIE2);
    SIMULATE_TIMSK |= _BV(TOIE3);

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
        debounce_waiting = true;

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
    debounce_waiting = false;
    bool high = bit_is_clear(MONITOR_PINREG, MONITOR_PIN);
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
                default:
                break;
            }
        }
    }

    // Disable timer
    MONITOR_TCCR = 0;
}

void simulate_camera_enable(bool enabled)
{
    monitor_simulate_camera = enabled;

    // Set monitor pin as I/O
    if (enabled)
    {
        // Set output
        MONITOR_DDR |= _BV(MONITOR_DD);

        // Set output high
        MONITOR_PORT &= ~_BV(MONITOR_PIN);
    }
    else
    {
        // Set input
        MONITOR_DDR &= ~_BV(MONITOR_DD);

        // Enable Pullup resistor
        MONITOR_PORT |= _BV(MONITOR_PIN);
    }
}

/*
 * Set the simulated camera status pin low
 * The I/O buffers invert the logic level, so set output high to simulate input low
 */
static void set_output_low(uint16_t timer_cnt)
{
    MONITOR_PORT |= _BV(MONITOR_PIN);

    cli();
    // Define timer tick as 64us
    TCCR3B = _BV(CS30)|_BV(CS32);
    TCNT3 = timer_cnt;
    sei();
}

/*
 * Set output high, delay for 4.19 seconds, then low
 */
void simulate_camera_startup()
{
    set_output_low(0x0000);
}

/*
 * Set output low, delay for 1 second, then high
 */
void simulate_camera_shutdown()
{
    set_output_low(0xC2F6);
}

/*
 * Set output high, delay for 3.2 seconds, then low
 */
void simulate_camera_download()
{
    set_output_low(0x3CAF);
}

/*
 * Timer3 overflow interrupt
 * Return monitor level to high and disable timer
 */
ISR(TIMER3_OVF_vect)
{
    MONITOR_PORT &= ~_BV(MONITOR_PIN);
    TCCR3B = 0;
}
