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

// gps_state values
#define NO_GPS					0
#define SYNCING				    1
#define GPS_TIME_GOOD			3	// Gps is locked and working.
#define INVALID                 4   // Invalid state

// gps_record_synctime statues
#define SYNCTIME_VALID 1
#define RECORD_THIS_PACKET 1
#define RECORD_NEXT_PACKET 2

// gps packet types
#define UNKNOWN_PACKET 0
#define MAGELLAN_STATUS_PACKET 1
#define MAGELLAN_TIME_PACKET 2

unsigned char gps_timeout_count; // Number of counts since the last gps packet was recieved. Incremented by display.c
unsigned char gps_state;         // 
unsigned char gps_record_synctime;            // Flag to indicate whether the gps should process a packet as synctime
unsigned char gps_processing_packet;          // Flag to indicate when the gps is currently processing a packet

timestamp gps_last_timestamp;
timestamp gps_last_synctime;

unsigned char gps_packet_type;
unsigned char gps_packet_received;
unsigned char gps_packet_length;
unsigned char gps_packet[32];

void gps_init(void);
void gps_timeout(void);
void gps_transmit_byte(unsigned char);
unsigned char gps_receive_byte(void);
void store_error_packet(unsigned char code);
void gps_send_trimble_init(void);
void gps_send_magellan_init(void);
void gps_process_trimble_packet(void);

#endif
