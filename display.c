//***************************************************************************
//
//  File        : display.c
//  Copyright   : 2012 Paul Chote
//  Description : Dot matrix display routines
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include "display.h"
#include "main.h"
#include "command.h"
#include "gps.h"
#include "monitor.h"

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <stdio.h>

// Character data, stored in program memory
const unsigned char char_defs[96][5] PROGMEM = {
    {0x00,0x20,0x40,0x60,0x80}, //   :0x20
    {0x04,0x24,0x44,0x60,0x84}, // ! :0x21
    {0x0A,0x2A,0x40,0x60,0x80}, // " :0x22
    {0x0A,0x3F,0x4A,0x7F,0x8A}, // # :0x23
    {0x0F,0x34,0x4E,0x65,0x9E}, // $ :0x24
    {0x19,0x3A,0x44,0x6B,0x93}, // % :0x25
    {0x08,0x34,0x4D,0x72,0x8D}, // & :0x26
    {0x0C,0x2C,0x44,0x68,0x80}, // ' :0x27
    {0x02,0x24,0x44,0x64,0x82}, // ( :0x28
    {0x08,0x24,0x44,0x64,0x88}, // ) :0x29
    {0x15,0x2E,0x5F,0x6E,0x95}, // * :0x2A
    {0x04,0x24,0x5F,0x64,0x84}, // + :0x2B
    {0x00,0x2C,0x4C,0x64,0x88}, // , :0x2C
    {0x00,0x20,0x5F,0x60,0x80}, // - :0x2D
    {0x00,0x20,0x40,0x6C,0x8C}, // . :0x2E
    {0x01,0x22,0x44,0x68,0x90}, // / :0x2F
    {0x0E,0x33,0x55,0x79,0x8E}, // 0 :0x30
    {0x04,0x2C,0x44,0x64,0x8E}, // 1 :0x31
    {0x1E,0x21,0x46,0x68,0x9F}, // 2 :0x32
    {0x1E,0x21,0x4E,0x61,0x9E}, // 3 :0x33
    {0x06,0x2A,0x5F,0x62,0x82}, // 4 :0x34
    {0x1F,0x30,0x5E,0x61,0x9E}, // 5 :0x35
    {0x06,0x28,0x5E,0x71,0x8E}, // 6 :0x36
    {0x1F,0x22,0x44,0x68,0x88}, // 7 :0x37
    {0x0E,0x31,0x4E,0x71,0x8E}, // 8 :0x38
    {0x0E,0x31,0x4F,0x62,0x8C}, // 9 :0x39
    {0x0C,0x2C,0x40,0x6C,0x8C}, // : :0x3A
    {0x0C,0x20,0x4C,0x64,0x88}, // ; :0x3B
    {0x02,0x24,0x48,0x64,0x82}, // < :0x3C
    {0x00,0x3F,0x40,0x7F,0x80}, // = :0x3D
    {0x08,0x24,0x42,0x64,0x88}, // > :0x3E
    {0x0E,0x31,0x42,0x64,0x88}, // ? :0x3F
    {0x0E,0x35,0x57,0x70,0x8E}, // @ :0x40
    {0x04,0x2A,0x5F,0x71,0x91}, // A :0x41
    {0x1E,0x29,0x4E,0x69,0x9E}, // B :0x42
    {0x0F,0x30,0x50,0x70,0x8F}, // C :0x43
    {0x1E,0x29,0x49,0x69,0x9E}, // D :0x44
    {0x1F,0x30,0x5E,0x70,0x9F}, // E :0x45
    {0x1F,0x30,0x5E,0x70,0x90}, // F :0x46
    {0x0F,0x30,0x53,0x71,0x8F}, // G :0x47
    {0x11,0x31,0x5F,0x71,0x91}, // H :0x48
    {0x0E,0x24,0x44,0x64,0x8E}, // I :0x49
    {0x01,0x21,0x41,0x71,0x8E}, // J :0x4A
    {0x13,0x34,0x58,0x74,0x93}, // K :0x4B
    {0x10,0x30,0x50,0x70,0x9F}, // L :0x4C
    {0x11,0x3B,0x55,0x71,0x91}, // M :0x4D
    {0x11,0x39,0x55,0x73,0x91}, // N :0x4E
    {0x0E,0x31,0x51,0x71,0x8E}, // O :0x4F
    {0x1E,0x31,0x5E,0x70,0x90}, // P :0x50
    {0x0C,0x32,0x56,0x72,0x8D}, // Q :0x51
    {0x1E,0x31,0x5E,0x74,0x92}, // R :0x52
    {0x0F,0x30,0x4E,0x61,0x9E}, // S :0x53
    {0x1F,0x24,0x44,0x64,0x84}, // T :0x54
    {0x11,0x31,0x51,0x71,0x8E}, // U :0x55
    {0x11,0x31,0x51,0x6A,0x84}, // V :0x56
    {0x11,0x31,0x55,0x7B,0x91}, // W :0x57
    {0x11,0x2A,0x44,0x6A,0x91}, // X :0x58
    {0x11,0x2A,0x44,0x64,0x84}, // Y :0x59
    {0x1F,0x22,0x44,0x68,0x9F}, // Z :0x5A
    {0x07,0x24,0x44,0x64,0x87}, // [ :0x5B
    {0x11,0x2A,0x44,0x6E,0x84}, // \ :0x5C
    {0x1C,0x24,0x44,0x64,0x9C}, // ] :0x5D
    {0x04,0x2A,0x51,0x60,0x80}, // ^ :0x5E
    {0x00,0x20,0x40,0x60,0x9F}, // _ :0x5F
    {0x00,0x20,0x40,0x60,0x80}, // ` :0x60
    {0x00,0x2E,0x52,0x72,0x8D}, // a :0x61
    {0x10,0x30,0x5E,0x71,0x9E}, // b :0x62
    {0x00,0x2F,0x50,0x70,0x8F}, // c :0x63
    {0x01,0x21,0x4F,0x71,0x8F}, // d :0x64
    {0x00,0x2E,0x5F,0x70,0x8E}, // e :0x65
    {0x04,0x2A,0x4B,0x7C,0x88}, // f :0x66
    {0x00,0x2F,0x50,0x73,0x8F}, // g :0x67
    {0x10,0x30,0x56,0x79,0x91}, // h :0x68
    {0x04,0x20,0x4C,0x64,0x8E}, // i :0x69
    {0x00,0x26,0x42,0x72,0x8C}, // j :0x6A
    {0x10,0x30,0x56,0x78,0x96}, // k :0x6B
    {0x0C,0x24,0x44,0x64,0x8E}, // l :0x6C
    {0x00,0x2A,0x55,0x71,0x91}, // m :0x6D
    {0x00,0x36,0x59,0x71,0x91}, // n :0x6E
    {0x00,0x2E,0x51,0x71,0x8E}, // o :0x6F
    {0x00,0x3E,0x51,0x7E,0x90}, // p :0x70
    {0x00,0x2F,0x51,0x6F,0x81}, // q :0x71
    {0x00,0x33,0x54,0x78,0x90}, // r :0x72
    {0x00,0x23,0x44,0x62,0x8C}, // s :0x73
    {0x08,0x3C,0x48,0x6A,0x84}, // t :0x74
    {0x00,0x32,0x52,0x72,0x8D}, // u :0x75
    {0x00,0x31,0x51,0x6A,0x84}, // v :0x76
    {0x00,0x31,0x55,0x7B,0x91}, // w :0x77
    {0x00,0x32,0x4C,0x6C,0x92}, // x :0x78
    {0x00,0x31,0x4A,0x64,0x98}, // y :0x79
    {0x00,0x3E,0x44,0x68,0x9E}, // z :0x7A
    {0x06,0x24,0x48,0x64,0x86}, // { :0x7B
    {0x04,0x24,0x40,0x64,0x84}, // | :0x7C
    {0x0C,0x24,0x42,0x64,0x8C}, // } :0x7D
    {0x04,0x22,0x5F,0x62,0x84}, // ->:0x7E
    {0x04,0x3C,0x5F,0x68,0x84}, // <-:0x7F
};

