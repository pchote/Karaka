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

unsigned char cursor_ptr;
unsigned char putHeader;

void display_init(void);
void display_reset_header(void);
void display_write_byte(unsigned char value);
void display_write_control(unsigned char value, int time);
void display_write_string(const char *s);
void display_write_number(int number, unsigned char places);
void display_write_header(const char *msg);
void display_set_state(unsigned char LCD_state);

#endif
