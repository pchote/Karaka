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

enum timing_mode
{
    MODE_PPSCOUNTER = 0,
    MODE_HIGHRES = 1,
};

extern uint8_t timing_mode;

// Run millisecond timer without a prescaler:
//   each tick is 0.1000us at 10MHz
//   each tick is 0.0625us at 16MHz
#define STOP_MILLISECOND_TIMER (TCCR1B = _BV(WGM12))
#define START_MILLISECOND_TIMER (TCCR1B = _BV(WGM12)|_BV(CS10))

// Bootloader bipass EEPROM parameters - must match definitions in bootloader.c
#define BOOTLOADER_EEPROM_OFFSET (uint8_t *)(0x00)
#define BIPASS_DISABLED 0xFF
#define BIPASS_ENABLED 0x42

// Relay mode EEPROM parameters
#define RELAY_EEPROM_OFFSET (uint8_t *)(0x01)
#define RELAY_DISABLED 0xFF
#define RELAY_ENABLED 0x42

extern uint16_t exposure_total;
extern volatile uint16_t exposure_countdown;
extern uint8_t align_boundary;
extern volatile uint16_t millisecond_count;

enum count_status
{
    COUNTDOWN_DISABLED  = 0,
    COUNTDOWN_SYNCING   = 1,
    COUNTDOWN_ALIGNED   = 2,
    COUNTDOWN_ENABLED   = 3,
    COUNTDOWN_TRIGGERED = 4,
} countdownstate;
extern volatile enum count_status countdown_mode;

enum interrupt_flags
{
    FLAG_SEND_STATUS       = (1 << 0),
    FLAG_STOP_EXPOSURE     = (1 << 1),
    FLAG_TIME_DRIFT        = (1 << 2),
    FLAG_DUPLICATE_PPS     = (1 << 3),
    FLAG_SEND_TIMESTAMP    = (1 << 4),
    FLAG_SEND_TRIGGER      = (1 << 5),
};

extern volatile enum interrupt_flags interrupt_flags;

struct timestamp
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint16_t milliseconds;
    bool locked;
    uint16_t exposure_progress;
};

extern volatile struct timestamp download_timestamp;
extern struct timestamp current_timestamp;

enum timer_status
{
    TIMER_IDLE,
    TIMER_WAITING,
    TIMER_ALIGN,
    TIMER_EXPOSING,
    TIMER_READOUT,
    TIMER_RELAY
};

extern volatile enum timer_status timer_status;
void set_timer_status(enum timer_status status);

enum gps_status
{
    GPS_UNAVAILABLE = 0,
    GPS_SYNCING     = 1,
    GPS_ACTIVE      = 2
};

extern volatile enum gps_status gps_status;
void set_gps_status(enum gps_status status);

void set_time(struct timestamp *t);

#endif
