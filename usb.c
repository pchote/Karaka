//***************************************************************************
//
//  File        : usb.c
//  Copyright   : 2013 Paul Chote
//  Description : Communication interface with Acquisition PC via USB (uart0)
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>

#include "display.h"
#include "gps.h"
#include "main.h"
#include "camera.h"
#include "usb.h"

#define MAX_DATA_LENGTH 200
enum packet_state {HEADERA = 0, HEADERB, TYPE, LENGTH, DATA, CHECKSUM, FOOTERA, FOOTERB};
enum packet_type
{
    TIMESTAMP = 'A',
    TRIGGER = 'B',
    MESSAGE = 'C',
    MESSAGE_RAW = 'D',
    START_EXPOSURE = 'E',
    STOP_EXPOSURE = 'F',
    STATUS = 'H',
    ENABLE_RELAY = 'R',
};

struct packet_startexposure
{
    uint8_t use_monitor;
    enum timing_mode mode;
    uint16_t exposure;
    uint8_t stride;
    uint8_t align_first;
};

struct packet_status
{
    enum timer_status timer;
    enum gps_status gps;
};

struct packet_message
{
    uint8_t length;
    char str[MAX_DATA_LENGTH-1];
};

struct timer_packet
{
    enum packet_state state;
    enum packet_type type;
    uint8_t length;
    uint8_t progress;
    uint8_t checksum;

    union
    {
        // Extra byte allows us to always null-terminate strings for display
        uint8_t bytes[MAX_DATA_LENGTH+1];
        struct packet_startexposure startexp;
    } data;
};

const char unknown_packet_fmt[]  PROGMEM = "Unknown packet type '%c' - ignoring";
const char long_packet_fmt[]     PROGMEM = "Ignoring long packet: %c (length %u)";
const char checksum_failed_fmt[] PROGMEM = "Packet checksum failed. Got 0x%02x, expected 0x%02x";
const char invalid_packet_fmt[]  PROGMEM = "Invalid packet end byte. Got 0x%02x, expected 0x%02x";
const char got_packet_fmt[]      PROGMEM = "Got packet type '%c'";

static uint8_t input_buffer[256];
static uint8_t input_read = 0;
static volatile uint8_t input_write = 0;

static uint8_t output_buffer[256];
static volatile uint8_t output_read = 0;
static volatile uint8_t output_write = 0;

// Add a byte to the send buffer.
// Will block if the buffer is full
static void queue_byte(uint8_t b)
{
    // Don't overwrite data that hasn't been sent yet
    while (output_write == (uint8_t)(output_read - 1));

    output_buffer[output_write++] = b;

    // Enable transmit if necessary
    UCSR0B |= _BV(UDRIE0);
}

// Send data from RAM
static void queue_data(uint8_t type, const void *data, uint8_t length)
{
    // Header
    queue_byte('$');
    queue_byte('$');
    queue_byte(type);
    queue_byte(length);

    // Data
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < length; i++)
    {
        uint8_t b = ((uint8_t *)data)[i];
        queue_byte(b);
        checksum ^= b;
    }

    // Footer
    queue_byte(checksum);
    queue_byte('\r');
    queue_byte('\n');
}

static bool byte_available()
{
    return input_write != input_read;
}

static uint8_t read_byte()
{
    // Loop until data is available
    while (input_read == input_write);
    return input_buffer[input_read++];
}

ISR(USART0_UDRE_vect)
{
    if (output_write != output_read)
        UDR0 = output_buffer[output_read++];

    // Ran out of data to send - disable the interrupt
    if (output_write == output_read)
        UCSR0B &= ~_BV(UDRIE0);
}

ISR(USART0_RX_vect)
{
    input_buffer[(uint8_t)(input_write++)] = UDR0;
}

void usb_initialize()
{
#define BAUD 9600
#include <util/setbaud.h>
    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;
#if USE_2X
    UCSR0A = _BV(U2X0);
#endif

    // Enable receive, transmit, data received interrupt
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);

    input_read = input_write = 0;
    output_read = output_write = 0;
}

