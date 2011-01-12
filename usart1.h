//***************************************************************************
//
//  File........: usart.h
//
//  Author(s)...: Johnny McClymont
//
//  Target(s)...: ATmega128
//
//  Compiler....: AVR-GCC 3.3.1; avr-libc 1.0
//
//  Description.: ATMega128 USART routines
//
//  Revisions...: 1.0
//
//  YYYYMMDD - VER. - COMMENT                                       - SIGN.
//
//  20070824 - 1.0  - Created                                       - J.McClymont
//
//***************************************************************************

void USART1_Init(unsigned int baudrate);
void Usart1_Tx(char);
char Usart1_Rx(void);

char incomingbyte;		//buffer for incoming byte
unsigned char checking_DLE_stuffing;		//flag to check if packet has DLE stuffed
unsigned char GPS_startBit_Rcvd;		//flag to indicate we a recording a GPS packet
unsigned char packet_cntr;
unsigned char gps_usart_state;		//state used to determine if we are synced to GPS packets
unsigned char sync_state;			//state to check usart data string to try and sync with GPS packet

#define	SYNCING_PACKETS	0
#define	CAPTURE_PACKETS	1


#define LOOK_FOR_DLE		0
#define	LOOK_FOR_ETX		1
#define LOOK_FOR_2ND_DLE	2
#define LOOK_FOR_NO_DLE		3


