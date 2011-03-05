//***************************************************************************
//
//  File........: msec_timer.c
//  Description.: A timer to keep track of the number of milliseconds
//                that have passed.
//  Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#ifndef KARAKA_MSEC_TIMER_H
#define KARAKA_MSEC_TIMER_H

// Overflow after 250 ticks
#define MSEC_TIMER_TICKS 6

unsigned int milliseconds;

void msec_timer_init(void);
void msec_timer_reset(void);

#endif
