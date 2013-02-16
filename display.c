//***************************************************************************
//
//  File        : display.c
//  Copyright   : 2012, 2013 Paul Chote
//  Description : Dot matrix / LCD display controller
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include "main.h"
#include "display.h"
#include "gps.h"

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdio.h>

static const uint8_t led_chars[96][5] PROGMEM = {
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

enum display_flags
{
    DISPLAY_TOP    = _BV(0),
    DISPLAY_BOTTOM = _BV(1),
    DISPLAY_LEFT   = _BV(2),
    DISPLAY_RIGHT  = _BV(3)
};

enum display_exposure_mode
{
    EXPOSURE_SECONDS = _BV(0),
    EXPOSURE_PERCENT = _BV(1),
    EXPOSURE_HIDE    = _BV(2),
};

enum display_type
{
    DISPLAY_LED = _BV(0),
    DISPLAY_LCD = _BV(1)
};

enum lcd_data_type
{
    LCD_COMMAND = 0x00,
    LCD_CHAR    = 0x01,
    LCD_STARTUP = 0x02
};

// Display messages
static const char msg_noserial[]    PROGMEM = "NO SERIAL CONNECTION";
static const char msg_idle[]        PROGMEM = "        IDLE        ";
static const char msg_wait[]        PROGMEM = " WAITING FOR CAMERA ";
static const char msg_relay[]       PROGMEM = "     RELAY MODE     ";
static const char msg_expose_c[]    PROGMEM = "       EXPOSE       ";
static const char msg_readout_c[]   PROGMEM = "       READOUT      ";

// For display with countdown
static const char msg_align[]       PROGMEM = "  ALIGN             ";
static const char msg_expose[]      PROGMEM = "  EXPOSE            ";
static const char msg_readout[]     PROGMEM = "  READOUT           ";
static const char fmt_countdown[]   PROGMEM = "           %03d/%03d  ";
static const char fmt_percentage[]  PROGMEM = "              %3d%%  ";

// Bottom display
static const char fmt_time[]        PROGMEM = "    %02d:%02d:%02d UTC    ";
static const char fmt_time_nolock[] PROGMEM = "%02d:%02d:%02d NO GPS LOCK";
static const char msg_syncing[]     PROGMEM = "  SYNCING TO SERIAL ";

static const uint8_t led_display_map[4] = {_BV(PB1), _BV(PB2), _BV(PB3), _BV(PB4)};
static const uint8_t lcd_display_map[4] = {0x80, 0x8A, 0xC0, 0xCA};

enum display_type display_type = DISPLAY_LED;
volatile uint8_t led_brightness = 0xF7;
enum display_exposure_mode exposure_mode;

/*
 * Queue data to the display via SPI
 */
static void led_send_byte(uint8_t display, uint8_t b)
{
    // Toggle load line for the appropriate display
    PORTB &= ~display;

    // Load data into SPI out
    SPDR = b;
    loop_until_bit_is_set(SPSR, SPIF);

    // Return load line to end read
    PORTB |= display;
}

/*
 * Set the brightness of the display
 * Uses bottom 3 bits of display_brightness to set
 * values: 1, 0.53, 0.4, 0.27, 0.2, 0.13, 0.066, 0
 */
static void led_update_brightness()
{
    static uint8_t last_led_brightness = 0xFF;
    uint8_t temp = led_brightness;

    if (last_led_brightness != temp)
    {
        last_led_brightness = temp;
        uint8_t c = 0xF0 | (0x07 & led_brightness);
        if (c == 0xF7) c = 0xFF; // 0% brightness

        for (uint8_t i = 0; i < 4; i++)
            led_send_byte(led_display_map[i], c);
    }
}

/*
 * Brightness pot sample handler
 */
ISR(ADC_vect)
{
    // Only care about top 3 bits, inverted
    led_brightness = ((uint8_t)~ADCH) >> 5;
}

/*
 * Initialize the SPI bus and display select lines
 * Clear the displays and set initial brightness to 0%
 */
static void led_init()
{
    // Set MOSI, SCK, display select pins to output
    DDRB |= 0xBE;

    // Enable SPI Master @ 2MHz, transmit LSB first
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPI2X) | _BV(SPR0) | _BV(DORD);

    // Enable ADC for brightness level input
    // Set sample rate to 125khz
    ADCSRA |= _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);

    // Left-align output in ADCH
    ADMUX |= _BV(ADLAR);

    // Enable ADC; enable free running mode; enable interrupt; start measuring
    ADCSRA |= _BV(ADEN) | _BV(ADATE) | _BV(ADIE) | _BV(ADSC);

    // Clear display
    for (uint8_t i = 0; i < 4; i++)
        led_send_byte(led_display_map[i], 0xC0);
}

