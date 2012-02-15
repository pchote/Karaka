//***************************************************************************
//
//  File        : command.c
//  Copyright   : 2009-2012 Johnny McClymont, Paul Chote
//  Description : Handles communication with the Acquisition PC via serial
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include "command.h"
#include "main.h"
#include "gps.h"
#include "monitor.h"

char command_msg_bad_packet[]     PROGMEM = "Bad packet - ignoring";
char command_fmt_got_packet[]     PROGMEM = "Got packet 0x%02x";
char command_fmt_unknown_packet[] PROGMEM = "Unknown packet type 0x%02x - ignoring";
char command_fmt_bad_checksum[]   PROGMEM = "Command 0x%02x checksum failed. Expected 0x%02x, calculated 0x%02x.";

static unsigned char usart_packet_type = UNKNOWN_PACKET;
static unsigned char usart_packet_length = 0;
static unsigned char usart_packet[256];
static unsigned char usart_packet_expected_length = 0;

// Note: 256 size is used to allow overflow -> circular buffer
// If buffer size is changed you will need to add explicit overflow checks
static unsigned char usart_input_buffer[256];
static unsigned char usart_input_read = 0;
static volatile unsigned char usart_input_write = 0;

static unsigned char usart_output_buffer[256];
static volatile unsigned char usart_output_read = 0;
static volatile unsigned char usart_output_write = 0;

/*
 * Initialize usart0 for talking to the acquisition PC via USB
 */
void command_init_hardware()
{
    // Set the baudrate to 250k (0% error)
    UBRR0 = 0x03;

    // Enable receive, transmit, data received interrupt
    UCSR0B = _BV(RXEN0)|_BV(TXEN0)|_BV(RXCIE0);

    // Set 8-bit data frame
    UCSR0C = _BV(UCSZ01)|_BV(UCSZ00);
}

/*
 * Add a byte to the send queue and start sending data if necessary
 */
static void queue_send_byte(unsigned char b)
{
    // Don't overwrite data that hasn't been sent yet
    while (usart_output_write == usart_output_read - 1);

    usart_output_buffer[usart_output_write++] = b;

    // Enable Transmit data register empty interrupt if necessary to send bytes down the line
    cli();
    if ((UCSR0B & _BV(UDRIE0)) == 0)
        UCSR0B |= _BV(UDRIE0);
    sei();
}


/*
 * Queue a data packet to the aquisition pc
 */
static void queue_data(unsigned char type, unsigned char *data, unsigned char length)
{
    // Data packet starts with $$ and packet type (which != $)
    queue_send_byte('$');
    queue_send_byte('$');
    queue_send_byte(type);

    // Length of data section
    queue_send_byte(length);

    // Packet data - calculate checksum as we go
    unsigned char csm = 0;
    for (unsigned char i = 0; i < length; i++)
    {
        queue_send_byte(data[i]);
        csm ^= data[i];
    }

    // Checksum
    queue_send_byte(csm);

    // Data packet ends a linefeed and carriage return
    queue_send_byte('\r');
    queue_send_byte('\n');
}

static void queue_data_P(unsigned char type, unsigned char *data, unsigned char length)
{
    // Data packet starts with $$ and packet type (which != $)
    queue_send_byte('$');
    queue_send_byte('$');
    queue_send_byte(type);

    // Length of data section
    queue_send_byte(length);

    // Packet data - calculate checksum as we go
    unsigned char csm = 0;
    for (unsigned char i = 0; i < length; i++)
    {
        unsigned char c = pgm_read_byte(&data[i]);
        queue_send_byte(c);
        csm ^= c;
    }

    // Checksum
    queue_send_byte(csm);

    // Data packet ends a linefeed and carriage return
    queue_send_byte('\r');
    queue_send_byte('\n');
}

/*
 * Queue a timestamp packet to the acquisition pc
 */
void send_timestamp()
{
    unsigned char data[] =
    {
        gps_last_timestamp.hours,
        gps_last_timestamp.minutes,
        gps_last_timestamp.seconds,
        gps_last_timestamp.day,
        gps_last_timestamp.month,
        gps_last_timestamp.year & 0x00FF,
        gps_last_timestamp.year >> 8,
        gps_last_timestamp.locked,
        exposure_countdown
    };
    queue_data(CURRENTTIME, data, 9);
}

void send_downloadtimestamp()
{
    unsigned char data[] =
    {
        gps_last_synctime.hours,
        gps_last_synctime.minutes,
        gps_last_synctime.seconds,
        gps_last_synctime.day,
        gps_last_synctime.month,
        gps_last_synctime.year & 0x00FF,
        gps_last_synctime.year >> 8,
        gps_last_synctime.locked
    };
    queue_data(DOWNLOADTIME, data, 8);
}

void send_stopexposure()
{
    queue_data(STOP_EXPOSURE, NULL, 0);
}

