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
#define CURRENTTIME 'A'
#define DOWNLOADTIME 'B'
#define DEBUG_STRING 'C'
#define DEBUG_RAW 'D'
#define START_EXPOSURE 'E'
#define STOP_EXPOSURE 'F'
#define RESET 'G'
#define DOWNLOADCOMPLETE 'H'

void command_init_hardware();
void command_init_state();
void send_debug_fmt_P(char *fmt, ...);
void send_debug_string_P(char *string);
void send_debug_raw(uint8_t *data, uint8_t length);
void send_timestamp();
void send_downloadtimestamp();
void send_stopexposure();
void send_downloadcomplete();

void usart_process_buffer();

#endif
