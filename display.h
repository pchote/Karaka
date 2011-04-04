//***************************************************************************
//
//  File........: display.h
//  Description.: LCD display routines
//  Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#ifndef KARAKA_DISPLAY_H
#define KARAKA_DISPLAY_H
/*
 * Instruction list
 */
 
// Initialize with 8 bit data, 2 lines, 5x7 font
#define INITIALIZE    0x38
#define DISPLAY_CLEAR 0x01
#define INITIALIZEB   0x0C
#define CURSOR_HOME   0x02
#define NEWLINE       0xC0

// Display modes
#define DISPLAY_OFF       0x08
#define DISPLAY_ON_CURSOR 0x0F
#define DISPLAY_ON        0x0C

unsigned char display_cursor;
unsigned char display_last_gps_state;
unsigned char display_gps_was_locked;

void display_init(void);
void update_display();
#endif
