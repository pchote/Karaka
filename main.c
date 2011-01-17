//***************************************************************************
//
//  File........: main.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: ATMega128 USB timer card.  Interface between GPS module, 
//				  CCD camera, and Laptop
//
//***************************************************************************

#include <avr/interrupt.h>
#include "main.h"
#include "msec_timer.h"
#include "sync_pulse.h"
#include "gps.h"
#include "display.h"
#include "command.h"
	
/*
 * Initialise the unit and wait for interrupts.
 */
int main(void)
{
	// Configure port A.
	// Pin 0 is used to output the CCD sync pulse.
	// Pins 1-5 used to input the (unimplemented) button controls
	DDRA = (1<<CCD_PULSE)|(0<<UP)|(0<<LEFT)|(0<<DOWN)|(0<<RIGHT)|(0<<CENTER);
	// Set the pin HIGH so that the output bnc is set LOW.
	PORTA = (1<<CCD_PULSE);	
	
	// Configure port B; unused by code
	PORTB = 0x00;
	
	// Configure port C.
	// All ports are output to the LCD
	DDRC = 0xFF;
	PORTC = 0x00;
	
	// Configure Port D.
	// Used for communication with the GPS
	// Pin 0 is set to trigger SIG_INTERRUPT0 when a pulse from the gps arrives
	DDRD &= ~(1<<GPS_PULSE);
	PORTD = 0x00;
	
	// Configure Port E.
	// Pins 0 and 1 are used for serial communication (via usb to the host machine?)
	// Pin 6 is used as an input to indicate that the (unimplemented) button controls have changed
	DDRE = (0<<SWITCH_CHANGE)|(1<<TXD)|(0<<RXD);
	PORTE = 0x00;
	
	// Configure Port F.
	// Used to send state to the LCD
	DDRF = (1<<LCD_ENABLE)|(1<<LCD_READ_WRITE)|(1<<LCD_REG_SELECT);		//set port F
	PORTF = 0x00;

	// Initialise global variables
	exposure_total = 0;
	exposure_current = 0;
	status_register = 0;
	control_register = 0;
	wait_4_ten_second_boundary = TRUE;
	wait_4_timestamp = FALSE;
	gps_should_wait_for_synctime = FALSE;

	// Initialise the hardware units
	command_init();
	gps_init();
	msec_timer_init();		// Millisecond counter
	sync_pulse_init();		// Pulse timer
	display_init();
	
	// initialise all external interupts to be rising edge triggered
	EICRA = (1<<ISC31)|(1<<ISC30)|(1<<ISC21)|(1<<ISC20)|(1<<ISC11)|(1<<ISC10)|(1<<ISC01)|(0<<ISC00); 
	EICRB = (0<<ISC71)|(1<<ISC70)|(0<<ISC61)|(1<<ISC60)|(0<<ISC51)|(1<<ISC50)|(0<<ISC41)|(1<<ISC40);
	
	// Enable interrupt 0 on PIND0 for incoming gps pulses	
	EIMSK = (1<<INT0);
	
	// Enable interrupts
	sei();

	// Wait. Main program logic is handled in interrupts.
	while(1){}
}

/*
 * GPS time pulse interrupt handler
 * Fired when a `second boundary' time pulse is recieved via the attached BNC cable
 */
SIGNAL(SIG_INTERRUPT0)
{
	// Don't count down unless we have a valid exposure time, and the GPS is locked
	if(exposure_total != 0 && gps_state == GPS_TIME_GOOD)
	{
		msec_timer_reset();
		
		// Are we waiting for a 10s boundary before we start counting down?
		if (!wait_4_ten_second_boundary)
		{
			wait_4_timestamp = TRUE;
			exposure_current++;
			
			// End of exposure - send a syncpulse to the camera
			// and store a flag so the gps can save the synctime.
			if (exposure_current == exposure_total)
			{
				exposure_current = 0;
				gps_should_wait_for_synctime = TRUE;
				
				if (gps_packet_proccessed)
				{
					gps_record_synctime = TRUE;	//set end of frame flag so we know to record next time stamp as end of frame time
					nextPacketisEOF = FALSE;
				}
				else
					nextPacketisEOF = TRUE;
				
				sync_pulse_trigger();
			}
		}
	}
}

/*
 * Convert an ascii character '0'-'F' to a nibble
 */
unsigned char ascii_to_nibble(unsigned char a)
{
	// '0' - '9'
	if (a >= 0x30 && a <= 0x39)
		return a - 0x30;
	// 'A' - 'F'
	else if (a >= 0x41 && a <= 0x46)
		return a - 0x37;
	// 'a' - 'f'
	else if (a >= 0x61 && a <= 0x66)
		return a - 0x57;
	else return 0;
}	

/*
 * Convert a nibble in the range 0-F to ascii
 */
unsigned char nibble_to_ascii(unsigned char n)
{
	// Ignore the high nibble
	n &= 0x0F;
	
	// '0' to '9'
	if (n >= 0 && n <= 9)
		return n + 0x30;
	// 'A' to 'F'
	else return n + 0x37;
}