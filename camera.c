//***************************************************************************
//
//  File        : camera.c
//  Copyright   : 2013 Paul Chote
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
#include "camera.h"
#include "main.h"

enum monitor_mode
{
    MONITOR_WAIT    = 0,
    MONITOR_START   = 1,
    MONITOR_ACQUIRE = 2,
    MONITOR_STOP    = 3
};
volatile enum monitor_mode monitor_mode = MONITOR_WAIT;

enum camera_status {CAMERA_READY, CAMERA_BUSY};
volatile enum camera_status camera_status = CAMERA_READY;
bool monitor_camera_status = true;

// 6.71 seconds
#define SIMULATED_STARTUP 0xFFFF

// 1 second (?)
#define SIMULATED_READOUT 0x7A11

#define SIMULATED_SHUTDOWN 0x2625

void camera_initialize()
{
    // Timer0 is used to control the download pulse length
    // Timer3 is used to control the simulated camera response

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

    TIMSK3 |= _BV(OCIE3A);

    // Enable pullup resistor on monitor input
    PORTD |= _BV(PD6);

    // Disable timers
    TCCR3B = _BV(WGM32);

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
void camera_tick()
{
    if (!monitor_camera_status)
        return;

    enum camera_status status = bit_is_clear(PIND, PD6) ? CAMERA_READY : CAMERA_BUSY;
    if (camera_status != status)
    {
        OCR3A = 5;
        TCCR3B = _BV(WGM32) | _BV(CS32) | _BV(CS30);
    }
}

/*
 * Set the simulated camera status pin low
 * The I/O buffers invert the logic level, so set output high to simulate input low
 */
static void simulate_camera_busy(uint16_t timer_cnt)
{
    camera_status = CAMERA_BUSY;

    OCR3A = timer_cnt;
    TCCR3B = _BV(WGM32) | _BV(CS32) | _BV(CS30);
}

// Acquisition program wants to start exposing
// Wait until the camera is ready to expose, then
// set a flag to notify that we can begin sending triggers
void camera_start_exposing(bool monitor_status)
{
    monitor_camera_status = monitor_status;

    monitor_mode = MONITOR_START;

    // Wait for 6.71 seconds to simulate camera initialization
    if (!monitor_camera_status)
        simulate_camera_busy(SIMULATED_STARTUP);

    set_timer_status(TIMER_WAITING);
}

// Acquisition program wants to stop exposing
// Wait until the camera has finished exposing,
// then send a flag to notify that the acquisition PC
// can stop the exposure sequence
//
// This is necessary to work around a serious bug in the
// PVCAM camera library that is triggered when an
// exposure-stop command is sent while reading out
void camera_stop_exposing()
{
    // Camera is not exposing - can stop immediately
    if (camera_status == CAMERA_READY)
    {
        monitor_mode = MONITOR_WAIT;
        interrupt_flags |= FLAG_STOP_EXPOSURE;

        set_timer_status(TIMER_IDLE);
    }
    else
    {
        // Wait until the readout is complete
        monitor_mode = MONITOR_STOP;
        set_timer_status(TIMER_WAITING);

        if (!monitor_camera_status)
            simulate_camera_busy(SIMULATED_SHUTDOWN);
    }
}

// Act on status change after a debounce period (if monitoring status)
// or fixed delay (if simulated status)
ISR(TIMER3_COMPA_vect)
{
    // Disable timer
    TCCR3B = _BV(WGM32);

    enum camera_status status = !monitor_camera_status || bit_is_clear(PIND, PD6) ? CAMERA_READY : CAMERA_BUSY;
    if (camera_status != status)
    {
        camera_status = status;

        switch (monitor_mode)
        {
            case MONITOR_START:
                monitor_mode = MONITOR_ACQUIRE;
                set_timer_status(TIMER_ALIGN);
                break;
            case MONITOR_ACQUIRE:
                if (status == CAMERA_BUSY)
                    set_timer_status(TIMER_READOUT);
                else
                    set_timer_status(TIMER_EXPOSING);
                break;
            case MONITOR_STOP:
                interrupt_flags |= FLAG_STOP_EXPOSURE;
                set_timer_status(TIMER_IDLE);
                break;
            default:
                break;
        }
    }
}

// Start a camera readout by pulling the output line low
void camera_trigger_readout()
{
    PORTD |= _BV(PD5);
    TCCR0B = _BV(CS01) | _BV(CS00);

    // Trigger a fake download
    if (!monitor_camera_status)
    {
        set_timer_status(TIMER_READOUT);
        simulate_camera_busy(SIMULATED_READOUT);
    }
}

// End readout trigger by pulling output high
ISR(TIMER0_COMPA_vect)
{
    TCCR0B = 0;
    PORTD &= ~_BV(PD5);
}

