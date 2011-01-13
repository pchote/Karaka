//***************************************************************************
//
//  File........: msec_timer.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: A timer to keep track of the number of milliseconds
//                that have passed.
//
//***************************************************************************

#ifndef KARAKA_MSEC_TIMER_H
#define KARAKA_MSEC_TIMER_H

void msec_timer_init(void);
void msec_timer_start(void);
void msec_timer_stop(void);

#endif
