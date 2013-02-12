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

bool monitor_simulate_camera = false;

volatile bool monitor_level_high = false;
volatile monitorstate monitor_mode = MONITOR_WAIT;

/*
 * Initialize camera state input and timer0 for debouncing input
 */
void monitor_init()
{
    TIMSK3 |= _BV(OCIE3A);

    // Enable pullup resistor on monitor input
    PORTD |= _BV(PD6);

    monitor_level_high = false;
    monitor_mode = MONITOR_WAIT;

    // Disable timers
    TCCR3B = _BV(WGM32);

    monitor_simulate_camera = false;
}

/*
 * Poll the logic output of the camera
 * Logic LOW indicates that the camera is currently downloading a frame, or is not undertaking an exposure sequence
 * Logic HIGH indicates that the camera is not reading out, and is safe to disable
 * Polls for the logic level to become high, and then triggers an action after a debounce period
 *
 *   MONITOR_START: Align exposure period to time boundary and begin exposing
 *   MONITOR_ACQUIRE: Send a "Download Complete" notification to the acquisition PC
 *   MONITOR_STOP: Send a notification to the acquisition PC that it is safe to halt an aquisition sequence
 *
 * The signal is noisy when LOW, so wait for 0.5ms and double-check
 * that the level has indeed changed before acting
 */
void monitor_tick()
{
    if (!monitor_simulate_camera &&
        monitor_level_high != bit_is_clear(PIND, PD6))
    {
        OCR3A = 5;
        TCCR3B = _BV(WGM32) | _BV(CS32) | _BV(CS30);
    }
}

/*
 * Set the simulated camera status pin low
 * The I/O buffers invert the logic level, so set output high to simulate input low
 */
static void set_output_low(uint16_t timer_cnt)
{
    monitor_level_high = false;

    OCR3A = timer_cnt;
    TCCR3B = _BV(WGM32) | _BV(CS32) | _BV(CS30);
}

/*
 * Set output high, delay for max time, then low
 */
void simulate_camera_startup()
{
    // 4.19 seconds @ 16MHz
    // 6.71 seconds @ 10MHz
    set_output_low(0xFFFF);
}

/*
 * Set output low, delay for 1 second, then high
 */
void simulate_camera_shutdown()
{
    set_output_low(0x2625);
}

/*
 * Set output high, delay for 3.2 seconds, then low
 */
void simulate_camera_download()
{
    set_output_low(0x7A11);
}

/*
 * Timer3 compare match interrupt
 * Return monitor level to high and disable timer
 */
ISR(TIMER3_COMPA_vect)
{
    // Disable timer
    TCCR3B = _BV(WGM32);

    bool high = monitor_simulate_camera || bit_is_clear(PIND, PD6);
    if (monitor_level_high != high)
    {
        monitor_level_high = high;

        switch (monitor_mode)
        {
            case MONITOR_START:
                countdown_mode = COUNTDOWN_SYNCING;
                monitor_mode = MONITOR_ACQUIRE;
                interrupt_flags |= FLAG_BEGIN_ALIGN;
                break;
            case MONITOR_ACQUIRE:
                // Ignore the download start transition
                if (high)
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
