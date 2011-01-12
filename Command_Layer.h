//***************************************************************************
//
//  File........: Command_Layer.h
//
//  Author(s)...: Johnny McClymont
//
//  Target(s)...: ATmega128
//
//  Compiler....: AVR-GCC 3.3.1; avr-libc 1.0
//
//  Description.: Module to process commands from the User Interface software 
//
//  Revisions...: 1.0
//
//  YYYYMMDD - VER. - COMMENT                                       - SIGN.
//
//  20090619 - 1.0  - Created                                       - J.McClymont
//
//***************************************************************************

void Command_Init(void);
void Command_processPacket(void);
void Command_sendPacket(void);

unsigned char command_Packet[15];
unsigned char reply_Packet[100];
unsigned char reply_cntr;
unsigned char stored_error_state;
unsigned char error_state;
unsigned char size;

//Packet IDs
#define DUMMY	    			0x01   	// Packet ID for dummy packet
#define GET_STATUS	    		0x21   	// Packet ID for request Status
#define GET_CONTROL				0x22	// Packet ID for request Control state
#define SET_CONTROL				0x42	// Packet ID for writing control state
#define GET_UTCTIME				0x23	// Packet ID for request UTC time at last GPS pulse
#define SET_CCD_EXPOSURE		0x44	// Packet ID for writing CCD exposure time
#define GET_CCD_EXPOSURE		0x24	// Packet ID for request CCD exposure time
#define	GET_EOFTIME				0x25	// Packet ID for request End of frame time at last Sync pulse
#define GET_LAST_PACKET			0x26	// Packet ID for request last GPS packet
#define GET_ERROR_PACKET		0x27	// Packet ID for request last GPS packet that caused an error


//Error codes
#define NO_ERROR					0x01
#define PACKETID_INVALID			0x02
#define UTC_ACCESS_ON_UPDATE		0x04
#define EOF_ACCESS_ON_UPDATE		0x08
#define PACKET_8FAB_LENGTH_WRONG	0x10
#define GPS_TIME_NOT_LOCKED			0x20
#define GPS_SERIAL_LOST				0x40


