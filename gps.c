//***************************************************************************
//
//  File        : gps.c
//  Copyright   : 2013 Paul Chote
//  Description : Parses time information from a Trimble or Magellan serial stream
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <stdio.h>
#include "main.h"
#include "gps.h"
#include "usb.h"

enum packet_state {TB_HEADER = 0, TB_TYPEA, TB_TYPEB, TB_DATA, TB_FOOTERA, TB_FOOTERB,
                   MGL_HEADERA, MGL_HEADERB, MGL_TYPE, MGL_DATA, MGL_CHECKSUM, MGL_FOOTER};

struct trimble_timestamp
{
    // Big endian
    uint32_t gps_seconds;
    uint16_t gps_week;
    int16_t utc_offset;

    uint8_t flags;
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;

    // Big endian
    uint16_t year;
};

struct magellan_timestamp
{
    uint8_t unknown;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t day;
    uint8_t month;

    // Big endian
    uint16_t year;
};

struct magellan_status
{
    uint8_t unknown[10];
    uint8_t status;
};

struct gps_packet
{
    enum packet_state state;
    uint8_t length;
    uint8_t progress;

    // Checksum for magellan, padding flag for trimble
    uint8_t extra;

    union
    {
        struct trimble_timestamp trimble;
        struct magellan_timestamp magellan_time;
        struct magellan_status magellan_status;
        uint8_t bytes[1];
    } data;
};

// Init Magellan: Disable the packets that the OEM software enables; enable timing and status packets
const char initialization_data[] PROGMEM = ""
    // Trimble initialization
    // Disable everything except the 8F-AB timing packet
    "\x10\x8E\xA5\x00\x01\x00\x00\x10\x03"
    // Configure UTC time output
    "\x10\x8E\xA2\x03\x10\x03"
    // Magellan initialization
    "$PMGLI,00,G00,0,A\r\n"
    "$PMGLI,00,B00,0,A\r\n"
    "$PMGLI,00,B02,0,A\r\n"
    "$PMGLI,00,D00,0,A\r\n"
    "$PMGLI,00,E00,0,A\r\n"
    "$PMGLI,00,F02,0,A\r\n"
    "$PMGLI,00,R04,0,A\r\n"
    "$PMGLI,00,S01,0,A\r\n"
    "$PMGLI,00,A00,2,B\r\n"
    "$PMGLI,00,H00,2,B\r\n";
const uint8_t initialization_length = 205;

static const char invalid_packet_fmt[]  PROGMEM = "Invalid packet end byte. Got 0x%02x, expected 0x%02x";
static const char checksum_failed_fmt[] PROGMEM = "Packet checksum failed. Got 0x%02x, expected 0x%02x";
static const char additional_days_fmt[] PROGMEM = "Additional days: %d";

static uint8_t input_buffer[256];
static uint8_t input_read = 0;
static volatile uint8_t input_write = 0;

static uint8_t output_buffer[256];
static volatile uint8_t output_read = 0;
static volatile uint8_t output_write = 0;

static uint8_t serial_timeout_counter = 0;

/*
 * Add a byte to the send queue and start sending data if necessary
 */
void gps_send_byte(uint8_t b)
{
    // Don't overwrite data that hasn't been sent yet
    while (output_write == (uint8_t)(output_read - 1));

    output_buffer[output_write++] = b;

    // Enable transmit if necessary
    UCSR1B |= _BV(UDRIE1);
}

static inline bool byte_available()
{
    return input_write != input_read;
}

static uint8_t read_byte()
{
    // Loop until data is available
    while (input_read == input_write);
    return input_buffer[input_read++];
}

ISR(USART1_UDRE_vect)
{
    if (output_write != output_read)
        UDR1 = output_buffer[output_read++];

    // Ran out of data to send - disable the interrupt
    if (output_write == output_read)
        UCSR1B &= ~_BV(UDRIE1);
}

ISR(USART1_RX_vect)
{
    // Reset timeout countdown
    serial_timeout_counter = 0;

    // Update status if necessary
    if (gps_status == GPS_UNAVAILABLE)
        set_gps_status(GPS_SYNCING);

    input_buffer[(uint8_t)(input_write++)] = UDR1;
}

void gps_initialize()
{
    // Serial timeout watchdog
    // Triggers every 25.6ms to increment the timeout counter
    TCCR1A = 0x00;
    TCCR2A = _BV(WGM21);
    TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20);
    TIMSK2 |= _BV(OCIE2A);
    OCR2A = 250;

    // Set baud rate to 9600
    // and timeout counter to 16.384ms
#define BAUD 9600
#include <util/setbaud.h>
    UBRR1H = UBRRH_VALUE;
    UBRR1L = UBRRL_VALUE;
#if USE_2X
    UCSR1A = _BV(U2X0);
#endif

    // Enable receive, transmit, data received interrupt
    UCSR1B = _BV(RXEN1)|_BV(TXEN1)|_BV(RXCIE1);

    // Send GPS configuration data
    for (uint8_t i = 0; i < initialization_length; i++)
        gps_send_byte(pgm_read_byte(&initialization_data[i]));
}

ISR(TIMER2_COMPA_vect)
{
    // No data received in 3 seconds
    if (++serial_timeout_counter == 118)
    {
        set_gps_status(GPS_UNAVAILABLE);
        serial_timeout_counter = 0;
    }
}

