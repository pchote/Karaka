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

#define DISPLAY0 PB1
#define DISPLAY1 PB2
#define DISPLAY2 PB3
#define DISPLAY3 PB4

volatile unsigned char display_brightness;

void display_init();
void update_display_brightness();
void update_display();
#endif
