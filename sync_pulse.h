//***************************************************************************
//
//  File........: sync_pulse.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Sends a 512us "sync" pulse to the camera 
//                via pin XXXX using timer0
//
//***************************************************************************

#ifndef KARAKA_SYNC_PULSE_H
#define KARAKA_SYNC_PULSE_H

void sync_pulse_init(void);
void sync_pulse_trigger(void);

#endif
