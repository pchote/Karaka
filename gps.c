//***************************************************************************
//
//  File        : gps.c
//  Copyright   : 2009-2012 Johnny McClymont, Paul Chote
//  Description : Extracts timestamps from a Trimble or Magellan serial stream
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include "main.h"
#include "gps.h"
#include "command.h"

// Init Trimble: Enable only the 8F-AB primary timing packet
const uint8_t trimble_init[9] PROGMEM = {0x10, 0x8E, 0xA5, 0x00, 0x01, 0x00, 0x00, 0x10, 0x03};

// Init Magellan: Disable the packets that the OEM software enables; enable timing and status packets
const uint8_t mgl_init[] PROGMEM = "$PMGLI,00,G00,0,A\r\n"
                             "$PMGLI,00,B00,0,A\r\n"
                             "$PMGLI,00,B02,0,A\r\n"
                             "$PMGLI,00,D00,0,A\r\n"
                             "$PMGLI,00,E00,0,A\r\n"
                             "$PMGLI,00,F02,0,A\r\n"
                             "$PMGLI,00,R04,0,A\r\n"
                             "$PMGLI,00,S01,0,A\r\n"
                             "$PMGLI,00,A00,2,B\r\n"
                             "$PMGLI,00,H00,2,B\r\n";

const char gps_msg_missed_pps[]         PROGMEM = "Missing PPS pulse detected";
const char gps_msg_unknown_mgl_packet[] PROGMEM = "Unknown magellan packet";
const char gps_msg_bad_packet[]         PROGMEM = "Bad GPS packet";
const char gps_fmt_skipped_bytes[]      PROGMEM = "Skipped %d bytes while syncing";
const char gps_fmt_checksum_failed[]    PROGMEM = "GPS Checksum failed. Got 0x%02x, expected 0x%02x";

static uint8_t gps_magellan_length = 0;
static bool gps_magellan_locked = false;

// NOTE: This is set up as a circular buffer and makes use of uint8 overflow > 256
// If you change the buffer length, add explicit behavior to handle looping
static uint8_t gps_input_buffer[256];
static uint8_t gps_input_read = 0;
static volatile uint8_t gps_input_write = 0;

static gpspackettype gps_packet_type = UNKNOWN_PACKET;
static uint8_t gps_packet_length = 0;
static uint8_t gps_packet[32];

static uint8_t gps_output_buffer[256];
static volatile uint8_t gps_output_read = 0;
static volatile uint8_t gps_output_write = 0;

volatile gpsstate gps_state = GPS_UNAVAILABLE;
volatile bool gps_record_synctime = false;
static uint8_t bytes_to_sync = 0;

timestamp gps_last_timestamp;
timestamp gps_last_synctime;

/*
 * Add a byte to the send queue and start sending data if necessary
 */
static void queue_send_byte(uint8_t b)
{
    // Don't overwrite data that hasn't been sent yet
    while (gps_output_write == (uint8_t)(gps_output_read - 1));

    gps_output_buffer[gps_output_write++] = b;

    // Enable Transmit data register empty interrupt if necessary to send bytes down the line
    cli();
    if ((UCSR1B & _BV(UDRIE1)) == 0)
        UCSR1B |= _BV(UDRIE1);
    sei();
}

/*
 * data register empty interrupt to send a byte down the wire
 */
ISR(USART1_UDRE_vect)
{
    if(gps_output_write != gps_output_read)
        UDR1 = gps_output_buffer[gps_output_read++];

    // Ran out of data to send - disable the interrupt
    if(gps_output_write == gps_output_read)
        UCSR1B &= ~_BV(UDRIE1);
}

/*
 * Initialize usart1 for communicating with the GPS via RS232
 * Enable timer1 to monitor for serial connection loss
 */
void gps_init_hardware()
{
    // Set baud rate to 9600
    UBRR1H = 0;
    UBRR1L = 0xCF;
    UCSR1A = _BV(U2X1);

    // Enable receive, transmit, data received interrupt
    UCSR1B = _BV(RXEN1)|_BV(TXEN1)|_BV(RXCIE1);

    // Set 8-bit data frame
    UCSR1C = _BV(UCSZ11)|_BV(UCSZ10);

    // Initialize timer1 to monitor GPS loss
    TCCR1A = 0x00;

    // Set prescaler to 1/1024: 64us per tick
    TCCR1B = _BV(CS10)|_BV(CS12);
#if HARDWARE_VERSION < 3
    TIMSK |= _BV(TOIE1);
#else
    TIMSK1 |= _BV(TOIE1);
#endif
    TCNT1 = 0x0BDB; // Overflow after 62500 ticks: 4.0s
}

/*
 * Reset data buffers and send configuration data for both GPS units
 * The connected GPS will ignore the data for other unit
 */
