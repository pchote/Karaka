//***************************************************************************
//
//  File        : main.c
//  Copyright   : 2009-2012 Johnny McClymont, Paul Chote
//  Description : Main program logic
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/atomic.h>

#include "main.h"
#include "gps.h"
#include "display.h"
#include "usb.h"
#include "camera.h"

const char msg_duplicate_pulse[] PROGMEM = "Duplicate PPS pulse detected";
const char msg_missed_pps[]      PROGMEM = "Missing PPS pulse detected";
const char fmt_time_drift[]      PROGMEM = "WARNING: %dms time drift";

// Internal timing mode
//    MODE_PPSCOUNTER counts the 1Hz input signal and
//       flags the next time packet as the download time
//    MODE_HIGHRES uses the 1Hz signal for initial alignment
//       and stability checks, but all timing is tracked
//       from the hardware timers (assumes stable CPU clock)
uint8_t timing_mode = MODE_PPSCOUNTER;

uint16_t exposure_total = 0;
uint8_t align_boundary = 0;
volatile uint16_t exposure_countdown = 0;
volatile countdownstate countdown_mode = COUNTDOWN_DISABLED;
volatile interruptflags interrupt_flags = 0;

volatile enum timer_status timer_status = TIMER_IDLE;
volatile enum gps_status gps_status = GPS_UNAVAILABLE;

inline void set_timer_status(enum timer_status status)
{
    timer_status = status;
    interrupt_flags |= FLAG_SEND_STATUS;
}

inline void set_gps_status(enum gps_status status)
{
    enum gps_status old = gps_status;
    gps_status = status;

    if (old != gps_status)
        interrupt_flags |= FLAG_SEND_STATUS;
}

// Internal millisecond count
// Remains zero if timing_mode == MODE_PPSCOUNTER
volatile uint16_t millisecond_count = 0;
volatile int16_t millisecond_drift = 0;
volatile struct timestamp download_timestamp;
volatile bool record_trigger = false;

struct timestamp current_timestamp;

// Constants for configuring the millisecond timer
// MILLISECOND_TCNT is calibrated with an oscilloscope
// to minimize the offset between 1Hz signal and triggers
#define MILLISECOND_OCR 9999
#define MILLISECOND_TCNT 254

int main(void)
{
    // Enable pin change interrupt for PPS input
    PCMSK3 |= _BV(PCINT28);
    PCICR |= _BV(PCIE3);

    // Enable pullup resistor on unused pins
    PORTA = 0xFF;

    // Enable output compare interrupt for ms timer
    TIMSK1 |= _BV(OCIE1A);
    OCR1A = MILLISECOND_OCR;
    STOP_MILLISECOND_TIMER;

    // Set other init
    usb_initialize();
    camera_initialize();
    display_init();

    exposure_total = exposure_countdown = 0;
    millisecond_count = millisecond_drift = 0;
    countdown_mode = COUNTDOWN_DISABLED;
    interrupt_flags = 0;

	// Enable relay mode until reboot
	if (eeprom_read_byte(RELAY_EEPROM_OFFSET) == RELAY_ENABLED)
	{
        set_timer_status(TIMER_RELAY);
		eeprom_update_byte(RELAY_EEPROM_OFFSET, RELAY_DISABLED);
	}

    // Enable interrupts
    sei();
    gps_initialize();

    // Main program loop
    for (;;)
    {
        // Handle message flags set via interrupt
        if (interrupt_flags)
        {
            uint8_t temp_int_flags = 0;
            ATOMIC_BLOCK(ATOMIC_FORCEON)
            {
                temp_int_flags = interrupt_flags;
                interrupt_flags = 0;
            }

            if (temp_int_flags & FLAG_SEND_TRIGGER)
                usb_send_trigger();

            if (temp_int_flags & FLAG_SEND_TIMESTAMP)
                usb_send_timestamp();

            if (temp_int_flags & FLAG_SEND_STATUS)
                usb_send_status(timer_status, gps_status);

            if (temp_int_flags & FLAG_STOP_EXPOSURE)
                usb_stop_exposure();

            if (temp_int_flags & FLAG_DUPLICATE_PPS)
                usb_send_message_P(msg_duplicate_pulse);

            if (temp_int_flags & FLAG_TIME_DRIFT)
                usb_send_message_fmt_P(fmt_time_drift, millisecond_drift);
        }

        camera_tick();
        usb_tick();
        gps_tick();
        display_update();
    }
}

/*
 * Millisecond timer interrupt handler
 * Fired every 1ms when timing_mode == MODE_HIGHRES
 */