static void parse_packet(struct timer_packet *p)
{
    usb_send_message_fmt_P(got_packet_fmt, p->type);
    switch (p->type)
    {
        case START_EXPOSURE:
        {
            struct packet_startexposure *data = &p->data.startexp;

            timing_mode = data->mode;

            // These are only accessed from interrupt context
            // when timer_status == ALIGN,EXPOSING,READOUT so
            // these is safe to modify with interrupts enabled
            exposure_countdown = exposure_total = data->exposure;
            trigger_countdown = trigger_stride = data->stride;

            // align_boundary is 8-bit, so use a temporary variable
            uint16_t temp_boundary = exposure_total;
            if (timing_mode == MODE_HIGHRES)
                temp_boundary /= 1000;

            if (temp_boundary < 1 || data->align_first == 0)
                temp_boundary = 1;
            else if (temp_boundary > 60)
                temp_boundary = 60;

            align_boundary = temp_boundary;

            camera_start_exposing(data->use_monitor);

            // Update display configuration for new sequence
            display_update_config();
            break;
        }
        case STOP_EXPOSURE:
            // Disable the exposure countdown immediately
            STOP_MILLISECOND_TIMER;
            millisecond_count = 0;

            // These are only accessed from interrupt context
            // when timer_status == ALIGN,EXPOSING,READOUT so
            // these is safe to modify with interrupts enabled
            exposure_total = 0;
            exposure_countdown = 0;

            camera_stop_exposing();
            break;
        case ENABLE_RELAY:
            eeprom_update_byte(RELAY_EEPROM_OFFSET, RELAY_ENABLED);
            eeprom_update_byte(BOOTLOADER_EEPROM_OFFSET, BYPASS_ENABLED);
            break;
        default:
            usb_send_message_fmt_P(unknown_packet_fmt, p->type);
            break;
    }
}

void usb_tick()
{
    static struct timer_packet p = {.state = HEADERA};
    while (byte_available())
    {
        uint8_t b = read_byte();
        if (timer_status == TIMER_RELAY)
            gps_send_byte(b);

        switch (p.state)
        {
            case HEADERA:
            case HEADERB:
                if (b == '$')
                    p.state++;
                else
                    p.state = HEADERA;
                break;
            case TYPE:
                p.type = b;
                p.state++;
                break;
            case LENGTH:
                p.length = b;
                p.progress = 0;
                p.checksum = 0;
                if (p.length == 0)
                    p.state = CHECKSUM;
                else if (p.length <= sizeof(p.data))
                    p.state++;
                else
                {
                    usb_send_message_fmt_P(long_packet_fmt, p.type, p.length);
                    p.state = HEADERA;
                }
                break;
            case DATA:
                p.checksum ^= b;
                p.data.bytes[p.progress++] = b;
                if (p.progress == p.length)
                    p.state++;
                break;
            case CHECKSUM:
                if (p.checksum == b)
                    p.state++;
                else
                {
                    usb_send_message_fmt_P(checksum_failed_fmt, b, p.checksum);
                    p.state = HEADERA;
                }
                break;
            case FOOTERA:
                if (b == '\r')
                    p.state++;
                else
                {
                    usb_send_message_fmt_P(invalid_packet_fmt, b, '\r');
                    p.state = HEADERA;
                }
                break;
            case FOOTERB:
                if (b == '\n')
                    parse_packet(&p);
                else
                    usb_send_message_fmt_P(invalid_packet_fmt, b, '\n');
    
                p.state = HEADERA;
                break;
        }
    }
}

void usb_send_message_P(const char *string)
{
    struct packet_message msg;
    msg.length = strlen_P(string);
    if (msg.length > MAX_DATA_LENGTH-1)
        msg.length = MAX_DATA_LENGTH-1;

    strncpy_P(msg.str, string, msg.length);
    queue_data(MESSAGE, &msg, msg.length + 1);
}

void usb_send_message_fmt_P(const char *fmt, ...)
{
    va_list args;
    struct packet_message msg;

    va_start(args, fmt);
    int len = vsnprintf_P(msg.str, MAX_DATA_LENGTH, fmt, args);
    va_end(args);

    if (len > MAX_DATA_LENGTH-1)
        len = MAX_DATA_LENGTH-1;

    msg.length = (uint8_t)len;
    queue_data(MESSAGE, &msg, msg.length + 1);
}

void usb_send_timestamp()
{
    // Add exposure progress to timestamp
    uint16_t count;
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
        count = exposure_countdown;
    }
    current_timestamp.exposure_progress = exposure_total - count;

    queue_data(TIMESTAMP, &current_timestamp, sizeof(struct timestamp));
}

void usb_send_trigger()
{
    // This is non-atomic, but something is very wrong if this
    // doesn't get sent before the next exposure is triggered
    queue_data(TRIGGER, (void *)&download_timestamp, sizeof(struct timestamp));
}

void usb_stop_exposure()
{
    queue_data(STOP_EXPOSURE, NULL, 0);
}

void usb_send_status(enum timer_status timer, enum gps_status gps)
{
    struct packet_status data = {
        .timer = timer,
        .gps = gps
    };
    queue_data(STATUS, &data, sizeof(struct packet_status));
}

void usb_send_raw(uint8_t *data, uint8_t length)
{
    struct packet_message msg;
    msg.length = length;
    if (msg.length > MAX_DATA_LENGTH-1)
        msg.length = MAX_DATA_LENGTH-1;

    memcpy(msg.str, data, msg.length);
    queue_data(MESSAGE_RAW, &msg, msg.length + 1);
}

// Send a raw byte without wrapping in a packet
// Used for relay mode
void usb_send_byte(uint8_t b)
{
    queue_byte(b);
}
