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

typedef enum
{
    MONITOR_WAIT    = 0,
    MONITOR_START   = 1,
    MONITOR_ACQUIRE = 2,
    MONITOR_STOP    = 3
} monitorstate;

extern volatile bool monitor_level_high;
extern volatile monitorstate monitor_mode;
extern bool monitor_simulate_camera;

void monitor_init();
void monitor_tick();

void simulate_camera_startup();
void simulate_camera_shutdown();
void simulate_camera_download();
#endif