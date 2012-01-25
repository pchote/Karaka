//***************************************************************************
//
//  File        : command.c
//  Copyright   : 2009-2011 Johnny McClymont, Paul Chote
//  Description : Handles communication with the Acquisition PC via serial
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include <stdio.h>
#include "command.h"
#include "main.h"
#include "gps.h"
#include "monitor.h"

static unsigned char usart_packet_type;
static unsigned char usart_packet_length;
static unsigned char usart_packet[256];

// Buffer for a short error message
static char error[64];

// Note: 256 size is used to allow overflow -> circular buffer
// If buffer size is changed you will need to add explicit overflow checks
static unsigned char usart_input_buffer[256];
static unsigned char usart_input_read;
static volatile unsigned char usart_input_write;

static unsigned char usart_output_buffer[256];
static volatile unsigned char usart_output_read;
static volatile unsigned char usart_output_write;

/*
 * Initialise the command parser
 */
void command_init(void)
{
    // Set the baudrate prescaler
    // scale = (f_cpu / (16*baud)) - 1
    unsigned int baudrate = 16; // has a 2.5% error
    UBRR0H = (unsigned char)(baudrate>>8);
    UBRR0L = (unsigned char)baudrate;


    // Enable receiver and transmitter. Enable Receive interrupt, disable transmit interrupt.
    // Set USART capabilities
    // RXEN0 = 1: enable recieve
    // TXEN0 = 1: enable transmit
    // RXCIE0 = 1: enable recieve interrupt
    // UDRIE0 (transmit buffer ready) is toggled when data is ready to be sent
    UCSR0B = _BV(RXEN0)|_BV(TXEN0)|_BV(RXCIE0);

    // Set the data frame format
    // UMSEL0 = 0: set async operation
    // UPM00 = 0: no parity
    // USBS0 = 0: 1 stop bit
    // UCSZ00 = 3: 8 data bits
    // UCPOL0 = 0: ???
    UCSR0C = (3<<UCSZ00);

    // Double the prescaler frequency
    UCSR0A = _BV(U2X0);

    usart_input_write = 0;
    usart_input_read = 0;
    usart_output_write = 0;
    usart_output_read = 0;

    usart_packet_type = UNKNOWN_PACKET;
}

/*
 * Calculate a checksum by XORing bytes 
 */
static unsigned char checksum(unsigned char *data, unsigned char length)
{
    unsigned char csm = data[0];
    for (unsigned char i = 1; i < length; i++)
        csm ^= data[i];
    return csm;
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
    queue_send_byte(DLE);
    queue_send_byte(length);
    queue_send_byte(type);
    for (unsigned char i = 0; i < length; i++)
    {
        queue_send_byte(data[i]);
        if (data[i] == DLE)
            queue_send_byte(DLE);
    }
    // send the checksum of the *unpadded* data
    queue_send_byte(checksum(data, length));
    queue_send_byte(DLE);
    queue_send_byte(ETX);
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

static unsigned char unused = 0;

void send_stopexposure()
{
    queue_data(STOP_EXPOSURE, &unused, 1);
}

void send_downloadcomplete()
{
    queue_data(DOWNLOADCOMPLETE, &unused, 1);
}

void send_debug_string(char *string)
{
    queue_data(DEBUG_STRING, (unsigned char *)string, strlen(string));
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
unsigned char usart_process_buffer()
{
    // Take a local copy of usart_input_write as it can be modified by interrupts
    unsigned char temp_write = usart_input_write;

    // No new data has arrived
    if (usart_input_read == temp_write)
        return FALSE;

    // Sync to the start of a packet if necessary
    for (; usart_packet_type == UNKNOWN_PACKET && usart_input_read != temp_write; usart_input_read++)
    {
        if ( // Start of timing packet
            usart_input_buffer[(unsigned char)(usart_input_read - 1)] == DLE &&
            // End of previous packet
            usart_input_buffer[(unsigned char)(usart_input_read - 2)] == ETX &&
            usart_input_buffer[(unsigned char)(usart_input_read - 3)] == DLE)
        {
            usart_packet_type = usart_input_buffer[usart_input_read];
            // Rewind to the start of the packet
            usart_input_read -= 1;
            break;
        }
    }

    // Write bytes into the packet buffer
    for (; usart_input_read != temp_write; usart_input_read++)
    {
        // Skip the padding byte that occurs after a legitimate DLE
        // Don't loop this: we want to parse the 3rd DLE if you have
        // 4 in a row
        if (usart_input_buffer[usart_input_read] == DLE &&
            usart_input_buffer[(unsigned char)(usart_input_read - 1)] == DLE)
            usart_input_read++;

        usart_packet[usart_packet_length++] = usart_input_buffer[usart_input_read];

        // End of packet (data length is the second byte + 6 frame bytes)
        if (usart_packet_length > 2 && usart_packet_length == usart_packet[1] + 6)
        {
            // Check checksum
            unsigned char csm = checksum(&usart_packet[3], usart_packet[1]);
            if (csm == usart_packet[usart_packet_length-3])
            {
                // Handle packet
                switch(usart_packet[2])
                {
                    case START_EXPOSURE:
                        cli();
                        exposure_countdown = exposure_total = usart_packet[3];
                        sei();

                        if (usart_packet[4])
                        {
                            // Monitoring enabled: Start sync/countdown when NOTSCAN goes high
                            monitor_mode = MONITOR_START;
                        }
                        else
                        {
                            // Monitoring disabled: Wait the specified time before enabling sync/countdown
                            cli();
                            start_countdown = usart_packet[5];
                            sei();
                        }
                    break;
                    case STOP_EXPOSURE:
                        cli();
                        exposure_total = exposure_countdown = 0;
                        sei();

                        // Disable the exposure countdown immediately
                        countdown_mode = COUNTDOWN_DISABLED;

                        if (usart_packet[3])
                        {
                            // Monitoring enabled: tell the acquisition PC to
                            // stop the exposure when NOTSCAN goes high

                            // Can safely stop the exposure if the not-scan output is already HIGH
                            if (monitor_level_high)
                            {
                                monitor_mode = MONITOR_WAIT;
                                send_stopexposure();
                            }
                            else
                                monitor_mode = MONITOR_STOP;
                        }
                        else
                        {
                            // Monitoring disabled: Wait for the specified time
                            // before telling the acquisition PC to stop the exposure
                            cli();
                            stop_countdown = usart_packet[4];
                            sei();
                        }
                    break;
                    case RESET:
                        // Reset to startup state
                        cli();
                        reset_vars();
                        sei();
                    break;
                    default:
                        sprintf(error, "Unknown packet type 0x%02x - ignoring", usart_packet[2]);
                        send_debug_string(error);
                    break;
                }
            }
            else
            {
                sprintf(error, "Command 0x%02x checksum failed. Expected 0x%02x, calculated 0x%02x.",
                        usart_packet[2],
                        usart_packet[usart_packet_length-3],
                        csm);

                for (unsigned char i = 0; i < usart_packet_length; i++)
                {
                    sprintf(error, "0x%02x", usart_packet[i]);
                    send_debug_string(error);
                }
            }

            // Reset for next packet
            usart_packet_type = UNKNOWN_PACKET;
            usart_packet_length = 0;
            return TRUE;
        }
    }
    return FALSE;
}
