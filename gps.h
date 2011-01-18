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
#define SYNCING					0	// Trying to sync packets of GPS unit
#define SETUP_GPS				1	// Send packets to GPS to set up auto broadcast.
#define CHECK_GPS_TIME_VALID	2	// Check to see timing packets from GPS are from GPS clock
#define GPS_TIME_GOOD			3	// Gps is locked and working.
#define INVALID                 4   // Invalid state

// gps_record_synctime statues
#define SYNCTIME_VALID 1
#define RECORD_THIS_PACKET 1
#define RECORD_NEXT_PACKET 2

unsigned char gps_timeout_count; // Number of counts since the last gps packet was recieved. Incremented by display.c
unsigned char gps_state;         // 
unsigned char gps_record_synctime;            // Flag to indicate whether the gps should process a packet as synctime
unsigned char gps_processing_packet;          // Flag to indicate when the gps is currently processing a packet
unsigned char gps_packet_cntr;

timestamp gps_last_timestamp;
timestamp gps_last_synctime;

unsigned char gps_packet[100];

void gps_init(void);
void gps_timeout(void);
void gps_transmit_byte(unsigned char);
unsigned char gps_receive_byte(void);
void store_error_packet(unsigned char code);
void gps_send_trimble_init(void);
void gps_send_magellan_init(void);
void gps_process_trimble_packet(void);

#endif
