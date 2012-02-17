//***************************************************************************
//
//  File        : gps.h
//  Copyright   : 2009-2012 Johnny McClymont, Paul Chote
//  Description : Extracts timestamps from a Trimble or Magellan serial stream
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
#define GPS_UNAVAILABLE 0
#define GPS_SYNCING     1
#define GPS_ACTIVE      2

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

unsigned char gps_state;             // State of the gps listener (GPS_UNAVAILABLE, GPS_SYNCING, GPS_ACTIVE)
volatile unsigned char gps_record_synctime;   // Flag to indicate whether the gps should process a packet as synctime

timestamp gps_last_timestamp;
timestamp gps_last_synctime;

void gps_init_hardware();
void gps_send_config();
void gps_process_buffer();
#endif
