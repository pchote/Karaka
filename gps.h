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
typedef enum
{
    GPS_UNAVAILABLE = 0,
    GPS_SYNCING     = 1,
    GPS_ACTIVE      = 2
} gpsstate;

// gps packet types
typedef enum
{
    UNKNOWN_PACKET         = 0,
    TRIMBLE_PACKET         = 1,
    MAGELLAN_TIME_PACKET   = 2,
    MAGELLAN_STATUS_PACKET = 3
} gpspackettype;

extern volatile gpsstate gps_state;
extern volatile bool gps_record_trigger;

extern timestamp gps_last_timestamp;

void gps_send_raw(uint8_t b);
void gps_init_hardware();
void gps_init_state();
void gps_process_buffer();
#endif
