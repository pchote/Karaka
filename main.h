//***************************************************************************
//
//  File        : main.h
//  Copyright   : 2009-2012 Johnny McClymont, Paul Chote
//  Description : Main program logic
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <string.h>

#ifndef KARAKA_MAIN_H
#define KARAKA_MAIN_H

#define F_CPU 16000000UL  // 16 MHz

#define FALSE (0)
#define TRUE (!(FALSE))

unsigned char exposure_total;
volatile unsigned char exposure_countdown;

#define COUNTDOWN_DISABLED  0
#define COUNTDOWN_SYNCING   1
#define COUNTDOWN_ENABLED   2
#define COUNTDOWN_TRIGGERED 3
volatile unsigned char countdown_mode;

#define FLAG_DOWNLOAD_COMPLETE (1 << 0)
#define FLAG_STOP_EXPOSURE (1 << 1)
#define FLAG_NO_SERIAL (1 << 2)
#define FLAG_DUPLICATE_PULSE (1 << 3)
volatile unsigned char interrupt_flags;

void set_initial_state();
void trigger_countdown();
#endif
