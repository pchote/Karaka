//***************************************************************************
//
//  File........: command.h
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Responds to user commands over usb
//
//***************************************************************************

#ifndef KARAKA_COMMAND_H
#define KARAKA_COMMAND_H

// Packet IDs
#define ECHO	    			0x01   	// Echo packet
#define GET_STATUS	    		0x21   	// Request Status
#define GET_CONTROL				0x22	// Request Control state
#define SET_CONTROL				0x42	// Set control state
#define GET_UTCTIME				0x23	// Request UTC time at last GPS pulse
#define SET_CCD_EXPOSURE		0x44	// Set new CCD exposure time
#define GET_CCD_EXPOSURE		0x24	// Request for CCD exposure time
#define	GET_EOFTIME				0x25	// Request for End of frame time at last Sync pulse
#define GET_LAST_PACKET			0x26	// Request for last GPS packet
#define GET_ERROR_PACKET		0x27	// Request for last GPS packet that caused an error

// Error codes
#define NO_ERROR					0x01
#define PACKETID_INVALID			0x02
#define UTC_ACCESS_ON_UPDATE		0x04
#define EOF_ACCESS_ON_UPDATE		0x08
#define PACKET_8FAB_LENGTH_WRONG	0x10
#define GPS_TIME_NOT_LOCKED			0x20
#define GPS_SERIAL_LOST				0x40

unsigned char command_Packet[15];
unsigned char reply_Packet[100];
unsigned char reply_cntr;
unsigned char stored_error_state;
unsigned char error_state;
unsigned char size;

void command_init(void);
void command_process_packet(void);
void command_send_packet(void);


void USART_Init(unsigned int baudrate);
void Usart_Tx(char);
unsigned char Usart_Rx(void);
void sendmsg (const char *s);
char startBit_Rcvd;
unsigned char userCommand;;		//buffer for incoming byte
unsigned char checking_DLE_stuffing_flag;		//flag to check if packet has DLE stuffed

#endif
