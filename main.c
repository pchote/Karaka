//***************************************************************************
//
//  File........: main.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: ATMega128 USB timer card.  Interface between GPS module, 
//				  CCD camera, and Laptop
//
//***************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "main.h"
#include "usart.h"
#include "UART_Math.h"
#include "usart1.h"
#include "msec_timer.h"
#include "sync_pulse.h"
#include "GPS.h"
#include "display.h"
#include "Command_Layer.h"
	
/*
 * Initialise the unit and wait for interrupts.
 */
int main(void)
{
	// Configure port A.
	// Pin 0 is used to output the CCD sync pulse.
	// Pins 1-5 used to input the (unimplemented) button controls
	DDRA = (1<<CCD_PULSE)|(0<<UP)|(0<<LEFT)|(0<<DOWN)|(0<<RIGHT)|(0<<CENTER);
	
	// Pin 0 is initially set HIGH
	// TODO: Why? shouldn't this be LOW? or is it inverted by later circuits?
	PORTA = (1<<CCD_PULSE);	
	
	// Configure port B.
	// Appears to be unused by code
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
	Pulse_Counter = 0;
	Current_Count = 0;
	status_register = 0;
	control_register = 0;
	wait_4_ten_second_boundary = 1;
	wait_4_timestamp = 0;
	wait_4_EOFtimestamp = 0;

	// Initialise the hardware units
	Command_Init();
	GPS_Init();
	msec_timer_init();		// Millisecond counter
	sync_pulse_init();		// Pulse timer
	display_init();
	InputSignal_Init();	//set up interrupts.

	// Enable interrupts (TODO: Again?)
	sei();

	// Wait. Main program logic is handled in interrupts.
	while(1){}
}

/*
 * Initialise external interrupt pins
 * TODO: Describe what each interrupt is triggered by
 */
void InputSignal_Init(void)
{
	// initialise all external interupts to be rising edge triggered
	EICRA = (1<<ISC31)|(1<<ISC30)|(1<<ISC21)|(1<<ISC20)|(1<<ISC11)|(1<<ISC10)|(1<<ISC01)|(0<<ISC00); 
	EICRB = (0<<ISC71)|(1<<ISC70)|(0<<ISC61)|(1<<ISC60)|(0<<ISC51)|(1<<ISC50)|(0<<ISC41)|(1<<ISC40);
	
	// enable all external interrupts		
	EIMSK = (0<<INT7)|(0<<INT6)|(0<<INT5)|(0<<INT4)|(0<<INT3)|(0<<INT2)|(0<<INT1)|(1<<INT0);		
}

/*
 * Wait for a specified number of milliseconds
 * TODO: Isn't this usec?
 */
void Delay(unsigned int millisec)
{
	int i;
	while (millisec--)
		for (i=0; i<20; i++)  
			asm volatile ("nop"::);
}

/*
 * GPS time pulse interrupt handler
 * Fired when a `second boundary' time pulse is recieved via the attached BNC cable
 */
SIGNAL(SIG_INTERRUPT0)
{
	// TODO: The single `&' looks like a bug
	if((Pulse_Counter != 0) & (GPS_state == GPS_TIME_GOOD))  //make sure pulse counter value is 1 or greater and that we are in normal GPS state
	{
		if (wait_4_ten_second_boundary == 0)  //check that we have started counting on a 10 second boundary
		{
			wait_4_timestamp = 1;
			Current_Count++;
			if (Current_Count == Pulse_Counter)
			{
				Current_Count = 0;	      //reset current count for next frame count
				wait_4_EOFtimestamp = 1;
				pulse_timer = 1;	// set ccd intergrate pulse to 512uS long
				if (packet_proccessed == 1)
				{
					endOfFrame_pulse = 1;	//set end of frame flag so we know to record next time stamp as end of frame time
					nextPacketisEOF = 0;
				}
				else
				{
					nextPacketisEOF = 1;
				}
				sync_pulse_trigger();
			}
		}
		msec_timer_stop();
		milliseconds = 0;
		msec_timer_start();
	}
}