void send_downloadcomplete()
{
    queue_data(DOWNLOADCOMPLETE, NULL, 0);
}

void send_debug_fmt_P(char *fmt, ...)
{
    va_list args;
    char buf[128];

    va_start(args, fmt);
    int len = vsnprintf_P(buf, 128, fmt, args);
    va_end(args);

    if (len > 128) len = 128;
    queue_data(DEBUG_STRING, (unsigned char *)buf, (unsigned char)len);
}

void send_debug_string_P(char *string)
{
    queue_data_P(DEBUG_STRING, (unsigned char *)string, strlen_P(string));
}

void send_debug_raw(unsigned char *data, unsigned char length)
{
    queue_data(DEBUG_RAW, data, length);
}

/*
 * data register empty interrupt to send a byte down the wire
 */
ISR(USART0_UDRE_vect)
{
    if(usart_output_write != usart_output_read)
        UDR0 = usart_output_buffer[usart_output_read++];

    // Ran out of data to send - disable the interrupt
    if(usart_output_write == usart_output_read)
        UCSR0B &= ~_BV(UDRIE0);
}


/*
 * Data received interrupt
 */
ISR(USART0_RX_vect)
{
    usart_input_buffer[usart_input_write++] = UDR0;
}

/*
 * Process any data in the received buffer
 * Parses at most one packet - so must be called frequently
 * Note: this relies on the usart_input_buffer being 256 chars long so that
 * the data pointers automatically overflow at 256 to give a circular buffer
 */
void usart_process_buffer()
{
    // Take a local copy of usart_input_write as it can be modified by interrupts
    unsigned char temp_write = usart_input_write;

    // No new data has arrived
    if (usart_input_read == temp_write)
        return;

    // Sync to the start of a packet if necessary
    // Packet format: $$<type><data length><data 0>...<data length-1><checksum>\r\n
    for (; usart_packet_type == UNKNOWN_PACKET && usart_input_read != temp_write; usart_input_read++)
    {
        unsigned char temp_start = (unsigned char)(usart_input_read - 3);
        if (usart_input_buffer[temp_start] == '$' &&
            usart_input_buffer[temp_start + 1] == '$' &&
            usart_input_buffer[temp_start + 2] != '$')
        {
            usart_packet_type = usart_input_buffer[temp_start + 2];
            usart_packet_expected_length = usart_input_buffer[temp_start + 3] + 7;

            // Rewind to the start of the packet
            usart_input_read -= 3;
            break;
        }
    }

    // Write bytes into the packet buffer
    for (; usart_input_read != temp_write; usart_input_read++)
    {
        usart_packet[usart_packet_length++] = usart_input_buffer[usart_input_read];

        if (usart_packet_length < usart_packet_expected_length)
            continue;

        // End of packet
        unsigned char data_length = usart_packet[3];
        unsigned char *data = &usart_packet[4];
        unsigned char data_checksum = usart_packet[usart_packet_length - 3];

        // Check packet end
        if (usart_packet[usart_packet_length - 2] != '\r' || usart_packet[usart_packet_length - 1] != '\n')
        {
            send_debug_string_P(command_msg_bad_packet);
            send_debug_raw(usart_packet, usart_packet_length);
            goto resetpacket;
        }

        // Verify checksum
        unsigned char csm = 0;
        for (unsigned char i = 0; i < data_length; i++)
            csm ^= data[i];

        if (csm != data_checksum)
        {
            send_debug_fmt_P(command_fmt_bad_checksum, usart_packet_type, data_checksum, csm);
            send_debug_raw(usart_packet, usart_packet_length);
            goto resetpacket;
        }

        // Handle packet
        send_debug_fmt_P(command_fmt_got_packet, usart_packet_type);
        switch(usart_packet_type)
        {
            case START_EXPOSURE:
                cli();
                exposure_countdown = exposure_total = *data;
                sei();

                // Monitor the camera for a level change indicating it has finished initializing
                monitor_mode = MONITOR_START;
                break;
            case STOP_EXPOSURE:
                cli();
                exposure_total = exposure_countdown = 0;
                sei();

                // Disable the exposure countdown immediately
                countdown_mode = COUNTDOWN_DISABLED;

                // Camera is reading out - Wait for a level change indicating it has finished
                if (!monitor_level_high)
                    monitor_mode = MONITOR_STOP;
                else
                {
                    // Camera can be shutdown immediately
                    monitor_mode = MONITOR_WAIT;
                    send_stopexposure();
                }
            break;
            case RESET:
                set_initial_state();
            break;
            default:
                send_debug_fmt_P(command_fmt_unknown_packet, usart_packet_type);
            break;
        }
        goto resetpacket;
    }

    return;
    // Reset for next packet and return
resetpacket:
    usart_packet_type = UNKNOWN_PACKET;
    usart_packet_expected_length = 0;
    usart_packet_length = 0;
}
