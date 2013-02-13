//***************************************************************************
//
//  File        : camera.h
//  Copyright   : 2013 Paul Chote
//  Description : Monitors the level of the cameras _SCAN_ output connected to PE4
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#ifndef KARAKA_CAMERA_H
#define KARAKA_CAMERA_H

#include <stdint.h>
#include <stdbool.h>

void camera_initialize();
void camera_tick();

void camera_start_exposing(bool monitor_status);
void camera_stop_exposing();
void camera_trigger_readout();

#endif