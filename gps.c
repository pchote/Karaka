//***************************************************************************
//
//  File........: gps.c
//  Description.: Parses the serial stream from a Trimble or Magellan
//                GPS unit and extracts timestamp information
//  Copyright...: 2009-2011 Johnny McClymont, Paul Chote
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
#include "display.h"
#include "gps.h"
#include "command.h"

static unsigned char gps_magellan_length;
static unsigned char gps_magellan_locked;

// NOTE: If buffer length is changed the read/write offsets
// must be changed to int, and explicit overflow code added
static unsigned char gps_input_buffer[256];
static unsigned char gps_input_read;
static volatile unsigned char gps_input_write;


#define GPS_PACKET_LENGTH 32
static unsigned char gps_packet_type;
static unsigned char gps_packet_length;
static unsigned char gps_packet[GPS_PACKET_LENGTH];

static unsigned char gps_output_buffer[256];
static volatile unsigned char gps_output_read;
static volatile unsigned char gps_output_write;

/*
 * Add a byte to the send queue and start sending data if necessary
 */
static void queue_send_byte(unsigned char b)
{
	// Don't overwrite data that hasn't been sent yet
    while (gps_output_write == gps_output_read - 1);

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
SIGNAL(SIG_UART1_DATA)
{
	if(gps_output_write != gps_output_read)
		UDR1 = gps_output_buffer[gps_output_read++];
	
	// Ran out of data to send - disable the interrupt
	if(gps_output_write == gps_output_read) 
		UCSR1B &= ~_BV(UDRIE1);
}

/*
 * Queue data to the GPS
 */
static void queue_bytes(unsigned char *data, unsigned char length)
{
	for (unsigned char i = 0; i < length; i++)
		queue_send_byte(data[i]);
}

/*
 * Initialise the gps listener on USART1
 */
void gps_init(void)
{
	// Configure Port D.
	// Used for communication with the GPS
	// Pin 0 is set to trigger SIG_INTERRUPT0 when a pulse from the gps arrives
	// Set TXD1 (port D, pin 3) as an output
	DDRD |= _BV(DDD3);
	
	// Enable 2x speed
	UCSR1A = _BV(U2X1);

	// Set USART capabilities
	// RXEN1 = 1: enable recieve
	// TXEN1 = 1: enable transmit
	// RXCIE1 = 1: enable recieve interrupt
	// UDRIE1 (transmit buffer ready) is toggled when data is ready to be sent
	UCSR1B = _BV(RXEN1)|_BV(TXEN1)|_BV(RXCIE1);

	// Async. mode, 8N1 (8 data bits, No parity, 1 stop bit)
	UCSR1C = (3<<UCSZ10);
	
	// Set baud rate to 9600
	UBRR1H = 0x00;
	UBRR1L = 0xCF;
	
    // Initialize timer1 to monitor GPS loss
    TCCR1A = 0x00;
    // Set prescaler to 1/1024: 64us per tick
    TCCR1B = _BV(CS10)|_BV(CS12);
    TIMSK |= _BV(TOIE1);
    TCNT1 = 0x0BDB; // Overflow after 62500 ticks: 4.0s

	// Initialise the clocks to zero
	gps_last_timestamp.seconds = 0;
	gps_last_timestamp.minutes = 0;
	gps_last_timestamp.hours = 0;
	gps_last_timestamp.day = 0;
	gps_last_timestamp.month = 0;
	gps_last_timestamp.year = 0;
	
	gps_last_synctime.seconds = 0;
	gps_last_synctime.minutes = 0;
	gps_last_synctime.hours = 0;
	gps_last_synctime.day = 0;
	gps_last_synctime.month = 0;
	gps_last_synctime.year = 0;
	
	gps_packet_type = UNKNOWN_PACKET;
    gps_packet_length = 0;
    gps_magellan_length = 0;
    gps_magellan_locked = FALSE;
	gps_record_synctime = FALSE;
	gps_state = GPS_UNAVAILABLE;
    
    gps_input_read = 0;
    gps_input_write = 0;
    gps_output_read = 0;
    gps_output_write = 0;

    // Init magellan

    // Disable the messages that may have been left enabled by the OEM software
    char mgl_initbuf[22];
    char *mgl_disable[] = {"G00", "B00", "B02", "D00", "E00", "F02", "R04", "S01"};
    for (unsigned char i = 0; i < sizeof(mgl_disable)/sizeof(*mgl_disable); i++)
    {
        sprintf(mgl_initbuf, "$PMGLI,00,%s,0,A\r\n", mgl_disable[i]);
        queue_bytes((unsigned char *)mgl_initbuf, strlen(mgl_initbuf));
    }

    // Enable the messages that we want to use
    char *mgl_enable[] = {"A00", "H00"};
    for (unsigned char i = 0; i < sizeof(mgl_enable)/sizeof(*mgl_enable); i++)
    {
        sprintf(mgl_initbuf, "$PMGLI,00,%s,2,B\r\n", mgl_enable[i]);
        queue_bytes((unsigned char *)mgl_initbuf, strlen(mgl_initbuf));
    }

    // Init trimble
	unsigned char trimble_init[7] = {0x10, 0x8E, 0xA5, 0x01, 0x00, 0x10, 0x03};
    queue_bytes(trimble_init, 7);
}

static void set_time(unsigned char hours,
						 unsigned char minutes,
						 unsigned char seconds,
						 unsigned char day,
						 unsigned char month,
						 unsigned char year_high,
						 unsigned char year_low,
						 unsigned char locked)
{
    // Enable the counter for the next PPS pulse
    if (countdown_mode == COUNTDOWN_TRIGGERED)
        countdown_mode = COUNTDOWN_ENABLED;
    else if (countdown_mode == COUNTDOWN_ENABLED)
    {
        // We should always recieve the PPS pulse before the time packet
        cli();
        trigger_countdown();
        sei();
        send_debug_string("Missing PPS pulse: forcing countdown");
    }

	gps_last_timestamp.hours = hours;
	gps_last_timestamp.minutes = minutes;
	gps_last_timestamp.seconds = seconds;
	gps_last_timestamp.day = day;
	gps_last_timestamp.month = month;
	gps_last_timestamp.year = ((year_high << 8) & 0xFF00) | (year_low & 0x00FF);
    gps_last_timestamp.locked = locked;
    
	// Mark that we have a valid timestamp
	gps_state = GPS_ACTIVE;

	// Synchronise the exposure with the edge of a minute
	if (countdown_mode == COUNTDOWN_SYNCING && (gps_last_timestamp.seconds % exposure_total == 0))
		countdown_mode = COUNTDOWN_ENABLED;

	if (gps_record_synctime)
	{
        cli();
		gps_last_synctime.seconds = gps_last_timestamp.seconds;
		gps_last_synctime.minutes = gps_last_timestamp.minutes;
		gps_last_synctime.hours = gps_last_timestamp.hours;
		gps_last_synctime.day = gps_last_timestamp.day;
		gps_last_synctime.month = gps_last_timestamp.month;
		gps_last_synctime.year = gps_last_timestamp.year;
        gps_last_synctime.locked = gps_last_timestamp.locked;
		gps_record_synctime = FALSE;
        sei();
        
		send_downloadtimestamp();
	}
	send_timestamp();
}


/*
 * Haven't recieved any serial data in 4.0 seconds
 * The GPS has probably died
 */
SIGNAL(SIG_OVERFLOW1)
{
    gps_state = GPS_UNAVAILABLE;
}

/*
 * Received data from gps serial. Add to buffer
 */
SIGNAL(SIG_UART1_RECV)
{
    // Reset timeout countdown
    TCNT1 = 0x0BDB;

    // Update status if necessary
    if (gps_state == GPS_UNAVAILABLE)
        gps_state = GPS_SYNCING;
    gps_input_buffer[gps_input_write++] = UDR1;
}

/*
 * Process any data in the received buffer
 * Parses at most one time packet - so must be called frequently
 * Returns true if the timestamp or status info has changed
 * Note: this relies on the gps_input_buffer being 256 chars long so that
 * the data pointers automatically overflow at 256 to give a circular buffer
 */
 
static unsigned char bytes_to_sync = 0;
unsigned char gps_process_buffer()
{
    // Take a local copy of gps_input_write as it can be modified by interrupts
    unsigned char temp_write = gps_input_write;
    
    // Buffer to store an error string for send_debug()
    char error[128];
    
    // No new data has arrived
    if (gps_input_read == temp_write)
        return FALSE;
    
    // Sync to the start of a packet if necessary
    for (; gps_packet_type == UNKNOWN_PACKET && gps_input_read != temp_write; gps_input_read++, bytes_to_sync++)
    {
        // Magellan packet            
        if (gps_input_buffer[(unsigned char)(gps_input_read - 1)] == '$' &&
            gps_input_buffer[(unsigned char)(gps_input_read - 2)] == '$' &&
            // End of previous packet
            gps_input_buffer[(unsigned char)(gps_input_read - 3)] == 0x0A)
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
                send_debug_string("Unknown magellan packet");
                continue;
            }

            if (bytes_to_sync > 3)
            {
                sprintf(error, "Skipped %d bytes while syncing", bytes_to_sync);
                send_debug_string(error);
            }
            bytes_to_sync = 0;
            
            // Rewind to the start of the packet
            gps_input_read -= 2;
            break;
        }
        
        // Trimble
        if ( // Start of timing packet
            gps_input_buffer[gps_input_read] == 0xAB &&
            gps_input_buffer[(unsigned char)(gps_input_read - 1)] == 0x8F &&
            gps_input_buffer[(unsigned char)(gps_input_read - 2)] == DLE &&
            // End of previous packet
            gps_input_buffer[(unsigned char)(gps_input_read - 3)] == ETX &&
            gps_input_buffer[(unsigned char)(gps_input_read - 4)] == DLE)
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
        return FALSE;
        case TRIMBLE_PACKET:
            // Write bytes into the packet buffer
            for (; gps_input_read != temp_write; gps_input_read++)
            {
                // Skip the padding byte that occurs after a legitimate DLE
                // Don't loop this: we want to parse the 3rd DLE if you have
                // 4 in a row
                if (gps_input_buffer[gps_input_read] == DLE &&
                    gps_input_buffer[(unsigned char)(gps_input_read - 1)] == DLE)
                    gps_input_read++;

                gps_packet[gps_packet_length++] = gps_input_buffer[gps_input_read];
            
                // End of packet (Trimble timing packet is 21 bytes)
                if (gps_packet_length == 22)
                {
                    // Sanity check: Ensure the packet ends correctly
                    if (gps_packet[21] == ETX && 
            	        gps_packet[20] == DLE &&
            	        gps_packet[19] != DLE)
            	    {
                		set_time(gps_packet[14], // hours
                				 gps_packet[13], // minutes
                				 gps_packet[12], // seconds
                				 gps_packet[15], // day
                				 gps_packet[16], // month
                				 gps_packet[17], // year (high byte)
                				 gps_packet[18], // year (low byte)
                                 gps_packet[11] == 0x03 ? TRUE : FALSE); // lock status
            	    }
            	    else
            	    {
        	            // Bad packet - uh oh.
        	            // Do we want to set a flag somewhere?
            	    }
            	    // Reset for next packet
            	    gps_packet_type = UNKNOWN_PACKET;
                    gps_packet_length = 0;
                    return TRUE;
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
                    unsigned char updated = FALSE;
        			// Check that the packet is valid.
        			// A valid packet will have the final byte as a linefeed (0x0A)
        			// and the second-to-last byte will be a checksum which will match
        			// the XORed bytes between the $$ and checksum.
        			//   gps_packet_length - 1 is the linefeed
        			//   gps_packet_length - 2 is the checksum byte
        			if (gps_packet[gps_packet_length-1] == 0x0A)
        			{
        				unsigned char csm = gps_packet[2];
        				for (int i = 3; i < gps_packet_length-2; i++)
        					csm ^= gps_packet[i];

        				// Verify the checksum
        				if (csm == gps_packet[gps_packet_length-2])
        				{
        					if (gps_packet_type == MAGELLAN_TIME_PACKET)
        					{
                                updated = TRUE;
        					    set_time(gps_packet[4], // hours
        								gps_packet[5], // minutes
    									 gps_packet[6], // seconds
    									 gps_packet[7], // day
    									 gps_packet[8], // month
    									 gps_packet[9], // year (high byte)
    									 gps_packet[10],// year (low byte)
                                         gps_magellan_locked); // lock status
        					}
        					else if (gps_packet_type == MAGELLAN_STATUS_PACKET) // Status packet
        					{
        					    if (gps_magellan_locked != (gps_packet[13] == 6))
        					    {
        					        gps_magellan_locked = (gps_packet[13] == 6);
                                    updated = TRUE;
        					    }
        					}
        					else
        					{
        			            send_debug_string("Bad GPS packet");
                                send_debug_raw(gps_packet, gps_packet_length);
                			}
            			}
            			else
            			{
                            sprintf(error,"GPS Checksum failed. Got 0x%02x, expected 0x%02x", csm, gps_packet[gps_packet_length-2]);
            			    send_debug_string(error);
                            send_debug_raw(gps_packet, gps_packet_length);
            			}
            		}
            		else
        			{
        			    send_debug_string("Bad GPS packet");
                        send_debug_raw(gps_packet, gps_packet_length);
        			}
    		
            		// Reset buffer for the next packet
            		gps_packet_type = UNKNOWN_PACKET;
                    gps_packet_length = 0;
                    return updated;
            	}
            }
        break;
    }
    return FALSE;
}