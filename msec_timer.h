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

// Overflow after 250 ticks
#define MSEC_TIMER_TICKS 6

void msec_timer_init(void);
void msec_timer_reset(void);

#endif
