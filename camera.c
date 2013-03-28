//***************************************************************************
//
//  File        : camera.c
//  Copyright   : 2013 Paul Chote
//  Description : Sends trigger pulses and monitors the camera logic output
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include "camera.h"
#include "main.h"

enum monitor_mode {MONITOR_IDLE, MONITOR_START, MONITOR_ACQUIRING, MONITOR_STOP};
volatile enum monitor_mode monitor_mode = MONITOR_IDLE;

enum camera_status {CAMERA_READY, CAMERA_BUSY};
volatile enum camera_status camera_status = CAMERA_READY;
bool monitor_camera_status = true;

// 6.71 seconds
#define SIMULATED_STARTUP 0xFFFF

// 3.2 seconds
#define SIMULATED_READOUT 0x7A11

// 1 second
#define SIMULATED_SHUTDOWN 0x2625

void camera_initialize()
{
    // Set pin as an output, initially high
    DDRD |= _BV(DDD5);
    PORTD &= ~_BV(PD5);

    // Enable pullup resistor on monitor input
    PORTD |= _BV(PD6);

    // Set trigger length to 512us
    // and disable until it is needed
    OCR0A = 79;
    TCCR0A = _BV(WGM01);
    TIMSK0 |= _BV(OCIE0A);
    TCCR0B = 0;

    // Debounce timeout / simulated camera delay
    TIMSK3 |= _BV(OCIE3A);
    TCCR3B = _BV(WGM32);
}

/*
 * Poll the logic output of the camera, and trigger an action after a debounce period
 * Logic LOW indicates that the camera is currently downloading a frame, or is not undertaking an exposure sequence
 * Logic HIGH indicates that the camera is not reading out, and is safe to disable
 *
 *   MONITOR_START: Align exposure period to time boundary and begin exposing
 *   MONITOR_ACQUIRE: Send a "Download Complete" notification to the acquisition PC
 *   MONITOR_STOP: Send a notification to the acquisition PC that it is safe to halt an aquisition sequence
 *
 */
void camera_tick()
{
    if (!monitor_camera_status)
        return;

    enum camera_status status = bit_is_clear(PIND, PD6) ? CAMERA_READY : CAMERA_BUSY;
    if (camera_status != status)
    {
        OCR3A = 5; // 0.512ms
        TCCR3B = _BV(WGM32) | _BV(CS32) | _BV(CS30);
    }
}

static void simulate_camera_busy(uint16_t timer_cnt)
{
    camera_status = CAMERA_BUSY;

    OCR3A = timer_cnt;
    TCCR3B = _BV(WGM32) | _BV(CS32) | _BV(CS30);
}

// Acquisition program wants to start exposing
// Set monitor mode and wait for the camera to
// become ready to accept triggers
//
// If monitor_camera is false, the camera logic will be
// simulated internally using fixed delays
void camera_start_exposing(bool monitor_camera)
{
    monitor_camera_status = monitor_camera;
    monitor_mode = MONITOR_START;

    if (!monitor_camera_status)
        simulate_camera_busy(SIMULATED_STARTUP);

    set_timer_status(TIMER_WAITING);
}

// Acquisition program wants to stop exposing
// Set monitor mode and wait for the camera to
// finish exposing
//
// This is necessary to work around a serious bug in the
// PVCAM camera library that is triggered when an
// exposure-stop command is sent while reading out
void camera_stop_exposing()
{
    if (camera_status == CAMERA_READY)
    {
        // Camera is not exposing - can stop immediately
        monitor_mode = MONITOR_IDLE;
        message_flags |= FLAG_STOP_EXPOSURE;

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
// or a fixed delay (if simulated status)
ISR(TIMER3_COMPA_vect)
{
    // Disable timer
    TCCR3B = _BV(WGM32);

    enum camera_status status = !monitor_camera_status || bit_is_clear(PIND, PD6) ? CAMERA_READY : CAMERA_BUSY;
    if (camera_status == status)
        return;

    camera_status = status;
    switch (monitor_mode)
    {
        case MONITOR_START:
            monitor_mode = MONITOR_ACQUIRING;
            set_timer_status(TIMER_ALIGN);
            break;
        case MONITOR_ACQUIRING:
            // Suppress status updates for exposures < 500ms
            if (timing_mode == MODE_HIGHRES && exposure_total < 500)
                break;
            if (status == CAMERA_BUSY)
                set_timer_status(TIMER_READOUT);
            else
                set_timer_status(TIMER_EXPOSING);
            break;
        case MONITOR_STOP:
            message_flags |= FLAG_STOP_EXPOSURE;
            set_timer_status(TIMER_IDLE);
            break;
        default:
            break;
    }
}

// Start a camera readout by pulling the output line low
void camera_trigger_readout()
{
    PORTD |= _BV(PD5);
    TCCR0B = _BV(CS01) | _BV(CS00);

    // Suppress status updates for exposures < 500ms
    if (timing_mode == MODE_HIGHRES && exposure_total < 500)
        return;

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

