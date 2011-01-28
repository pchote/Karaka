//***************************************************************************
//
//  File........: gps.h
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Parses the serial stream from a Trimble GPS unit
//                and extracts timestamp information
//
//***************************************************************************

#include "main.h"

#ifndef KARAKA_GPS_H
#define KARAKA_GPS_H

#define ETX 0x03
#define DLE 0x10

// gps_state values
#define NO_GPS					0
#define SYNCING				    1
#define GPS_TIME_GOOD			2	// Gps is locked and working.

// gps_record_synctime statues
// set to FALSE if not recording
#define RECORD_THIS_PACKET 1
#define RECORD_NEXT_PACKET 2

// gps packet types
#define UNKNOWN_PACKET 0
#define MAGELLAN_PACKET 1
#define TRIMBLE_PACKET 2


typedef struct
{
	unsigned int year;
	unsigned char month;
	unsigned char day;
	unsigned char hours;
	unsigned char minutes;
	unsigned char seconds;
    unsigned char locked;
} timestamp;


unsigned char gps_timeout_count;     // Number of counts since the last gps packet was recieved. Incremented by display.c
unsigned char gps_state;             // State of the gps listener (NO_GPS, SYNCING, GPS_TIME_GOOD)
unsigned char gps_record_synctime;   // Flag to indicate whether the gps should process a packet as synctime
unsigned char gps_timestamp_locked; // Flag to indicate when the gps is currently processing a packet
unsigned char gps_timestamp_stale;   // Set to true when a second pulse has been recieved, but the time packet hasn't

unsigned char max_recv;

timestamp gps_last_timestamp;
timestamp gps_last_synctime;

unsigned char gps_recv_buf[4];

#define GPS_PACKET_LENGTH 32
unsigned char gps_packet_type;
unsigned char gps_packet_length;
unsigned char gps_packet[GPS_PACKET_LENGTH];
unsigned char gps_magellan_length;
unsigned char gps_magellan_locked;

void gps_init(void);
void gps_timeout(void);
void gps_transmit_byte(unsigned char);
unsigned char gps_receive_byte(void);
void store_error_packet(unsigned char code);
void gps_send_trimble_init(void);
void gps_send_magellan_init(void);
void gps_process_trimble_packet(void);

#endif
