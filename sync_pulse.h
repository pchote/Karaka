//***************************************************************************
//
//  File........: sync_pulse.c
//  Description.: Sends a 512us "sync" pulse to the camera 
//                via PINA0 using timer0
//  Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#ifndef KARAKA_SYNC_PULSE_H
#define KARAKA_SYNC_PULSE_H

void sync_pulse_init(void);
void sync_pulse_trigger(void);

#endif
