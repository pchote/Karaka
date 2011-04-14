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


/*
 * Send one byte through USART1
 */
static void transmit_byte(unsigned char data)
{
	while (!(UCSR1A & (1<<UDRE1)));	
	UDR1 = data;
	
	// Wait until transmission is complete.
	while (!(UCSR1A & (1<<TXC1)));
}

/*
 * Send Magellan initialisation strings
 */
static void send_magellan_init(void)
{
	// Enable binary timing and status packets
    unsigned char msg[38] = {'$','P','M','G','L','I',',','0','0',',',
                             'A','0','0',',','2',',','B','\r','\n','$',
                             'P','M','G','L','I',',','0','0',',','H',
                             '0','0',',','2',',','B','\r','\n'};
	for (unsigned char i = 0; i < 38; i++)
		transmit_byte(msg[i]);
}

/*
 * Send a packet mask to the Trimble unit to supress automatic packets
 */
void send_trimble_init(void)
{
    unsigned char thisPacket[7] = {0x10, 0x8E, 0xA5, 0x01, 0x00, 0x10, 0x03};
    for (unsigned char i = 0; i < 7; i++)
        transmit_byte(thisPacket[i]);
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
	DDRD |= (1<<DDD3);
	
	// Enable 2x speed
	UCSR1A = (1<<U2X1);

	// Enable transmitter.  .
	UCSR1B = (1<<RXEN1)|(1<<TXEN1)|(1<<RXCIE1)|(0<<UDRIE1);

	// Async. mode, 8N1 (8 data bits, No parity, 1 stop bit)
	UCSR1C = (0<<UMSEL1)|(0<<UPM01)|(0<<USBS1)|(3<<UCSZ10)|(0<<UCPOL1);
	
	// Set baud rate to 9600
	UBRR1H = 0x00;
	UBRR1L = 0xCF;
	
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
	gps_state = NO_GPS;
    
    gps_input_read = gps_input_write = 0;
	send_magellan_init();
    send_trimble_init();
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
	gps_last_timestamp.hours = hours;
	gps_last_timestamp.minutes = minutes;
	gps_last_timestamp.seconds = seconds;
	gps_last_timestamp.day = day;
	gps_last_timestamp.month = month;
	gps_last_timestamp.year = ((year_high << 8) & 0xFF00) | (year_low & 0x00FF);
    gps_last_timestamp.locked = locked;
    
	// Mark that we have a valid timestamp
	gps_state = GPS_TIME_GOOD;

	// Synchronize the exposure countdown with the 10 second mark
	if (exposure_syncing && (gps_last_timestamp.seconds % 10 == 0))
		exposure_syncing = FALSE;

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
 * Received data from gps serial. Add to buffer
 */
SIGNAL(SIG_UART1_RECV)
{
    gps_input_buffer[gps_input_write++] = UDR1;
}

/*
 * Process any data in the received buffer
 * Parses at most one time packet - so must be called frequently
 * Returns true if the timestamp or status info has changed
 * Note: this relies on the gps_input_buffer being 256 chars long so that
 * the data pointers automatically overflow at 256 to give a circular buffer
 */
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
    for (; gps_packet_type == UNKNOWN_PACKET && gps_input_read != temp_write; gps_input_read++)
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