// Swap the endian-ness of a 16-bit integer
static inline uint16_t swap_bytes(uint16_t b)
{
    return ((b & 0xFF00) >> 8) | ((b & 0xFF) << 8);
}

static uint8_t is_leap_year(uint16_t year)
{
    if (year % 4) return 0;
    if (year % 100) return 1;
    return (year % 400) ? 0 : 1;
}

void parse_packet(struct gps_packet *p)
{
    static bool magellan_time_locked = false;
    switch (p->length)
    {
        case sizeof(struct trimble_timestamp):
        {
            struct trimble_timestamp *tt = &p->data.trimble;

            set_time(&(struct timestamp){
                .year = swap_bytes(tt->year),
                .month = tt->month,
                .day = tt->day,
                .hours = tt->hours,
                .minutes = tt->minutes,
                .seconds = tt->seconds,
                .milliseconds = 0,
                .locked = tt->flags == 0x03
            });
            break;
        }
        case sizeof(struct magellan_status):
        {
            magellan_time_locked = p->data.magellan_status.status == 0x06;
            break;
        }
        case sizeof(struct magellan_timestamp):
        {
            struct magellan_timestamp *mt = &p->data.magellan_time;

            // Swap from big-endian to little-endian
            mt->year = swap_bytes(mt->year);

            uint8_t days[12] = {
                31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
            };

            // The Magellan firmware has not been updated to the new GPS epoch
            // so we must add 1024 weeks to correct the date
            uint16_t day = 7168;

            // Rewind to Jan 1 of the assumed year
            days[1] = is_leap_year(mt->year) ? 29 : 28;
            for (uint8_t i = 0; i < mt->month - 1; i++)
                day += days[i];
            day += mt->day;
            mt->month = 1;

            // Advance full years
            while (day >= 366)
            {
                day -= is_leap_year(mt->year) ? 366 : 365;
                mt->year++;
            }

            // Advance final partial year
            days[1] = is_leap_year(mt->year) ? 29 : 28;
            while (day > days[mt->month - 1])
                day -= days[mt->month++ - 1];

            set_time(&(struct timestamp){
                .year = mt->year,
                .month = mt->month,
                .day = day,
                .hours = mt->hours,
                .minutes = mt->minutes,
                .seconds = mt->seconds,
                .milliseconds = 0,
                .locked = magellan_time_locked
            });
            break;
        }
    }
}

void gps_tick()
{
    static struct gps_packet p = {.state = TB_HEADER};
    while (byte_available())
    {
        uint8_t b = read_byte();
        if (timer_status == TIMER_RELAY)
            usb_send_byte(b);

        switch (p.state)
        {
        case TB_HEADER:
        case MGL_HEADERA:
            if (b == 0x10)
                p.state = TB_TYPEA;
            else if (b == '$')
                p.state = MGL_HEADERB;
            break;

        // Trimble packets
        case TB_TYPEA:
            // We only care about 8F-AB
            if (b == 0x8F)
                p.state++;
            else
                p.state = TB_HEADER;
            break;
        case TB_TYPEB:
            if (b == 0xAB)
            {
                p.length = sizeof(struct trimble_timestamp);
                p.progress = 0;
                p.extra = 0;
                p.state++;
            }
            else
                p.state = TB_HEADER;
            break;
        case TB_DATA:
            // Padding byte - The next byte is another 0x10, or 0x03 (frame-end)
            if (b == 0x10 && (p.extra ^= 1))
                break;

            p.data.bytes[p.progress++] = b;
            if (p.progress == p.length)
                p.state++;
            break;
        case TB_FOOTERA:
            if (b == 0x10)
                p.state++;
            else
            {
                usb_send_message_fmt_P(invalid_packet_fmt, b, 0x10);
                usb_send_raw(p.data.bytes, p.length);
                p.state = TB_HEADER;
            }
            break;
        case TB_FOOTERB:
            if (b == 0x03)
                parse_packet(&p);
            else
            {
                usb_send_message_fmt_P(invalid_packet_fmt, b, 0x03);
                usb_send_raw(p.data.bytes, p.length);
            }
            p.state = TB_HEADER;
            break;

        // Magellan packets
        case MGL_HEADERB:
            if (b == '$')
                p.state++;
            else
                p.state = MGL_HEADERA;
            break;
        case MGL_TYPE:
            if (b == 'A' || b == 'H')
            {
                p.length = b == 'A' ? sizeof(struct magellan_timestamp) : sizeof(struct magellan_status);
                p.progress = 0;
                p.extra = b;
                p.state++;
            }
            else
                p.state = TB_HEADER;
            break;
        case MGL_DATA:
            p.extra ^= b;
            p.data.bytes[p.progress++] = b;
            if (p.progress == p.length)
                p.state++;
            break;
        case MGL_CHECKSUM:
            if (p.extra == b)
                p.state++;
            else
            {
                usb_send_message_fmt_P(checksum_failed_fmt, b, p.extra);
                p.state = MGL_HEADERA;
            }
            break;
        case MGL_FOOTER:
            if (b == '\n')
                parse_packet(&p);
            else
                usb_send_message_fmt_P(invalid_packet_fmt, b, '\n');
            p.state = MGL_HEADERA;
            break;
        }
    }
}
