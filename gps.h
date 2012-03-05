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

#ifndef KARAKA_GPS_H
#define KARAKA_GPS_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

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
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    bool locked;
} timestamp;

extern volatile uint8_t gps_state;
extern volatile bool gps_record_synctime;

extern timestamp gps_last_timestamp;
extern timestamp gps_last_synctime;

void gps_init_hardware();
void gps_init_state();
void gps_process_buffer();
#endif
