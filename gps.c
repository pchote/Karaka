//***************************************************************************
//
//  File........: gps.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Parses the serial stream from a Trimble GPS unit
//                and extracts timestamp information
//
//***************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "main.h"
#include "display.h"
#include "gps.h"
#include "command.h"
	
/*
 * Initialise the gps listener on USART1
 */
void gps_init(void)
{
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
    gps_recv_buf[3] = gps_recv_buf[2] = gps_recv_buf[1] = gps_recv_buf[0] = 0;
    gps_packet_length = 0;
    gps_magellan_length = 0;
    gps_magellan_locked = FALSE;
	gps_record_synctime = FALSE;
	gps_state = NO_GPS;
    
	gps_send_magellan_init();
    gps_send_trimble_init();
}

/*
 * Send one byte through USART1
 */
void gps_transmit_byte(unsigned char data)
{
	while (!(UCSR1A & (1<<UDRE1)));	
	UDR1 = data;
	
	// Wait until transmission is complete.
	while (!(UCSR1A & (1<<TXC1)));
}

/*
 * Receive one byte through USART1
 */
unsigned char gps_receive_byte(void)
{
	while (!(UCSR1A & (1<<RXC1)));
	return UDR1;
}

/*
 * Send Magellan initialisation strings
 */
void gps_send_magellan_init(void)
{
	// Enable binary timing and status packets
    unsigned char msg[38] = {'$','P','M','G','L','I',',','0','0',',',
                             'A','0','0',',','2',',','B','\r','\n','$',
                             'P','M','G','L','I',',','0','0',',','H',
                             '0','0',',','2',',','B','\r','\n'};
	for (unsigned char i = 0; i < 38; i++)
		gps_transmit_byte(msg[i]);
}

/*
 * Send a packet mask to the Trimble unit to supress automatic packets
 */
void gps_send_trimble_init(void)
{
    unsigned char thisPacket[7] = {0x10, 0x8E, 0xA5, 0x01, 0x00, 0x10, 0x03};
    for (unsigned char i = 0; i < 7; i++)
        gps_transmit_byte(thisPacket[i]);
}

void gps_timeout(void)
{
	gps_state = NO_GPS;
	error_state |= GPS_SERIAL_LOST;
	
	gps_packet_type = UNKNOWN_PACKET;
	gps_packet_length = 0;
	
	error_state = error_state & 0xFE;
	gps_timeout_count = 0;
}


static void gps_set_time(unsigned char hours,
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
	gps_timestamp_stale = FALSE;
	gps_state = GPS_TIME_GOOD;

	// Synchronize the exposure countdown with the 10 second mark
	if (exposure_syncing && (gps_last_timestamp.seconds % 10 == 0))
		exposure_syncing = FALSE;

	if (gps_record_synctime == RECORD_THIS_PACKET)
	{
		gps_last_synctime.seconds = gps_last_timestamp.seconds;
		gps_last_synctime.minutes = gps_last_timestamp.minutes;
		gps_last_synctime.hours = gps_last_timestamp.hours;
		gps_last_synctime.day = gps_last_timestamp.day;
		gps_last_synctime.month = gps_last_timestamp.month;
		gps_last_synctime.year = gps_last_timestamp.year;
        gps_last_synctime.locked = gps_last_timestamp.locked;
		gps_record_synctime = FALSE;
	}
}


/* 
 * Interrupt service routine for Data received from UART
 * Receives one byte of data from the USART. Determines
 * command char received and execute code as required.
 */
