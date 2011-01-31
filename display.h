//***************************************************************************
//
//  File........: display.h
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: LCD routines
//
//***************************************************************************

#ifndef KARAKA_DISPLAY_H
#define KARAKA_DISPLAY_H

#define NEWLINE			0xC0
#define CLEARLCD		0x01
#define CURSORON		0x0E
#define CURSORHOME		0x02
#define DISPLAYOFF		0x08
#define DISPLAYON		0x0F

// Reset to overflow after 7812 ticks
#define DISPLAY_TIMER_TICKS 0XF0BD

unsigned char display_cursor;
unsigned char display_last_gps_state;
unsigned char display_gps_was_locked;

void display_init(void);

#endif
