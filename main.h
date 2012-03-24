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
#include <stdbool.h>

#if CPU_MHZ == 16
#   define F_CPU 16000000UL
#elif CPU_MHZ == 10
#   define F_CPU 10000000UL
#else
#   error Unknown CPU Frequency
#endif

#define CPU_ATMEGA128   0
#define CPU_ATMEGA1284p 1

#define BOOTFLAG_EEPROM_OFFSET 0
#define BOOTFLAG_UPGRADE       0xFF
#define BOOTFLAG_BOOT          0x42

typedef enum
{
    MODE_PPSCOUNTER = 0,
    MODE_HIGHRES = 1,
} timingmode;
extern uint8_t timing_mode;

// Run millisecond timer without a prescaler:
//   each tick is 0.1000us at 10MHz
//   each tick is 0.0625us at 16MHz
#define STOP_MILLISECOND_TIMER (TCCR1B = _BV(WGM12))
#define START_MILLISECOND_TIMER (TCCR1B = _BV(WGM12)|_BV(CS10))

extern uint16_t exposure_total;
extern volatile uint16_t exposure_countdown;
extern uint8_t align_boundary;
extern volatile uint16_t millisecond_count;

typedef enum
{
    COUNTDOWN_DISABLED  = 0,
    COUNTDOWN_SYNCING   = 1,
    COUNTDOWN_ALIGNED   = 2,
    COUNTDOWN_ENABLED   = 3,
    COUNTDOWN_TRIGGERED = 4,
    COUNTDOWN_RELAY     = 5,
} countdownstate;
extern volatile countdownstate countdown_mode;

typedef enum
{
    FLAG_DOWNLOAD_COMPLETE = (1 << 0),
    FLAG_STOP_EXPOSURE     = (1 << 1),
    FLAG_NO_SERIAL         = (1 << 2),
    FLAG_TIME_DRIFT        = (1 << 3),
    FLAG_DUPLICATE_PPS     = (1 << 4),
    FLAG_BEGIN_ALIGN       = (1 << 5),
    FLAG_SEND_TIMESTAMP    = (1 << 6),
    FLAG_SEND_TRIGGER      = (1 << 7),
} interruptflags;

extern volatile interruptflags interrupt_flags;

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint16_t milliseconds;
    bool locked;
} timestamp;

extern volatile timestamp download_timestamp;

void trigger_restart();
#endif