// Display messages
char display_msg_noserial_left[]  PROGMEM = "NO SERIAL ";
char display_msg_noserial_right[] PROGMEM = "CONNECTION";
char display_msg_syncing_left[]   PROGMEM = "  SYNCING ";
char display_msg_syncing_right[]  PROGMEM = "TO SERIAL ";

char display_msg_idle_left[]  PROGMEM = "        ID";
char display_msg_idle_right[] PROGMEM = "LE        ";
char display_msg_align[]      PROGMEM = "  ALIGN   ";
char display_msg_expose[]     PROGMEM = "  EXPOSE  ";
char display_msg_readout[]    PROGMEM = "  READOUT ";
char display_msg_wait_left[]  PROGMEM = " WAITING F";
char display_msg_wait_right[] PROGMEM = "OR CAMERA ";
char display_fmt_countdown[]  PROGMEM = " %03d/%03d  ";

char display_fmt_time_left[]  PROGMEM = "    %02d:%02d:";
char display_fmt_time_right[] PROGMEM = "%02d UTC    ";

char display_fmt_time_nolock_left[]  PROGMEM = "%02d:%02d:%02d N";
char display_fmt_time_nolock_right[] PROGMEM = "O GPS LOCK";

volatile unsigned char display_brightness = 0xF7;

