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

void set_initial_state();
void trigger_countdown();
#endif
