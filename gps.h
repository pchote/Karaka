//***************************************************************************
//
//  File........: gps.h
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Parses the serial stream from a Trimble or Magellan
//                GPS unit and extracts timestamp information
//  Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
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

// gps packet types
#define UNKNOWN_PACKET 0
#define TRIMBLE_PACKET 1
#define MAGELLAN_TIME_PACKET 2
#define MAGELLAN_STATUS_PACKET 3


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
unsigned char gps_timestamp_stale;   // Set to true when a second pulse has been recieved, but the time packet hasn't

timestamp gps_last_timestamp;
timestamp gps_last_synctime;

void gps_init(void);
void gps_timeout(void);
int gps_process_buffer(void);
#endif
