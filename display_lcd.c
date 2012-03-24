//***************************************************************************
//
//  File        : display_lcd.c
//  Copyright   : 2012 Paul Chote
//  Description : LCD display routines
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include "main.h"

#if CPU_TYPE != CPU_ATMEGA128
#   error LCD display only supported by atmega128 board
#endif

#include "display.h"
#include "command.h"
#include "gps.h"
#include "monitor.h"

#include <stdbool.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <util/delay.h>
#include <util/atomic.h>

#define DISPLAY0 0x02
#define DISPLAY1 0xC0

// Display messages
const char display_msg_idle[]            PROGMEM = "      IDLE      ";
const char display_msg_wait[]            PROGMEM = " WAIT FOR CAMERA";
const char display_fmt_align[]           PROGMEM = " ALIGN   %03d/%03d";
const char display_fmt_expose[]          PROGMEM = " EXPOSE  %03d/%03d";
const char display_fmt_expose_percent[]  PROGMEM = " EXPOSE    %3d%% ";
const char display_msg_expose_blank[]    PROGMEM = "     EXPOSE     ";
const char display_fmt_readout[]         PROGMEM = " READOUT %03d/%03d";
const char display_fmt_readout_percent[] PROGMEM = " READOUT   %3d%% ";
const char display_msg_readout_blank[]   PROGMEM = "     READOUT    ";

const char display_fmt_time[]        PROGMEM = "  %02d:%02d:%02d UTC  ";
const char display_fmt_time_nolock[] PROGMEM = "%02d:%02d:%02d NO LOCK";
const char display_msg_noserial[]    PROGMEM = "    NO SERIAL   ";
const char display_msg_syncing[]     PROGMEM = "     SYNCING    ";

uint8_t display_exposure_type = DISPLAY_EXPOSURE_REGULAR;

/*
 * Queue data to the display
 */
static void send_byte(uint8_t b, bool is_data)
{
    // Load the character into the data output
    PORTC = b;
    
    // Select register
    if (is_data)
        PORTF |= _BV(PF0);
    else
        PORTF &= ~_BV(PF0);

    // Wait for >tAS = 140ns
    _delay_us(1);

    // Toggle enable bit to trigger the download
    PORTF |= _BV(PF2);
    _delay_us(40);
    PORTF &= ~_BV(PF2);
}

/*
 * Set a 16 character message on the requested display module
 * Assumes that msg is at least 10 bytes, all in the range 0x20 - 0x7F
 */
static void set_msg_P(uint8_t display, const char *msg)
{
    // Select display
    send_byte(display, false);
    _delay_us(700);

    // Iterate over the characters in the message
    for (uint8_t i = 0; i < 16; i++)
        send_byte(pgm_read_byte(&msg[i]), true);
}

static void set_fmt_P(uint8_t display, const char *fmt, ...)
{
    // Select display
    send_byte(display, false);

    // Read message into ram
    va_list args;
    char buf[17];
    va_start(args, fmt);
    vsprintf_P(buf, fmt, args);
    va_end(args);

    // Iterate over characters in the message
    for (uint8_t i = 0; i < 16; i++)
        send_byte(buf[i], true);
}

/*
 * Initialise the LCD
 */
void display_init()
{
    // Set all of PORTC as data output
    DDRC |= 0xFF;

    // Set status pins as output
    DDRF |= _BV(PF0)|_BV(PF1)|_BV(PF2);

    // Initialize with 8 bit data, 2 lines, 5x7 font
    send_byte(0x38, false);
    _delay_ms(4.1);
    send_byte(0x0C, false); // Hide cursor
    send_byte(0x01, false); // Clear display
    _delay_ms(1.64);
}

void update_display()
{
    // Update the display every second or when the GPS state changes
    static uint8_t display_gps_seconds = 255; // Bogus value to force redraw on first run
    static uint8_t display_gps_state = GPS_UNAVAILABLE;
    if (display_gps_state == gps_state && display_gps_seconds == gps_last_timestamp.seconds)
        return;
    
    display_gps_state = gps_state;
    display_gps_seconds = gps_last_timestamp.seconds;
    
    // Take a local copy of state that can be changed by interrupts
    uint8_t display_monitor_mode = monitor_mode;
    uint8_t display_countdown_mode = countdown_mode;
    uint8_t display_monitor_level_high = monitor_level_high;

    uint16_t display_countdown;
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
        display_countdown = exposure_countdown;
    }

    // Update top row (status and countdown)
    if (display_monitor_mode == MONITOR_START || display_monitor_mode == MONITOR_STOP)
        set_msg_P(DISPLAY0, display_msg_wait);

    else if (display_countdown_mode == COUNTDOWN_SYNCING || display_countdown_mode == COUNTDOWN_ALIGNED || display_monitor_mode == MONITOR_ACQUIRE)
    {
        if (display_countdown_mode == COUNTDOWN_SYNCING || display_countdown_mode == COUNTDOWN_ALIGNED)
            set_fmt_P(DISPLAY0, display_fmt_align, gps_last_timestamp.seconds % align_boundary, align_boundary);
        else
        {
            switch (display_exposure_type)
            {
                case DISPLAY_EXPOSURE_HIDE:
                {
                    PGM_P msg = (!display_monitor_level_high) ? display_msg_readout_blank : display_msg_expose_blank;
                    set_msg_P(DISPLAY0, msg);
                    break;
                }
                case DISPLAY_EXPOSURE_PERCENT:
                {
                    PGM_P msg = (!display_monitor_level_high) ? display_fmt_readout_percent : display_fmt_expose_percent;
                    uint16_t percentage = (exposure_total - display_countdown) / (exposure_total / 100);
                    set_fmt_P(DISPLAY0, msg, percentage);
                    break;
                }
                default:
                case DISPLAY_EXPOSURE_REGULAR:
                {
                    PGM_P msg = (!display_monitor_level_high) ? display_fmt_readout : display_fmt_expose;
                    if (timing_mode == MODE_HIGHRES)
                        set_fmt_P(DISPLAY0, msg, (exposure_total - display_countdown) / 1000, display_total / 1000);
                    else
                        set_fmt_P(DISPLAY0, msg, exposure_total - display_countdown, exposure_total);

                    break;
                }
            }
        }
    }
    else
        set_msg_P(DISPLAY0, display_msg_idle);

    // Update bottom row (time and locked state)
    switch (display_gps_state)
    {
        case GPS_ACTIVE:
                set_fmt_P(DISPLAY1, (gps_last_timestamp.locked ? display_fmt_time : display_fmt_time_nolock),
                          gps_last_timestamp.hours,
                          gps_last_timestamp.minutes,
                          gps_last_timestamp.seconds);
            break;
        case GPS_SYNCING:
            set_msg_P(DISPLAY1, display_msg_syncing);
            break;
        case GPS_UNAVAILABLE:
            set_msg_P(DISPLAY1, display_msg_noserial);
            break;
    }
}