ISR(TIMER1_COMPA_vect)
{
    millisecond_count++;

    // End of exposure - send a trigger and save the time
    // This is a 16-bit operation, but we are in an interrupt so it is atomic
    if (--exposure_countdown == 0)
    {
        camera_trigger_readout();
        exposure_countdown = exposure_total;
        download_timestamp = current_timestamp;

        download_timestamp.milliseconds = millisecond_count;
        interrupt_flags |= FLAG_SEND_TRIGGER;
    }
}

/*
 * GPS time pulse interrupt handler
 * Fired on any level change from the PPS input (PD4)
 */
ISR(PCINT3_vect)
{
    // Trigger on the falling edge of the PPS pulse
    // Note that the input buffer inverts the signal
    //
    // Triggering on the rising edge is unreliable as
    // this check will fail of other interrupts delay
    // the interrupt by >10us.
    // This is needed for millisecond-mode as the timer
    // interrupt fires at the same time as the PPS arrives.
    if (bit_is_clear(PIND, PD4))
        return;

    if (timing_mode == MODE_HIGHRES)
    {
        // Test for time drift
        // Doesn't need to be atomic, as interrupts are disabled in an interrupt context
        uint16_t drift = millisecond_count;
        while (drift >= 1000)
            drift -= 1000;

        if (drift != 0)
        {
            millisecond_drift = drift > 500 ? (drift - 1000) : drift;
            interrupt_flags |= FLAG_TIME_DRIFT;
        }

        // Synced to a second boundary on startup
        // Enable the millisecond timer to begin sending triggers
        if (countdown_mode == COUNTDOWN_ALIGNED)
        {
            // Start timer with an initial count to align trigger to PPS as
            // closely as possible. This figure is determined experimentally
            // by comparing the PPS and trigger pulses with an oscilloscope
            TCNT1 = MILLISECOND_TCNT;
            START_MILLISECOND_TIMER;
            countdown_mode = COUNTDOWN_ENABLED;
        }
    }
    else
    {
        // Don't count down unless we have a valid exposure time and the GPS is locked
        if (gps_status == GPS_ACTIVE && timer_status != TIMER_RELAY)
        {
            // Send a warning about the duplicate pulse
            if (countdown_mode == COUNTDOWN_TRIGGERED)
                interrupt_flags |= FLAG_DUPLICATE_PPS;

            if (countdown_mode == COUNTDOWN_ENABLED || countdown_mode == COUNTDOWN_TRIGGERED)
            {
                // End of exposure - send a syncpulse to the camera
                // and store a flag so the gps can save the synctime.
                // This is a 16-bit operation, but we are in an interrupt so it is atomic
                if (--exposure_countdown == 0)
                {
                    camera_trigger_readout();
                    exposure_countdown = exposure_total;
                    record_trigger = true;
                }

                countdown_mode = COUNTDOWN_TRIGGERED;
            }
            else if (countdown_mode == COUNTDOWN_ALIGNED)
            {
                camera_trigger_readout();
                exposure_countdown = exposure_total;
                record_trigger = true;

                countdown_mode = COUNTDOWN_TRIGGERED;
            }
        }
    }

    if (timer_status == TIMER_RELAY)
        camera_trigger_readout();
}

void set_time(struct timestamp *t)
{
    current_timestamp = *t;

    // Mark that we have a valid timestamp
    set_gps_status(GPS_ACTIVE);
    interrupt_flags |= FLAG_SEND_TIMESTAMP;

    // Synchronise the exposure with the edge of a minute
    if (countdown_mode == COUNTDOWN_SYNCING && (current_timestamp.seconds % align_boundary == align_boundary - 1))
        countdown_mode = COUNTDOWN_ALIGNED;

    if (timing_mode == MODE_PPSCOUNTER)
    {
        // Enable the counter for the next PPS pulse
        if (countdown_mode == COUNTDOWN_TRIGGERED)
            countdown_mode = COUNTDOWN_ENABLED;
        else if (countdown_mode == COUNTDOWN_ENABLED)
        {
            // We should always receive the PPS pulse before the time packet
            usb_send_message_P(msg_missed_pps);
        }

        if (record_trigger)
        {
            download_timestamp = current_timestamp;
            record_trigger = false;
            interrupt_flags |= FLAG_SEND_TRIGGER;
        }
    }
    else
    {
        // Reset rollover count
        ATOMIC_BLOCK(ATOMIC_FORCEON)
        {
            while (millisecond_count > 1000)
                millisecond_count -= 1000;
        }
    }
}
