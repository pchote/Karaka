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
#define DISPLAYOFF		0x08;
#define DISPLAYON		0x0F

unsigned char cursor_ptr;
unsigned char putHeader;

void LCD_init(void);
void LCD_ClearDisplay(void);
void LCD_WriteData(unsigned char value);
void LCD_WriteControl(unsigned char value, int time);
void LCD_sendmsg (const char *s);
void LCD_sendDecimal(int number, unsigned char places);
void update_LCD(unsigned char LCD_state);
void start_timer1(void);

#endif
