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

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/atomic.h>

#include "main.h"
#include "download.h"
#include "gps.h"
#include "display.h"
#include "command.h"
#include "monitor.h"

const char msg_duplicate_pulse[] PROGMEM = "Duplicate PPS pulse detected";
const char fmt_time_drift[]      PROGMEM = "WARNING: %dms time drift";
const char msg_no_serial[]       PROGMEM = "GPS serial connection lost";

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

// Internal millisecond count
// Remains zero if timing_mode == MODE_PPSCOUNTER
volatile uint16_t millisecond_count = 0;
volatile int16_t millisecond_drift = 0;
volatile timestamp download_timestamp;

// Constants for configuring the millisecond timer
// MILLISECOND_TCNT is calibrated with an oscilloscope
// to minimize the offset between 1Hz signal and triggers
#define MILLISECOND_OCR 9999
#define MILLISECOND_TCNT 254

/* Hardware usage (ATmega12-15AI) - Hardware versions 1-2:
 * PORTA:
 *    PA0: INTG output
 *    PA1-7: Unused
 * PORTB:
 *    PB0: Force-on (to GPS input buffer)
 *    PB1: SCK (to FT232 chip)
 *    PB2-7: Unused
 * PORTC:
 *    PC0-7: 8-bit data to display
 * PORTD:
 *    PD0: GPS PPS input
 *    PD1: Force-off (to RS232 chip)
 *    PD2: RXD1 (From GPS)
 *    PD3: TXD1 (To GPS)
 *    PD4-7: Unused
 * PORTE:
 *    PE0: RXD0 (From PC)
 *    PE1: TXD0 (To PC)
 *    PE2-3: Unused
 *    PE4: Monitor input (Version 2 only)
 *    PE5-7: Unused
 * PORTF:
 *    PF0: Display register select
 *    PF1: Display read/write select
 *    PF2: Display read/write start
 *    PF3: Unused
 *    PF4-7: TCK/TMS/TDO/TDI (to JTAG header)
 * PORTG:
 *    PG0-2: Unused
 *
 * Timer0: Download pulse length
 * Timer1: Millisecond timer
 * Timer2: GPS serial timeout
 * Timer3: Monitor debounce and simulated level delays
 *
 * Usart0: USB <-> Acquisition PC
 * Usart1: RS232 <-> GPS
 */

/* Hardware usage (ATmega1284p-AU) - Hardware version 3:
 * PORTA:
 *    PA0: Display brightness input (ADC0)
 *    PA1-7: Unused
 * PORTB:
 *    PB0: Force-on (to GPS input buffer)
 *    PB1-4: Display select for writing data to display modules
 *    PB5: MOSI (data output to display modules)
 *    PB6: RS232 off (to GPS input buffer)
 *    PB7: SCK (SPI clock input)
 * PORTC:
 *    PC0-1: Unused
 *    PC2-5: TCK/TMS/TDO/TDI (to JTAG header)
 *    PC6-7: Unused
 * PORTD: 
 *    PD0: RXD0 (From PC)
 *    PD1: TXD0 (To PC)
 *    PD2: RXD1 (From GPS)
 *    PD3: TXD1 (To GPS)
 *    PD4: GPS PPS input
 *    PD5: INTG output
 *    PD6: Monitor input
 *    PD7: Spare output
 *
 * Timer0: Download pulse length
 * Timer1: Millisecond timer
 * Timer2: GPS serial timeout
 * Timer3: Monitor debounce and simulated level delays
 *
 * Usart0: USB <-> Acquisition PC
 * Usart1: RS232 <-> GPS
 */

/*
 * Trigger a hardware restart
 */
void trigger_restart()
{
    cli();
    wdt_enable(WDTO_15MS);
    for(;;);
}

/*
 * Initialize the unit and wait for interrupts.
 */
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
    command_init();
    gps_init();
    download_init();
    monitor_init();
    display_init();

    exposure_total = exposure_countdown = 0;
    millisecond_count = millisecond_drift = 0;
    countdown_mode = COUNTDOWN_DISABLED;
    interrupt_flags = 0;

    // Enable interrupts
    sei();

    // Send config to attached GPS
    // Requires interrupts to be enabled
    gps_configure_gps();

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
            {
                send_downloadtimestamp();
                send_status(TIMER_READOUT);
            }

            if (temp_int_flags & FLAG_SEND_TIMESTAMP)
                send_timestamp();

            if (temp_int_flags & FLAG_DOWNLOAD_COMPLETE)
                send_status(TIMER_EXPOSING);

            if (temp_int_flags & FLAG_BEGIN_ALIGN)
                send_status(TIMER_ALIGN);

            if (temp_int_flags & FLAG_STOP_EXPOSURE)
            {
                send_status(TIMER_IDLE);
                send_stopexposure();
            }

            if (temp_int_flags & FLAG_NO_SERIAL)
                send_debug_string_P(msg_no_serial);

            if (temp_int_flags & FLAG_DUPLICATE_PPS)
                send_debug_string_P(msg_duplicate_pulse);

            if (temp_int_flags & FLAG_TIME_DRIFT)
                send_debug_fmt_P(fmt_time_drift, millisecond_drift);
        }

        monitor_tick();
        usart_process_buffer();
        gps_process_buffer();
        update_display();
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
        trigger_download();
        exposure_countdown = exposure_total;
        download_timestamp = gps_last_timestamp;

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
        if (gps_state == GPS_ACTIVE && countdown_mode != COUNTDOWN_RELAY)
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
                    trigger_download();
                    exposure_countdown = exposure_total;
                    gps_record_trigger = true;
                }

                countdown_mode = COUNTDOWN_TRIGGERED;
            }
            else if (countdown_mode == COUNTDOWN_ALIGNED)
            {
                trigger_download();
                exposure_countdown = exposure_total;
                gps_record_trigger = true;

                countdown_mode = COUNTDOWN_TRIGGERED;
            }
        }
    }

    if (countdown_mode == COUNTDOWN_RELAY)
        trigger_download();
}
