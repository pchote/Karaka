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

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <stdio.h>
#include "command.h"
#include "main.h"
#include "gps.h"
#include "monitor.h"
#include "display.h"

const char command_msg_bad_packet[]     PROGMEM = "Bad packet - ignoring";
const char command_fmt_got_packet[]     PROGMEM = "Got packet '%c'";
const char command_fmt_unknown_packet[] PROGMEM = "Unknown packet type '%c' - ignoring";
const char command_fmt_bad_checksum[]   PROGMEM = "Command 0x%02x checksum failed. Expected 0x%02x, calculated 0x%02x.";

static uint8_t usart_packet_type = UNKNOWN_PACKET;
static uint8_t usart_packet_length = 0;
static uint8_t usart_packet[256];
static uint8_t usart_packet_expected_length = 0;

// NOTE: This is set up as a circular buffer and makes use of uint8 overflow > 256
// If you change the buffer length, add explicit behavior to handle looping
static uint8_t usart_input_buffer[256];
static uint8_t usart_input_read = 0;
static volatile uint8_t usart_input_write = 0;

static uint8_t usart_output_buffer[256];
static volatile uint8_t usart_output_read = 0;
static volatile uint8_t usart_output_write = 0;

/*
 * Initialize usart0 for talking to the acquisition PC via USB
 */
void command_init()
{
    // Set 9600 baud rate
    UBRR0H = 0;
#if CPU_MHZ == 16
    UBRR0L = 0x67;
#elif CPU_MHZ == 10
    UCSR0A = _BV(U2X0);
    UBRR0L = 0x81;
#else
    #error Unknown CPU Frequency
#endif

    // Enable receive, transmit, data received interrupt
    UCSR0B = _BV(RXEN0)|_BV(TXEN0)|_BV(RXCIE0);

    // Set 8-bit data frame
    UCSR0C = _BV(UCSZ01)|_BV(UCSZ00);

    usart_packet_type = UNKNOWN_PACKET;
    usart_packet_length = usart_packet_expected_length = 0;
    usart_input_read = usart_input_write = 0;
    usart_output_read = usart_output_write = 0;
}

/*
 * Add a byte to the send queue and start sending data if necessary
 */
void command_send_raw(uint8_t b)
{
    // Don't overwrite data that hasn't been sent yet
    while (usart_output_write == (uint8_t)(usart_output_read - 1));

    usart_output_buffer[usart_output_write++] = b;

    // Enable Transmit data register empty interrupt if necessary to send bytes down the line
    UCSR0B |= _BV(UDRIE0);
}


/*
 * Queue a data packet to the aquisition pc
 */
static void queue_data(uint8_t type, uint8_t *data, uint8_t length)
{
    // Data packet starts with $$ and packet type (which != $)
    command_send_raw('$');
    command_send_raw('$');
    command_send_raw(type);

    // Length of data section
    command_send_raw(length);

    // Packet data - calculate checksum as we go
    uint8_t csm = 0;
    for (uint8_t i = 0; i < length; i++)
    {
        command_send_raw(data[i]);
        csm ^= data[i];
    }

    // Checksum
    command_send_raw(csm);

    // Data packet ends a linefeed and carriage return
    command_send_raw('\r');
    command_send_raw('\n');
}

static void queue_data_P(uint8_t type, uint8_t *data, uint8_t length)
{
    // Data packet starts with $$ and packet type (which != $)
    command_send_raw('$');
    command_send_raw('$');
    command_send_raw(type);

    // Length of data section
    command_send_raw(length);

    // Packet data - calculate checksum as we go
    uint8_t csm = 0;
    for (uint8_t i = 0; i < length; i++)
    {
        uint8_t c = pgm_read_byte(&data[i]);
        command_send_raw(c);
        csm ^= c;
    }

    // Checksum
    command_send_raw(csm);

    // Data packet ends a linefeed and carriage return
    command_send_raw('\r');
    command_send_raw('\n');
}

/*
 * Queue a timestamp packet to the acquisition pc
 */
void send_timestamp()
{
    uint16_t count;
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
        count = exposure_countdown;
    }
    count = exposure_total - count;

    uint8_t data[] =
    {
        gps_last_timestamp.year & 0xFF,
        gps_last_timestamp.year >> 8,
        gps_last_timestamp.month,
        gps_last_timestamp.day,
        gps_last_timestamp.hours,
        gps_last_timestamp.minutes,
        gps_last_timestamp.seconds,
        gps_last_timestamp.milliseconds & 0xFF,
        gps_last_timestamp.milliseconds >> 8,
        gps_last_timestamp.locked,
        count & 0x00FF,
        count >> 8
    };
    queue_data(CURRENTTIME, data, 12);
}

void send_downloadtimestamp()
{
    // This is non-atomic, but something is very wrong if this
    // doesn't get sent before the next exposure is triggered
    uint8_t data[] =
    {
        download_timestamp.year & 0x00FF,
        download_timestamp.year >> 8,
        download_timestamp.month,
        download_timestamp.day,
        download_timestamp.hours,
        download_timestamp.minutes,
        download_timestamp.seconds,
        download_timestamp.milliseconds & 0xFF,
        download_timestamp.milliseconds >> 8,
        download_timestamp.locked
    };
    queue_data(DOWNLOADTIME, data, 10);
}

void send_stopexposure()
{
    queue_data(STOP_EXPOSURE, NULL, 0);
}

