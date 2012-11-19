//***************************************************************************
//
//  File        : display.h
//  Copyright   : 2012 Paul Chote
//  Description : Dot matrix display routines
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#ifndef KARAKA_DISPLAY_H
#define KARAKA_DISPLAY_H

#define DISPLAY_EXPOSURE_REGULAR 0
#define DISPLAY_EXPOSURE_PERCENT 1
#define DISPLAY_EXPOSURE_HIDE    2
extern uint8_t display_exposure_type;

void display_init_hardware();
void update_display();
#endif
