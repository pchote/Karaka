//***************************************************************************
//
//  File........: main.h
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: ATMega128 USB Timer interface module
//
//***************************************************************************

#include <string.h>

#ifndef KARAKA_MAIN_H
#define KARAKA_MAIN_H


#define SWHIGH	1
#define SWLOW	0
#define SWREV	1

#define RXD			    PINE0   // UART 0 RECEIVE
#define TXD				PINE1   // UART 0 TRANSMIT
#define TXD1			PIND3	// UART 1 TRANSMIT
#define RXD1			PIND2	// UART 1 RECEIVE

#define GPS_PULSE		PIND0	// Input for GPS Pulse

#define CCD_PULSE		PINA0	// Output for CCD pulse

#define SWITCH_CHANGE	PINE4	// Input to indicate a switch change has occurred
#define UP				PINA1	// Input for UP button
#define LEFT			PINA2	// Input for Left button
#define DOWN			PINA3	// Input for Down button
#define RIGHT			PINA4	// Input for Right button
#define CENTER			PINA5	// Input for Center button

#define LCD_ENABLE		PINF2	// Output to enable LCD
#define LCD_READ_WRITE	PINF1	// Output to select reading or writing from LCD (Read active LOW)
#define LCD_REG_SELECT	PINF0	// Output to select register

#define LCD_DATA		PORTC	// Output data port for LCD

#define	SYNCING					0	//trying to sync packets of GPS unit
#define SETUP_GPS				1	//send packets to GPS to set up auto broadcast.
#define CHECK_GPS_TIME_VALID	2	//check to see timing packets from GPS are from GPS clock
#define GPS_TIME_GOOD			3	//gps is locked and working.


typedef struct
{
	unsigned int year;
	unsigned char month;
	unsigned char day;
	unsigned char hours;
	unsigned char minutes;
	unsigned char seconds;
} clock;

unsigned char status_register;
unsigned char control_register;
unsigned int Pulse_Counter;
unsigned int Current_Count;

clock UTCtime_lastPulse;
clock UTCtime_endOfFrame;

unsigned int milliseconds;
unsigned char GPS_state;
unsigned char wait_4_ten_second_boundary;
unsigned char wait_4_timestamp;
unsigned char wait_4_EOFtimestamp;
unsigned char check_GPS_present;
int command_cntr;
unsigned char pulse_timer;			//variable to set length of CCD pulse (number * 512uS)
unsigned char nextPacketisEOF;		//flag to inform GPS module to record next packet as EOF time

void InputSignal_Init(void);
void Delay(unsigned int millisec);
void reset_LCD(void);


#endif
