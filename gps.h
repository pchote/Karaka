//***************************************************************************
//
//  File        : gps.h
//  Copyright   : 2013 Paul Chote
//  Description : Parses time information from a Trimble or Magellan serial stream
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

void gps_send_byte(uint8_t b);
void gps_initialize();
void gps_tick();

#endif
