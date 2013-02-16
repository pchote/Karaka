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
#include <util/atomic.h>

#include "main.h"
#include "gps.h"
#include "display.h"
#include "usb.h"
#include "camera.h"

const char msg_duplicate_pulse[] PROGMEM = "WARNING: Missed serial data or duplicate time pulse";
const char msg_missing_pulse[]   PROGMEM = "WARNING: Missed time pulse";
const char fmt_time_drift[]      PROGMEM = "WARNING: %dms time drift";

// Internal timing mode
//    MODE_PULSECOUNTER counts the 1Hz input signal and
//       flags the next time packet as the download time
//    MODE_HIGHRES uses the 1Hz signal for initial alignment
//       and stability checks, but all timing is tracked
//       from the hardware timers (assumes stable CPU clock)
uint8_t timing_mode = MODE_PULSECOUNTER;

uint16_t exposure_total = 0;
uint8_t align_boundary = 0;

volatile uint16_t exposure_countdown = 0;
volatile enum interrupt_flags interrupt_flags = 0;
volatile enum timer_status timer_status = TIMER_IDLE;
volatile enum gps_status gps_status = GPS_UNAVAILABLE;

enum gps_last_data {GPS_UNKNOWN, GPS_PULSE, GPS_SERIAL};
volatile enum gps_last_data gps_last_data = GPS_UNKNOWN;

inline void set_timer_status(enum timer_status status)
{
    timer_status = status;
    interrupt_flags |= FLAG_SEND_STATUS;
}

inline void set_gps_status(enum gps_status status)
{
    gps_status = status;
    interrupt_flags |= FLAG_SEND_STATUS;
}

// Internal millisecond count
// Remains zero if timing_mode == MODE_PULSECOUNTER
volatile uint16_t millisecond_count = 0;
volatile int16_t millisecond_drift = 0;
volatile struct timestamp download_timestamp;
volatile bool record_trigger = false;

struct timestamp current_timestamp;

int main(void)
{
    // Enable pin change interrupt for pulse input
    PCMSK3 |= _BV(PCINT28);
    PCICR |= _BV(PCIE3);

    // Enable pullup resistor on unused pins
    PORTA = 0xFF;

    // Set millisecond-timer period to 1ms
    OCR1A = 9999;
    STOP_MILLISECOND_TIMER;
    TIMSK1 |= _BV(OCIE1A);

    // Set other init
    usb_initialize();
    camera_initialize();
    display_init();

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

            if (temp_int_flags & FLAG_DUPLICATE_PULSE)
                usb_send_message_P(msg_duplicate_pulse);

            if (temp_int_flags & FLAG_MISSING_PULSE)
                usb_send_message_P(msg_missing_pulse);

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
 * Fired on any level change from the pulse input (PD4)
 */
ISR(PCINT3_vect)
{
    // Trigger on the falling edge of the GPS pulse
    // Note that the input buffer inverts the signal
    //
    // Triggering on the rising edge is unreliable as
    // this check will fail of other interrupts delay
    // the interrupt by >10us.
    // This is needed for millisecond-mode as the timer
    // interrupt fires at the same time as the pulse arrives.
    if (bit_is_clear(PIND, PD4))
        return;

    switch (timer_status)
    {
        case TIMER_EXPOSING:
        case TIMER_READOUT:
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
            }
            else
            {
                // End of exposure - send a trigger to the camera
                // and store a flag so the gps can save the synctime.
                // This is a 16-bit operation, but we are in an interrupt so it is atomic
                if (--exposure_countdown == 0)
                {
                    camera_trigger_readout();
                    exposure_countdown = exposure_total;
                    record_trigger = true;
                }
            }
            break;
        case TIMER_ALIGN:
            // Start the first exposure so that a (potentially future) exposure
            // boundary will occur on the minute
            if (current_timestamp.seconds % align_boundary != align_boundary - 1)
                break;

            set_timer_status(TIMER_EXPOSING);
            if (timing_mode == MODE_HIGHRES)
            {
                // Enable the millisecond timer to begin sending triggers
                // Start timer with an initial count to align trigger as close to
                // the pulse as possible. This figure is determined experimentally
                // by comparing the GPS and trigger pulses with an oscilloscope
                //
                // Constants for configuring the millisecond timer
                // MILLISECOND_TCNT is calibrated with an oscilloscope
                // to minimize the offset between 1Hz signal and triggers
                TCNT1 = 254;
                START_MILLISECOND_TIMER;
            }
            else
            {
                camera_trigger_readout();
                exposure_countdown = exposure_total;
                record_trigger = true;
            }
            break;
        case TIMER_RELAY:
            camera_trigger_readout();
            // Skip pulse/timestamp check
            return;
        case TIMER_WAITING:
        case TIMER_IDLE:
            // Do nothing
            break;
    }

    // Send a warning about the duplicate pulse
    if (gps_last_data == GPS_PULSE)
        interrupt_flags |= FLAG_DUPLICATE_PULSE;
    gps_last_data = GPS_PULSE;
}

void set_time(struct timestamp *t)
{
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
        current_timestamp = *t;
    }
    interrupt_flags |= FLAG_SEND_TIMESTAMP;

    if (gps_status != GPS_ACTIVE)
        set_gps_status(GPS_ACTIVE);

    if (timing_mode == MODE_PULSECOUNTER && record_trigger)
    {
        download_timestamp = current_timestamp;
        record_trigger = false;
        interrupt_flags |= FLAG_SEND_TRIGGER;
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

    // Send a warning about the missing pulse
    if (gps_last_data == GPS_SERIAL && timer_status != TIMER_RELAY)
        interrupt_flags |= FLAG_MISSING_PULSE;
    gps_last_data = GPS_SERIAL;
}