void gps_init_state()
{
    gps_magellan_length = 0;
    gps_magellan_locked = false;
    gps_input_read = gps_input_write = 0;
    gps_packet_type = gps_packet_length = 0;
    gps_output_read = gps_output_write = 0;

    gps_record_synctime = false;
    gps_state = GPS_UNAVAILABLE;

    // Send Trimble init data
    for (uint8_t i = 0; i < 9; i++)
        queue_send_byte(pgm_read_byte(&trimble_init[i]));

    // Send Magellan init data
    uint8_t i = 0, b = pgm_read_byte(&mgl_init[0]);
    do
    {
        queue_send_byte(b);
        b = pgm_read_byte(&mgl_init[++i]);
    } while (b != '\0');
}

static void set_time(timestamp *t)
{
    // Enable the counter for the next PPS pulse
    if (countdown_mode == COUNTDOWN_TRIGGERED)
        countdown_mode = COUNTDOWN_ENABLED;
    else if (countdown_mode == COUNTDOWN_ENABLED)
    {
        // We should always receive the PPS pulse before the time packet
        send_debug_string_P(gps_msg_missed_pps);
    }

    gps_last_timestamp = *t;

    // Mark that we have a valid timestamp
    gps_state = GPS_ACTIVE;

    // Synchronise the exposure with the edge of a minute
    if (countdown_mode == COUNTDOWN_SYNCING && (gps_last_timestamp.seconds % exposure_total == 0))
        countdown_mode = COUNTDOWN_ENABLED;

    if (gps_record_synctime)
    {
        cli();
        gps_last_synctime = gps_last_timestamp;
        gps_record_synctime = false;
        sei();

        send_downloadtimestamp();
    }
    send_timestamp();
}


/*
 * Haven't received any serial data in 4.0 seconds
 * The GPS has probably died
 */
ISR(TIMER1_OVF_vect)
{
    gps_state = GPS_UNAVAILABLE;
    interrupt_flags |= FLAG_NO_SERIAL;
}

/*
 * Received data from gps serial. Add to buffer
 */
ISR(USART1_RX_vect)
{
    // Reset timeout countdown
    TCNT1 = 0x0BDB;

    // Update status if necessary
    if (gps_state == GPS_UNAVAILABLE)
        gps_state = GPS_SYNCING;
    gps_input_buffer[gps_input_write++] = UDR1;
}

/*
 * Helper routine for determining whether a given year is a leap year
 */
static uint8_t is_leap_year(uint16_t year)
{
    if (year % 4) return 0;
    if (year % 100) return 1;
    return (year % 400) ? 0 : 1;
}

/*
 * Process any data in the received buffer
 * Parses at most one time packet - so must be called frequently
 * Returns true if the timestamp or status info has changed
 */
