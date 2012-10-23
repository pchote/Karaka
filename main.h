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

extern uint8_t exposure_total;
extern volatile uint8_t exposure_countdown;

typedef enum
{
    COUNTDOWN_DISABLED  = 0,
    COUNTDOWN_SYNCING   = 1,
    COUNTDOWN_ENABLED   = 2,
    COUNTDOWN_TRIGGERED = 3,
    COUNTDOWN_RELAY     = 4,
} countdownstate;
extern volatile countdownstate countdown_mode;

typedef enum
{
    FLAG_DOWNLOAD_COMPLETE = (1 << 0),
    FLAG_STOP_EXPOSURE     = (1 << 1),
    FLAG_NO_SERIAL         = (1 << 2),
    FLAG_DUPLICATE_PULSE   = (1 << 3)
} interruptflags;

extern volatile interruptflags interrupt_flags;

void set_initial_state();
#endif