void send_status(TimerMode status)
{
    queue_data(STATUSMODE, &status, 1);
}

void send_debug_fmt_P(const char *fmt, ...)
{
    va_list args;
    char buf[128];

    va_start(args, fmt);
    int len = vsnprintf_P(buf, 128, fmt, args);
    va_end(args);

    if (len > 128) len = 128;
    queue_data(DEBUG_STRING, (uint8_t *)buf, (uint8_t)len);
}

void send_debug_string_P(const char *string)
{
    queue_data_P(DEBUG_STRING, (uint8_t *)string, strlen_P(string));
}

void send_debug_raw(uint8_t *data, uint8_t length)
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
 */
void usart_process_buffer()
{
    // Take a local copy of usart_input_write as it can be modified by interrupts
    uint8_t temp_write = usart_input_write;

    // No new data has arrived
    if (usart_input_read == temp_write)
        return;

    // Relay mode - forward all data to the GPS
    if (countdown_mode == COUNTDOWN_RELAY)
    {
        for (; usart_input_read != temp_write; usart_input_read++)
            gps_send_raw(usart_input_buffer[usart_input_read]);

        // Check for a reset command (usart_input_buffer full of 0)
        if (usart_input_buffer[usart_input_read] == 0 &&
            usart_input_buffer[usart_input_read + 1] == 0)
        {
            for (uint8_t i = 0; i < 255; i++)
                if (usart_input_buffer[i] != 0)
                    return;

            trigger_restart();
        }
        return;
    }

    // Sync to the start of a packet if necessary
    // Packet format: $$<type><data length><data 0>...<data length-1><checksum>\r\n
    for (; usart_packet_type == UNKNOWN_PACKET && usart_input_read != temp_write; usart_input_read++)
    {
        uint8_t temp_start = (uint8_t)(usart_input_read - 3);
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
        uint8_t data_length = usart_packet[3];
        uint8_t *data = &usart_packet[4];
        uint8_t data_checksum = usart_packet[usart_packet_length - 3];

        // Check packet end
        if (usart_packet[usart_packet_length - 2] != '\r' || usart_packet[usart_packet_length - 1] != '\n')
        {
            send_debug_string_P(command_msg_bad_packet);
            send_debug_raw(usart_packet, usart_packet_length);
            goto resetpacket;
        }

        // Verify checksum
        uint8_t csm = 0;
        for (uint8_t i = 0; i < data_length; i++)
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
                // First byte specifies whether the logic monitor is enabled
                monitor_simulate_camera = !data[0];

                // Second byte specifies timing mode
                timing_mode = (data[1] == MODE_HIGHRES) ? MODE_HIGHRES : MODE_PPSCOUNTER;

                // Remaining bytes specify 16-bit exposure
                //
                // These are only accessed from interrupt context
                // when countdown_mode = ENABLED or TRIGGERED so
                // these is safe to modify with interrupts enabled
                exposure_countdown = data[2] | data[3] << 8;
                exposure_total = exposure_countdown;

                // align_boundary is 8-bit, so use a temporary variable
                uint16_t temp_boundary = exposure_total;
                if (timing_mode == MODE_HIGHRES)
                    temp_boundary /= 1000;

                if (temp_boundary < 1)
                    temp_boundary = 1;
                else if (align_boundary > 60)
                    temp_boundary = 60;

                align_boundary = temp_boundary;

                // Set exposure display mode
                display_exposure_type = DISPLAY_EXPOSURE_REGULAR;
                if (timing_mode == MODE_HIGHRES)
                {
                    if (exposure_total < 2000)
                        display_exposure_type = DISPLAY_EXPOSURE_HIDE;
                    else if (exposure_total % 1000)
                        display_exposure_type = DISPLAY_EXPOSURE_PERCENT;
                }
                else
                {
                    if (exposure_total < 2)
                        display_exposure_type = DISPLAY_EXPOSURE_HIDE;
                    else if (exposure_total > 999)
                        display_exposure_type = DISPLAY_EXPOSURE_PERCENT;
                }

                // Trigger fake camera output
                if (monitor_simulate_camera)
                    simulate_camera_startup();

                // Monitor the camera for a level change indicating it has finished initializing
                monitor_mode = MONITOR_START;
                send_status(TIMER_WAITING);
            break;
            case STOP_EXPOSURE:
                // Disable the exposure countdown immediately
                STOP_MILLISECOND_TIMER;
                millisecond_count = 0;
                countdown_mode = COUNTDOWN_DISABLED;

                // These are only accessed from interrupt context
                // when countdown_mode = ENABLED or TRIGGERED so
                // these is safe to modify with interrupts enabled
                exposure_total = 0;
                exposure_countdown = 0;

                if (monitor_simulate_camera)
                    simulate_camera_shutdown();

                // Camera is reading out - Wait for a level change indicating it has finished
                if (!monitor_level_high)
                {
                    monitor_mode = MONITOR_STOP;
                    send_status(TIMER_WAITING);
                }
                else
                {
                    // Camera can be shutdown immediately
                    monitor_mode = MONITOR_WAIT;
                    send_stopexposure();
                    send_status(TIMER_IDLE);
                }
            break;
            case RESET:
                trigger_restart();
            break;
            case START_RELAY:
                countdown_mode = COUNTDOWN_RELAY;
            break;
            case START_UPGRADE:
                eeprom_update_byte(BOOTFLAG_EEPROM_OFFSET, BOOTFLAG_UPGRADE);
                trigger_restart();
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
