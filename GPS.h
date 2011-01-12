//***************************************************************************
//
//  File........: GPS.h
//
//  Author(s)...: Johnny McClymont
//
//  Target(s)...: ATmega128
//
//  Compiler....: AVR-GCC 3.3.1; avr-libc 1.0
//
//  Description.: GPS routines
//
//  Revisions...: 1.0
//
//  YYYYMMDD - VER. - COMMENT                                       - SIGN.
//
//  20090606 - 1.0  - Created                                       - J.McClymont
//
//***************************************************************************

void GPS_Init(void);
void GPS_processPacket(void);
void GPS_sendPacketMask(void);
void store_error_packet(unsigned char code);

unsigned char GPS_Packet[100];
unsigned char Last_Packet[101];		//[0] holds packet size
unsigned char Error_Packet[102];	//[0] holds packet size, [1] holds error code resulting in packet storage


unsigned char endOfFrame_pulse;		//flag to indicate next time stamp is EOF timestamp
unsigned char packet_proccessed;	//flag to indicate if CCD pulse was set halfway through processing packet

	


