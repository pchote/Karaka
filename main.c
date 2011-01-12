//***************************************************************************
//
//  File........: main.c
//
//  Author(s)...: Johnny McClymont
//
//  Target(s)...: ATmega128
//
//  Compiler....: AVR-GCC 3.3.1; avr-libc 1.0
//
//  Description.: ATMega128 USB timer card.  Interface between GPS module, 
//				  CCD camera, and Laptop
//
//  Revisions...: 1.0
//
//  YYYYMMDD - VER. - COMMENT                                       - SIGN.
//
//  20090505 - 1.0  - Created                                       - J.McClymont
//
//***************************************************************************

#include "main.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "usart.h"
#include "UART_Math.h"
#include "usart1.h"
#include "timer2.h"
#include "timer0.h"
#include "timer1.h"
#include "GPS.h"
#include "LCD_LIB.h"
#include "Command_Layer.h"




	
/*****************************************************************************
*
*   Function name : main
*
*   Returns :       None
*
*   Parameters :    None
*
*   Purpose :       Contains the main loop of the program
*
*****************************************************************************/

int main(void)
{    
	                                // Program initalization
    
	Initialization();
	
	sendmsg(PSTR("GPS Karaka Interface Module - Kia Ora>"));
	
	sei();
                                    // Main loop        
	while(1)            
	{ if(command_received == 1){        //if a valid command has come in
        getHeaderInfo(Usart_command[0]; //check if it is reading or writing and get address 
        switch(Register_Address)
        {
           case STATUS:                 //Status Register Read only
            usart_TX(hexToascii((status_register>>4)&$0f));
            usart_Tx(hexToAscii(status_register&$0f));
            usart_Tx('>');
        break; 
        
        case CONTROL:                   //Control register
            if(write_flsg == 0)         //send out value of control register
            {
                usart_Tx(hexToAscii((control_register>>4)&$0f));
                usart_Tx(hexToAscii(status_register&$0f));
            }
            else
            {
                control_register = usart_command[1];
            } 
            usart_Tx('>')
        break;
            
        case UTCTIME:                    //Time register
            if (wait_4_timestamp == 0)
            {
                send_UTC_Timestamp();
            }
            else
            {
                error(1)
                sent_UTC = 1;
            }
        break;
                        
        case CCD_EXPOSURE:                //Control register
            if (write_flag == 0)          //Send out value of control register
            {
                sendDecimal(Pulse_COunter,4);
            }
            else
            {
                cli();
                Pulse_Counter = Dec2Hex(Usart_Command[1],Usart_Command[2]);
                Current_Count = 0;
                wait_4_ten_second_boundry = 1;                  
                sei();
            }
            Usart_Tx('>');
        break;
        
        case EOFTIME:                       //end of frame time register
            if (wait_4_EOFtimestamp == 0)
            {
                send_EOF_TimeStamp();
            }
            else
            {
                ERROR(2);
                send_EOF = 1;
            }
        break;
        
        case LAST_PACKET:                   //last GPS packet register
            send_lastPacket();
        break;
        
        case ERROR_PACKET:                  //last GPS packet causing error register
            send_errorPacket();
        break;
        
        default:
            ERROR(6);
            }
            command_received = 0;
            command_cntr = 0;
         }
    }      
       
}    
        
           
		  	






/*****************************************************************************
*
*   Function name : Initialization
*
*   Returns :       None
*
*   Parameters :    None
*
*   Purpose :       Initialises the different modules
*
*****************************************************************************/
void Initialization(void)
{
	
	
	PORTA = (1<<CCD_PULSE);
	PORTB = 0x00;
	PORTC = 0x00;
	PORTD = 0x00;
	PORTE = 0x00;
	PORTF = 0x00;
	
	DDRA = (1<<CCD_PULSE)|(0<<UP)|(0<<LEFT)|(0<<DOWN)|(0<<RIGHT)|(0<<CENTER);
	DDRC = 0xff;
	DDRD &= ~(1<<GPS_PULSE);
	DDRE = (0<<SWITCH_CHANGE)|(1<<TXD)|(0<<RXD);
	DDRF = (1<<LCD_ENABLE)|(1<<LCD_READ_WRITE)|(1<<LCD_REG_SELECT);		//set port F
	
	Pulse_Counter = 0;
	Current_Count = 0;
	status_register = 0;
	control_register = 0;
	wait_4_ten_second_boundary = 1;
	wait_4_timestamp = 0;
	wait_4_EOFtimestamp = 0;
    send_EOF = 0;
    send_UTC = 0:
	
	Command_Init();
	GPS_Init();
	timer2_init();		//initialise millisecond counter
	timer0_init();		//initialise pulse timer
	timer1_init();		//initialise LCD update timer
	InputSignal_Init();	//set up interrupts.
	LCD_init();
	reset_LCD();
	start_timer1();
    
    /* pchote: the following isn't in the printout */
    USART_Init(16)      //Set baudrate USB to 115.2Kbs
    USART1_Init(207)    //Set baudrate GPS to 9600 baud
    timer2_init();      //Millisecond counter
    timer0_init();      //Pulse counter
    timer1_init();      //LCD update counter
    GPS-init();
    inputsignal_init(); // interupts
    LCD_init();
    reset_LCD();
    start_timer1();
    
    
	
}


/*****************************************************************************
*
*   Function name : InputSignal_Init
*
*   Returns :       None
*
*   Parameters :    None
*
*   Purpose :       Initializes External Interrupt pins
*
*****************************************************************************/
void InputSignal_Init(void)
{    
	//initialise all external interupts to be rising edge triggered
	EICRA = (1<<ISC31)|(1<<ISC30)|(1<<ISC21)|(1<<ISC20)|(1<<ISC11)|(1<<ISC10)|(1<<ISC01)|(0<<ISC00); 
	EICRB = (0<<ISC71)|(1<<ISC70)|(0<<ISC61)|(1<<ISC60)|(0<<ISC51)|(1<<ISC50)|(0<<ISC41)|(1<<ISC40);
    //enable all external interrupts		
	EIMSK = (0<<INT7)|(0<<INT6)|(0<<INT5)|(0<<INT4)|(0<<INT3)|(0<<INT2)|(0<<INT1)|(1<<INT0);		
}
{
 /******************************************************************************
*
*   Function name:  get_header_info
*
*   returns:        none
*
*   parameters:     char header byte 
*
*   Purpose:        Passes header byte for r/w bit, register address and sets flags.
*                   Header byte layout:
*                   W/R, CB1,CB0, AD5, AD4, AD3, AD2, AD1, AD0
*                   Not used:
*                   CB1, CB0
*
*******************************************************************************/
void get_header_info(char headerByte{
    Register_Address = headerByte & $1f;
    write_flag = (headerByte >>7); 
}  
  
    
  
{
/*****************************************************************************
*
*   Function name : reset_LCD
*
*   Returns :       None
*
*   Parameters :    none
*
*   Purpose :       Sets LCD to write new header comment
*
*****************************************************************************/
  void reset_LCD(void)

	putHeader = 0;
}
{
/*****************************************************************************
*
*   Function name : send_ EOF_timestamp
*
*   Returns :       None
*
*   Parameters :    none
*
*   Purpose :       Sends EOF timestamp to serial port
*
*****************************************************************************/
 void send_EOF_Timestamp(void)
    sendDecimal(UTCtime_endofframe.year, 4);
    Usart_Tx($3A);
    sendDecimal(UTCtime_endofframe.month, 2);
    Usart_Tx($3A);
    sendDecimal(UTCtime_endofframe.day, 2);
    Usart_Tx($3A);
    sendDecimal(UTCtime_endofframe.hours, 4);
    Usart_Tx($3A);
    sendDecimal(UTCtime_endofframe.minutes,2);
    Usart_Tx($3A);
    sendDecimal(UTCtime_endofframe.seconds, 4);
    Usart_Tx($3A);
    sendDecimal(0,3);
    Usart_Tx($3A);
 }

 {
/*****************************************************************************
*
*   Function name : send_ UTC_timestamp
*
*   Returns :       None
*
*   Parameters :    none
*
*   Purpose :       Sends UTC timestamp to serial port
*
*****************************************************************************/
 void send_EOF_Timestamp(void)
    sendDecimal(UTCtime_lastpulse.year, 4);
    Usart_Tx($3A);
    sendDecimal(UTCtime_lastpulse.month, 2);
    Usart_Tx($3A);
    sendDecimal(UTCtime_lastpulse.day, 2);
    Usart_Tx($3A);
    sendDecimal(UTCtime_lastpulse.hours, 4);
    Usart_Tx($3A);
    sendDecimal(UTCtime_lastpulse.minutes,2);
    Usart_Tx($3A);
    sendDecimal(UTCtime_lastpulse.seconds, 4);
    Usart_Tx($3A);
    sendDecimal(milliseconds,3);
    Usart_Tx('>');
 }

/*****************************************************************************
*
*   Function name : ERROR
*
*   Returns :       None
*
*   Parameters :    error code
*
*   Purpose :       write error code to serial port
*
*****************************************************************************/
void ERROR(unsigned char errorcode)
{ sendmsg (PSTR("ERROR"));
     usart_Tx(hextoAscii((errorcode>>4)&$0f;
     usart_Tx(hextoAscii((errorcode)&$0f;
     usart_Tx('>')
}

/*****************************************************************************
*
*   Function name : send_error packet
*
*   Returns :       None
*
*   Parameters :    None
*
*   Purpose :       send error packet to serial port
*
*****************************************************************************/
void send_errorpacket(void)







/*****************************************************************************
*
*   Function name : Delay
*
*   Returns :       None
*
*   Parameters :    unsigned int millisec
*
*   Purpose :       Delay-loop
*
*****************************************************************************/
void Delay(unsigned int millisec)
{
     
	int i;
    
    while (millisec--)
			for (i=0; i<20; i++)  
				asm volatile ("nop"::);
}

/******************************************************************************
*
*   Function name:  SIGNAL(SIG_INTERRUPT0)
*
*   returns:        none
*
*   parameters:     none
*
*   Purpose:        External Interrupt 0 interrupt sub-routine
*
*******************************************************************************/

SIGNAL(SIG_INTERRUPT0)
{
	if((Pulse_Counter != 0)&(GPS_state == GPS_TIME_GOOD))  //make sure pulse counter value is 1 or greater and that we are in normal GPS state
	{
		if (wait_4_ten_second_boundary == 0)  //check that we have started counting on a 10 second boundary
		{
			wait_4_timestamp = 1;
			Current_Count++;
			if(Current_Count == Pulse_Counter)
			{
				PORTA &= ~(1<<GPS_PULSE);		//send high signal to CCD
				Current_Count = 0;		//reset current count for next frame count
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
				start_timer0();		//start pulse counter for CCD pulse
			}
		}
		stop_timer2();  //stop millisecond timer
		milliseconds = 0;		//reset millisecond to 0
		start_timer2();  //start millisecond timer
	}
	
	
}