/*
 * Queue data to the display
 */
static void send_data(unsigned char display, unsigned char *data, unsigned char length)
{
    // Push the data to the display via SPI
    // Transmit synchronously for now
    for (unsigned char i = 0; i < length; i++)
    {
        // Toggle load line for the appropriate display
        PORTB &= ~_BV(display);

        // Load data into SPI out
        SPDR = data[i];

        loop_until_bit_is_set(SPSR, SPIF);

        // Return load line to end read
        PORTB |= _BV(display);
    }
}

/*
 * Set a 10 character message on the requested display module
 * Assumes that msg is at least 10 bytes, all in the range 0x20 - 0x7F
 */
static void set_msg_P(unsigned char display, const char *msg)
{
    // Iterate over the characters in the message
    for (unsigned char i = 0; i < 10; i++)
    {
        // Characters are defined by 6 bytes
        unsigned char c[6];

        // First byte gives 'character' opcode plus index
        c[0] = 0xB0 | i;

        // Remaining 5 bytes define character bitmap
        for (unsigned char j = 0; j < 5; j++)
            c[j+1] = pgm_read_byte(&(char_defs[pgm_read_byte(&msg[i]) - 0x20][j]));

        send_data(display, c, 6);
    }
}

static void set_fmt_P(unsigned char display, char *fmt, ...)
{
    va_list args;
    char buf[11];

    va_start(args, fmt);
    vsprintf_P(buf, fmt, args);
    va_end(args);

    unsigned char c[6];
    for (unsigned char i = 0; i < 10; i++)
    {
        c[0] = 0xB0 | i;
        for (unsigned char j = 0; j < 5; j++)
            c[j+1] = pgm_read_byte(&(char_defs[buf[i] - 0x20][j]));
        send_data(display, c, 6);
    }
}

/*
 * Initialize the SPI bus and display select lines
 * Clear the displays and set initial brightness to 0%
 */
void display_init_hardware()
{
    // Set MOSI, SCK, display select pins to output
    DDRB |= _BV(DDB5)|_BV(DDB7)|_BV(DISPLAY0)|_BV(DISPLAY1)|_BV(DISPLAY2)|_BV(DISPLAY3);

    // Enable SPI Master @ 2MHz, transmit LSB first
    SPCR = _BV(SPE)|_BV(MSTR)|_BV(SPI2X)|_BV(SPR0)|_BV(DORD);

    // Enable ADC for brightness level input
    // Set sample rate to 125khz
    ADCSRA |= _BV(ADPS2)|_BV(ADPS1)|_BV(ADPS0);

    // Left-align output in ADCH
    ADMUX |= _BV(ADLAR);

    // Enable ADC; enable free running mode; enable interrupt; start measuring
    ADCSRA |= _BV(ADEN)|_BV(ADATE)|_BV(ADIE)|_BV(ADSC);

    // Clear display
    unsigned char c = 0xC0;
    send_data(DISPLAY0, &c, 1);
    send_data(DISPLAY1, &c, 1);
    send_data(DISPLAY2, &c, 1);
    send_data(DISPLAY3, &c, 1);

    update_display_brightness();
}

