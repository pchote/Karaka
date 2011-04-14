//***************************************************************************
//
//  File........: command.h
//  Description.: Responds to user commands over usb
//  Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include "gps.h"


#ifndef KARAKA_COMMAND_H
#define KARAKA_COMMAND_H

// Packet types
#define CURRENTTIME 0xA1
#define DOWNLOADTIME 0xA2
#define DEBUG_STRING 0xA3
#define EXPOSURE 0xA4
#define DEBUG_RAW 0xA5

void command_init(void);
void send_debug_string(char *string);
void send_debug_raw(unsigned char *data, unsigned char length);
void send_timestamp();
void send_downloadtimestamp();

unsigned char usart_process_buffer();

#endif