SIGNAL(SIG_UART1_RECV)
{
	// Signal that the gps is alive
	gps_timeout_count = 0;
	
	// Add byte to history buffer
    gps_recv_buf[3] = gps_recv_buf[2];
    gps_recv_buf[2] = gps_recv_buf[1];
    gps_recv_buf[1] = gps_recv_buf[0];
    gps_recv_buf[0] = gps_receive_byte();
    
    // Detect packet type from boundary between packets
    if (gps_packet_type == UNKNOWN_PACKET)
    {
        if (gps_recv_buf[2] == 0x0A &&
            gps_recv_buf[1] == '$' &&
            gps_recv_buf[0] == '$')
        {
            gps_packet_type = MAGELLAN_PACKET;
            gps_packet_length = 1;
            gps_packet[0] = '$';
        }
        
        if (gps_recv_buf[3] != DLE &&
            gps_recv_buf[2] == DLE &&
            gps_recv_buf[1] == ETX &&
            gps_recv_buf[0] == DLE)
        {
            gps_packet_type = TRIMBLE_PACKET;
            gps_packet_length = 0;
        }
    }
    
    if (gps_packet_type == TRIMBLE_PACKET)
    {
        // Ignore padding byte
        if (gps_recv_buf[1] == DLE && gps_recv_buf[0] == DLE)
            return;
        
        // Buffer is full (something went wrong)
        if (gps_packet_length == GPS_PACKET_LENGTH)
        {
    		gps_packet_type = UNKNOWN_PACKET;
            return;
        }
        
    	// Append the byte to the stored packet
    	gps_packet[gps_packet_length++] = gps_recv_buf[0];
        
        // End of packet
    	if (gps_packet_length > 3 &&
    	        gps_packet[gps_packet_length-1] == ETX && 
    	        gps_packet[gps_packet_length-2] == DLE &&
    	        gps_packet[gps_packet_length-3] != DLE)
    	{
        	gps_timestamp_locked = TRUE;

        	// Time packet
        	if (gps_packet[1] == 0x8F && gps_packet[2] == 0xAB)
        		gps_set_time(gps_packet[14], // hours
        					 gps_packet[13], // minutes
        					 gps_packet[12], // seconds
        					 gps_packet[15], // day
        					 gps_packet[16], // month
        					 gps_packet[17], // year (high byte)
        					 gps_packet[18], // year (low byte)
                             gps_packet[11] == 0x03 ? TRUE : FALSE); // lock status
        	
    		// Reset for the next packet
    		gps_packet_type = UNKNOWN_PACKET;
    		gps_timestamp_locked = FALSE;
    		if (gps_record_synctime == RECORD_NEXT_PACKET)
    			gps_record_synctime = RECORD_THIS_PACKET;
    	}
    }
    
    if (gps_packet_type == MAGELLAN_PACKET)
	{
        // Determine packet type
        if (gps_packet_length == 2)
    	{
    		switch (gps_recv_buf[0])
    		{
    		    case 'A': // Time
    			    gps_magellan_length = 13;
                break;
                case 'H': // Status
        			gps_magellan_length = 16;
                break;
                default: // Wait for the next packet
                    gps_packet_type = UNKNOWN_PACKET;
                    return;
                break;
    		}
        }
        
        // Store the packet
    	gps_packet[gps_packet_length++] = gps_recv_buf[0];
    	
    	// End of packet
    	if (gps_packet_length == gps_magellan_length)
    	{
    		gps_timestamp_locked = TRUE;

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
					switch (gps_packet[2])
					{
						case 'H': // Status
                            gps_magellan_locked = (gps_packet[13] == 6);
						break;
						case 'A': // Time
							gps_set_time(gps_packet[4], // hours
										 gps_packet[5], // minutes
										 gps_packet[6], // seconds
										 gps_packet[7], // day
										 gps_packet[8], // month
										 gps_packet[9], // year (high byte)
										 gps_packet[10],// year (low byte)
                                         gps_magellan_locked); // lock status
						break;
					}
    			}
    		}
    		
    		// Reset buffer for the next packet
    		gps_packet_type = UNKNOWN_PACKET;
    		gps_timestamp_locked = FALSE;
    		if (gps_record_synctime == RECORD_NEXT_PACKET)
    			gps_record_synctime = RECORD_THIS_PACKET;
    	}
	}
}