//***************************************************************************
//
//  File        : fakecamera.h
//  Copyright   : 2012 Paul Chote
//  Description : Provides a fake camera status level to connect to the
//                monitor input for testing without a camera present
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include "main.h"

#ifndef KARAKA_FAKECAMERA_H
#define KARAKA_FAKECAMERA_H

#if HARDWARE_VERSION >= 3
void fake_camera_init_state();
void fake_camera_init_hardware();

void fake_camera_startup();
void fake_camera_shutdown();
void fake_camera_download();
#endif

#endif