static void lcd_send_byte(enum lcd_data_type type, uint8_t b)
{
    // Wait until the busy flag clears
    // Only available after configuration is complete
    if (!(type & LCD_STARTUP))
    {
        // Switch data bus to input
        DDRA = 0x00;

        // Set RS/RW
        PORTC &= 0xFC;
        PORTC |= _BV(PC1);

        do
        {
            // Ensure full clock cycle is at least 1us
            _delay_us(0.5);

            // Clock for at least 450ns
            PORTC |= _BV(PC6);
            _delay_us(0.5);
            PORTC &= ~_BV(PC6); // Clock low
        } while (bit_is_set(PINA, PA7));
    }

    // Load the character into the data output
    DDRA = 0xFF;
    PORTA = b;

    // Set RS/RW
    PORTC &= 0xFC;
    PORTC |= type & 0x01;

    // Wait for at least 140ns before clocking
    _delay_us(0.15);

    // Clock for at least 450ns
    PORTC |= _BV(PC6);
    _delay_us(0.5);
    PORTC &= ~_BV(PC6);
}

static void lcd_init()
{
    // Set all of PORTA as data output
    DDRA = 0xFF;

    // Set status pins as output
    DDRC |= _BV(PC0) | _BV(PC1) | _BV(PC6);

    // Manual startup sequence
    _delay_ms(100);
    lcd_send_byte(LCD_STARTUP, 0x38);
    _delay_ms(4.1);
    lcd_send_byte(LCD_STARTUP, 0x38);
    _delay_us(100);
    lcd_send_byte(LCD_STARTUP, 0x38);

    lcd_send_byte(LCD_COMMAND, 0x06); // Increment on write
    lcd_send_byte(LCD_COMMAND, 0x0C); // Display on / no cursor
    lcd_send_byte(LCD_COMMAND, 0x01); // Clear display
}

/*
 * Read a 10 char string from flash and display on the requested module
 */
static void set_display_P(uint8_t display, const char *msg)
{
    if (display_type == DISPLAY_LCD)
    {
        lcd_send_byte(LCD_COMMAND, lcd_display_map[display]);
        for (uint8_t i = 0; i < 10; i++)
            lcd_send_byte(LCD_CHAR, pgm_read_byte(&msg[i]));
    }
    else
    {
        for (uint8_t i = 0; i < 10; i++)
        {
            // First byte gives 'character' opcode plus index
            led_send_byte(led_display_map[display], 0xB0 | i);

            // Remaining 5 bytes define character bitmap
            for (uint8_t j = 0; j < 5; j++)
            {
                uint8_t b = pgm_read_byte(&(led_chars[pgm_read_byte(&msg[i]) - 0x20][j]));
                led_send_byte(led_display_map[display], b);
            }
        }
    }
}

/*
 * Read a 10 char string from ram and display on the requested module
 */
static void set_display(uint8_t display, const char *msg)
{
    if (display_type == DISPLAY_LCD)
    {
        lcd_send_byte(LCD_COMMAND, lcd_display_map[display]);
        for (uint8_t i = 0; i < 10; i++)
            lcd_send_byte(LCD_CHAR, msg[i]);
    }
    else
    {
        for (uint8_t i = 0; i < 10; i++)
        {
            // First byte gives 'character' opcode plus index
            led_send_byte(led_display_map[display], 0xB0 | i);

            // Remaining 5 bytes define character bitmap
            for (uint8_t j = 0; j < 5; j++)
            {
                uint8_t b = pgm_read_byte(&(led_chars[msg[i] - 0x20][j]));
                led_send_byte(led_display_map[display], b);
            }
        }
    }
}

/*
 * Display a string on a subset of display modules
 */
static void set_msg_P(enum display_flags flags, const char *msg)
{
    if (flags & DISPLAY_TOP)
    {
        if (flags & DISPLAY_LEFT)
            set_display_P(0, msg);
        if (flags & DISPLAY_RIGHT)
            set_display_P(1, msg + 10);
    }

    if (flags & DISPLAY_BOTTOM)
    {
        if (flags & DISPLAY_LEFT)
            set_display_P(2, msg);
        if (flags & DISPLAY_RIGHT)
            set_display_P(3, msg + 10);
    }
}

