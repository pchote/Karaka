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


#ifndef KARAKA_MAIN_H
#define KARAKA_MAIN_H

#include <string.h>
#include <stdint.h>

#define F_CPU 16000000UL  // 16 MHz

#define FALSE (0)
#define TRUE (!(FALSE))

extern uint8_t exposure_total;
extern volatile uint8_t exposure_countdown;

#define COUNTDOWN_DISABLED  0
#define COUNTDOWN_SYNCING   1
#define COUNTDOWN_ENABLED   2
#define COUNTDOWN_TRIGGERED 3
extern volatile uint8_t countdown_mode;

#define FLAG_DOWNLOAD_COMPLETE (1 << 0)
#define FLAG_STOP_EXPOSURE (1 << 1)
#define FLAG_NO_SERIAL (1 << 2)
#define FLAG_DUPLICATE_PULSE (1 << 3)
extern volatile uint8_t interrupt_flags;

void set_initial_state();
#endif
