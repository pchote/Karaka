//***************************************************************************
//
//  File        : command.h
//  Copyright   : 2009-2012 Johnny McClymont, Paul Chote
//  Description : Handles communication with the Acquisition PC via serial
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <stdarg.h>
#include <stdint.h>
#include "gps.h"


#ifndef KARAKA_COMMAND_H
#define KARAKA_COMMAND_H

// Packet types
typedef enum
{
    CURRENTTIME = 'A',
    DOWNLOADTIME = 'B',
    DEBUG_STRING = 'C',
    DEBUG_RAW = 'D',
    START_EXPOSURE = 'E',
    STOP_EXPOSURE = 'F',
    RESET = 'G',
    STATUSMODE = 'H',
    START_RELAY = 'R',
    START_UPGRADE = 'U'
} commandtype;

typedef enum
{
    TIMER_IDLE,
    TIMER_WAITING,
    TIMER_ALIGN,
    TIMER_EXPOSING,
    TIMER_READOUT
} TimerMode;

void command_init();
void command_send_raw(uint8_t b);
void send_debug_fmt_P(const char *fmt, ...);
void send_debug_string_P(const char *string);
void send_debug_raw(uint8_t *data, uint8_t length);
void send_timestamp();
void send_downloadtimestamp();
void send_stopexposure();
void send_status(TimerMode mode);

void usart_process_buffer();

#endif