void gps_process_buffer()
{
    // Take a local copy of gps_input_write as it can be modified by interrupts
    uint8_t temp_write = gps_input_write;

    // No new data has arrived
    if (gps_input_read == temp_write)
        return;

    // Sync to the start of a packet if necessary
    for (; gps_packet_type == UNKNOWN_PACKET && gps_input_read != temp_write; gps_input_read++, bytes_to_sync++)
    {
        // Magellan packet
        if (gps_input_buffer[(uint8_t)(gps_input_read - 1)] == '$' &&
            gps_input_buffer[(uint8_t)(gps_input_read - 2)] == '$' &&
            // End of previous packet
            gps_input_buffer[(uint8_t)(gps_input_read - 3)] == 0x0A)
        {
            if (gps_input_buffer[gps_input_read] == 'A')
            {
                gps_packet_type = MAGELLAN_TIME_PACKET;
                gps_magellan_length = 13;
            }
            else if (gps_input_buffer[gps_input_read] == 'H')
            {
                gps_packet_type = MAGELLAN_STATUS_PACKET;
                gps_magellan_length = 16;
            }
            else // Some other Magellan packet - ignore it
            {
                send_debug_string_P(gps_msg_unknown_mgl_packet);
                continue;
            }

            if (bytes_to_sync > 3)
                send_debug_fmt_P(gps_fmt_skipped_bytes, bytes_to_sync);

            bytes_to_sync = 0;

            // Rewind to the start of the packet
            gps_input_read -= 2;
            break;
        }

        // Trimble
        if ( // Start of timing packet
            gps_input_buffer[gps_input_read] == 0xAB &&
            gps_input_buffer[(uint8_t)(gps_input_read - 1)] == 0x8F &&
            gps_input_buffer[(uint8_t)(gps_input_read - 2)] == DLE &&
            // End of previous packet
            gps_input_buffer[(uint8_t)(gps_input_read - 3)] == ETX &&
            gps_input_buffer[(uint8_t)(gps_input_read - 4)] == DLE)
        {
            gps_packet_type = TRIMBLE_PACKET;
            // Rewind to the start of the packet
            gps_input_read -= 2;
            break;
        }
    }

    switch (gps_packet_type)
    {
        case UNKNOWN_PACKET:
            // Still haven't synced to a packet
            return;

        case TRIMBLE_PACKET:
            // Write bytes into the packet buffer
            for (; gps_input_read != temp_write; gps_input_read++)
            {
                // DLE bytes are padded with a second DLE to distinguish from the packet start/end bytes
                if (gps_input_buffer[gps_input_read] == DLE)
                {
                    // Count previous number of DLEs - only want to skip if this is an odd count
                    uint8_t count = 1;

                    while (gps_input_buffer[(uint8_t)(gps_input_read - count)] == DLE)
                        count++;

                    if (!(count % 2))
                        gps_input_read++;
                }

                if (gps_input_read == temp_write)
                    break;

                gps_packet[gps_packet_length++] = gps_input_buffer[gps_input_read];

                // End of packet (Trimble timing packet is 21 bytes)
                if (gps_packet_length == 21)
                {
                    // Sanity check: Ensure the packet ends correctly
                    if (gps_packet[20] == ETX && gps_packet[19] == DLE)
                    {
                        set_time(&(timestamp){
                            .hours = gps_packet[14],
                            .minutes = gps_packet[13],
                            .seconds = gps_packet[12],
                            .day = gps_packet[15],
                            .month = gps_packet[16],
                            .year = ((gps_packet[17] << 8) & 0xFF00) | (gps_packet[18] & 0x00FF),
                            .locked = gps_packet[11] == 0x03 ? true : false
                        });
                    }
                    else
                    {
                        send_debug_string_P(gps_msg_bad_packet);
                        send_debug_raw(gps_packet, gps_packet_length);
                    }

                    // Reset for next packet
                    gps_packet_type = UNKNOWN_PACKET;
                    gps_packet_length = 0;
                    return;
                }
            }
        break;

        case MAGELLAN_TIME_PACKET:
        case MAGELLAN_STATUS_PACKET:
            for (; gps_input_read != temp_write; gps_input_read++)
            {
                // Store the packet
                gps_packet[gps_packet_length++] = gps_input_buffer[gps_input_read];

                // End of packet
                if (gps_packet_length == gps_magellan_length)
                {
                    // Check that the packet is valid.
                    // A valid packet will have the final byte as a linefeed (0x0A)
                    // and the second-to-last byte will be a checksum which will match
                    // the XORed bytes between the $$ and checksum.
                    //   gps_packet_length - 1 is the linefeed
                    //   gps_packet_length - 2 is the checksum byte
                    if (gps_packet[gps_packet_length - 1] == 0x0A)
                    {
                        uint8_t csm = gps_packet[2];
                        for (int i = 3; i < gps_packet_length - 2; i++)
                            csm ^= gps_packet[i];

                        // Verify the checksum
                        if (csm == gps_packet[gps_packet_length - 2])
                        {
                            if (gps_packet_type == MAGELLAN_TIME_PACKET)
                            {
                                // Correct for bad epoch offset
                                // Number of days in each month (ignoring leap years)
                                static uint8_t days[13] = {
                                    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
                                };

                                // Add 16 years (which always contains 4 leap days)
                                uint16_t year = (((gps_packet[9] << 8) & 0xFF00) | (gps_packet[10] & 0x00FF)) + 16;
                                uint8_t month = gps_packet[8];

                                // Add the rest as days to be resolved
                                uint16_t day = gps_packet[7] + 1324;

                                // Unknown 1 day offset
                                day += 1;

                                // Is this a leap year?
                                days[1] = is_leap_year(year) ? 29 : 28;

                                while (day > days[month - 1])
                                {
                                    if (++month > 12)
                                    {
                                        month = 1;
                                        year++;
                                        days[1] = is_leap_year(year) ? 29 : 28;
                                    }
                                    day -= days[month - 1];
                                }

                                set_time(&(timestamp){
                                    .hours = gps_packet[4],
                                    .minutes = gps_packet[5],
                                    .seconds = gps_packet[6],
                                    .day = day,
                                    .month = month,
                                    .year = year,
                                    .locked = gps_magellan_locked
                                });
                            }
                            else if (gps_packet_type == MAGELLAN_STATUS_PACKET) // Status packet
                            {
                                if (gps_magellan_locked != (gps_packet[13] == 6))
                                    gps_magellan_locked = (gps_packet[13] == 6);
                            }
                            else
                            {
                                send_debug_string_P(gps_msg_bad_packet);
                                send_debug_raw(gps_packet, gps_packet_length);
                            }
                        }
                        else
                        {
                            send_debug_fmt_P(gps_fmt_checksum_failed, csm, gps_packet[gps_packet_length - 2]);
                            send_debug_raw(gps_packet, gps_packet_length);
                        }
                    }
                    else
                    {
                        send_debug_string_P(gps_msg_bad_packet);
                        send_debug_raw(gps_packet, gps_packet_length);
                    }

                    // Reset buffer for the next packet
                    gps_packet_type = UNKNOWN_PACKET;
                    gps_packet_length = 0;
                    return;
                }
            }
        break;
    }
}
