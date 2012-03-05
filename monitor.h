//***************************************************************************
//
//  File        : monitor.h
//  Copyright   : 2011-2012 Paul Chote
//  Description : Monitors the level of the cameras _SCAN_ output connected to PE4
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#ifndef KARAKA_MONITOR_H
#define KARAKA_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

#define MONITOR_WAIT 0x00
#define MONITOR_START 0x01
#define MONITOR_ACQUIRE 0x02
#define MONITOR_STOP 0x03

extern volatile bool monitor_level_high;
extern volatile uint8_t monitor_mode;
extern bool monitor_simulate_camera;

void monitor_init_state();
void monitor_init_hardware();
void monitor_tick();

void simulate_camera_enable(bool enabled);
void simulate_camera_startup();
void simulate_camera_shutdown();
void simulate_camera_download();
#endif