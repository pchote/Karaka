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
#include "gps.h"


#ifndef KARAKA_COMMAND_H
#define KARAKA_COMMAND_H

// Packet types
#define CURRENTTIME 0xA1
#define DOWNLOADTIME 0xA2
#define DEBUG_STRING 0xA3
#define DEBUG_RAW 0xA5
#define START_EXPOSURE 0xA6
#define STOP_EXPOSURE 0xA7
#define RESET 0xA8
#define DOWNLOADCOMPLETE 0xA9

void command_init_hardware();
void send_debug_fmt_P(char *fmt, ...);
void send_debug_string_P(char *string);
void send_debug_raw(unsigned char *data, unsigned char length);
void send_timestamp();
void send_downloadtimestamp();
void send_stopexposure();
void send_downloadcomplete();

unsigned char usart_process_buffer();

#endif
