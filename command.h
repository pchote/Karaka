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
#define DEBUG 0xA3
#define EXPOSURE 0xA4


unsigned char usart_packet_type;

void command_init(void);
void send_timestamp();
void send_downloadtimestamp();
#endif