/*
 * Set the brightness of the display
 * Uses bottom 3 bits of display_brightness to set
 * values: 1, 0.53, 0.4, 0.27, 0.2, 0.13, 0.066, 0
 */
void update_display_brightness()
{
    unsigned char c = 0xF0 | (0x07 & display_brightness);
    if (c == 0xF7) c = 0xFF; // 0% brightness

    send_data(DISPLAY0, &c, 1);
    send_data(DISPLAY1, &c, 1);
    send_data(DISPLAY2, &c, 1);
    send_data(DISPLAY3, &c, 1);
}

void update_display()
{
    // Change display brightness if necessary
    static unsigned char last_display_brightness = 0xF7;
    unsigned char temp = display_brightness;

    if (last_display_brightness != temp)
    {
        last_display_brightness = temp;
        update_display_brightness();
    }

    // Update the display every second or when the GPS state changes
    static unsigned char display_gps_seconds = 255; // Bogus value to force redraw on first run
    static unsigned char display_gps_state = GPS_UNAVAILABLE;
    if (display_gps_state == gps_state && display_gps_seconds == gps_last_timestamp.seconds)
        return;

    display_gps_state = gps_state;
    display_gps_seconds = gps_last_timestamp.seconds;

    // Take a local copy of state that can be changed by interrupts
    unsigned char display_monitor_mode = monitor_mode;
    unsigned char display_countdown_mode = countdown_mode;
    unsigned char display_countdown = exposure_countdown;
    unsigned char display_monitor_level_high = monitor_level_high;

    // Update top row (status and countdown)
    if (display_monitor_mode == MONITOR_START || display_monitor_mode == MONITOR_STOP)
    {
        set_msg_P(DISPLAY0, display_msg_wait_left);
        set_msg_P(DISPLAY1, display_msg_wait_right);
    }
    else if (display_countdown_mode == COUNTDOWN_SYNCING || display_monitor_mode == MONITOR_ACQUIRE)
    {
        PGM_P msg = (display_countdown_mode == COUNTDOWN_SYNCING) ? display_msg_align :
                    (!display_monitor_level_high) ? display_msg_readout : display_msg_expose;
        set_msg_P(DISPLAY0, msg);

        if (display_countdown_mode == COUNTDOWN_SYNCING)
            set_fmt_P(DISPLAY1, display_fmt_countdown, gps_last_timestamp.seconds % exposure_total, exposure_total);
        else
            set_fmt_P(DISPLAY1, display_fmt_countdown, exposure_total - display_countdown, exposure_total);
    }
    else
    {
        set_msg_P(DISPLAY0, display_msg_idle_left);
        set_msg_P(DISPLAY1, display_msg_idle_right);
    }

    // Update bottom row (time and locked state)
    switch (display_gps_state)
    {
        case GPS_ACTIVE:
            if (gps_last_timestamp.locked)
            {
                set_fmt_P(DISPLAY2, display_fmt_time_left, gps_last_timestamp.hours, gps_last_timestamp.minutes);
                set_fmt_P(DISPLAY3, display_fmt_time_right, gps_last_timestamp.seconds);
            }
            else
            {
                set_fmt_P(DISPLAY2, display_fmt_time_nolock_left,
                    gps_last_timestamp.hours,
                    gps_last_timestamp.minutes,
                    gps_last_timestamp.seconds
                );
                set_msg_P(DISPLAY3, display_fmt_time_nolock_right);
            }
        break;
        case GPS_SYNCING:
            set_msg_P(DISPLAY2, display_msg_syncing_left);
            set_msg_P(DISPLAY3, display_msg_syncing_right);
        break;
        case GPS_UNAVAILABLE:
            set_msg_P(DISPLAY2, display_msg_noserial_left);
            set_msg_P(DISPLAY3, display_msg_noserial_right);
        break;
    }
}

/*
 * ADC sample completed interrupt handler
 */
ISR(ADC_vect)
{
    // Only care about top 3 bits, inverted
    unsigned char temp = ADCH;
    display_brightness = (~temp) >> 5;
}
