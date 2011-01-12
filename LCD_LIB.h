//***************************************************************************
//
//  File........: LCD_LIB.h
//
//  Author(s)...: Johnny McClymont
//
//  Target(s)...: ATmega128
//
//  Compiler....: AVR-GCC 3.3.1; avr-libc 1.0
//
//  Description.: LCD routines
//
//  Revisions...: 1.0
//
//  YYYYMMDD - VER. - COMMENT                                       - SIGN.
//
//  20090606 - 1.0  - Created                                       - J.McClymont
//
//***************************************************************************

void LCD_init(void);
void LCD_ClearDisplay(void);
void LCD_WriteData(unsigned char value);
void LCD_WriteControl(unsigned char value, int time);
void LCD_sendmsg (const char *s);
void LCD_sendDecimal(int number, unsigned char places);
void update_LCD(unsigned char LCD_state);

unsigned char cursor_ptr;
unsigned char putHeader;


#define NEWLINE			0xC0
#define CLEARLCD		0x01
#define CURSORON		0x0E
#define CURSORHOME		0x02
#define DISPLAYOFF		0x08;
#define DISPLAYON		0x0F