static void set_fmt_P(enum display_flags flags, const char *fmt, ...)
{
    va_list args;
    char buf[21];

    va_start(args, fmt);
    vsprintf_P(buf, fmt, args);
    va_end(args);

    if (flags & DISPLAY_TOP)
    {
        if (flags & DISPLAY_LEFT)
            set_display(0, buf);
        if (flags & DISPLAY_RIGHT)
            set_display(1, buf + 10);
    }

    if (flags & DISPLAY_BOTTOM)
    {
        if (flags & DISPLAY_LEFT)
            set_display(2, buf);
        if (flags & DISPLAY_RIGHT)
            set_display(3, buf + 10);
    }
}

void display_init()
{
    // Read display select pin
    DDRD &= ~_BV(PD7);
    display_type = (bit_is_set(PIND, PD7)) ? DISPLAY_LCD : DISPLAY_LED;

    if (display_type == DISPLAY_LCD)
        lcd_init();
    else
        led_init();

    display_update_config();
}

void display_update_config()
{
    exposure_mode = EXPOSURE_SECONDS;
    if (timing_mode == MODE_HIGHRES)
    {
        if (exposure_total < 2000)
            exposure_mode = EXPOSURE_HIDE;
        else if (exposure_total % 1000)
            exposure_mode = EXPOSURE_PERCENT;
    }
    else
    {
        if (exposure_total < 2)
            exposure_mode = EXPOSURE_HIDE;
        else if (exposure_total > 999)
            exposure_mode = EXPOSURE_PERCENT;
    }
}

void display_update()
{
    // Change display brightness if necessary
    if (display_type == DISPLAY_LED)
        led_update_brightness();

    uint16_t display_progress;
    enum timer_status status;
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
        display_progress = exposure_total - exposure_countdown;
        status = timer_status;
    }

    switch (status)
    {
        case TIMER_RELAY:
            set_msg_P(DISPLAY_TOP | DISPLAY_LEFT | DISPLAY_RIGHT, msg_relay);
            break;
        case TIMER_WAITING:
            set_msg_P(DISPLAY_TOP | DISPLAY_LEFT | DISPLAY_RIGHT, msg_wait);
            break;
        case TIMER_ALIGN:
            set_msg_P(DISPLAY_TOP | DISPLAY_LEFT, msg_align);
            set_fmt_P(DISPLAY_TOP | DISPLAY_RIGHT, fmt_countdown, current_timestamp.seconds % align_boundary, align_boundary);
            break;
        case TIMER_EXPOSING:
        case TIMER_READOUT:
            switch (exposure_mode)
            {
                case EXPOSURE_HIDE:
                {
                    const char *msg = status == TIMER_EXPOSING ? msg_expose_c : msg_readout_c;
                    set_msg_P(DISPLAY_TOP | DISPLAY_LEFT | DISPLAY_RIGHT, msg);
                    break;
                }
                case EXPOSURE_SECONDS:
                {
                    const char *msg = status == TIMER_EXPOSING ? msg_expose : msg_readout;
                    set_msg_P(DISPLAY_TOP | DISPLAY_LEFT, msg);
                    if (timing_mode == MODE_HIGHRES)
                        set_fmt_P(DISPLAY_TOP | DISPLAY_RIGHT, fmt_countdown,
                                  display_progress / 1000, exposure_total / 1000);
                    else
                        set_fmt_P(DISPLAY_TOP | DISPLAY_RIGHT, fmt_countdown,
                                  display_progress, exposure_total);
                    break;
                }
                case EXPOSURE_PERCENT:
                {
                    const char *msg = status == TIMER_EXPOSING ? msg_expose : msg_readout;
                    uint16_t percentage = display_progress / (exposure_total / 100);
                    set_msg_P(DISPLAY_TOP | DISPLAY_LEFT, msg);
                    set_fmt_P(DISPLAY_TOP | DISPLAY_RIGHT, fmt_percentage, percentage);
                    break;
                }
            }
            break;
        case TIMER_IDLE:
        default:
            set_msg_P(DISPLAY_TOP | DISPLAY_LEFT | DISPLAY_RIGHT, msg_idle);
    }

    // Update bottom row (time and locked state)
    switch (gps_status)
    {
        case GPS_ACTIVE:
        {
            const char *fmt = current_timestamp.locked ? fmt_time : fmt_time_nolock;
            set_fmt_P(DISPLAY_BOTTOM | DISPLAY_LEFT | DISPLAY_RIGHT, fmt,
                current_timestamp.hours,
                current_timestamp.minutes,
                current_timestamp.seconds
            );
            break;
        }
        case GPS_SYNCING:
            set_msg_P(DISPLAY_BOTTOM | DISPLAY_LEFT | DISPLAY_RIGHT, msg_syncing);
        break;
        case GPS_UNAVAILABLE:
            set_msg_P(DISPLAY_BOTTOM | DISPLAY_LEFT | DISPLAY_RIGHT, msg_noserial);
        break;
    }
}
