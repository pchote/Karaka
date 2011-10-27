//***************************************************************************
//
//  File        : monitor.h
//  Copyright   : 2011 Paul Chote
//  Description : Monitors the level of the cameras _SCAN_ output connected to PE4
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#ifndef KARAKA_MONITOR_H
#define KARAKA_MONITOR_H

#define MONITOR_WAIT 0x00
#define MONITOR_START 0x01
#define MONITOR_ACQUIRE 0x02
#define MONITOR_STOP 0x03

volatile char monitor_level_high;

unsigned char monitor_mode;
void monitor_init();
void monitor_tick();

#endif