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
#define GET_ERROR_PACKET		0x27	// Request for last GPS packet that caused an error

// Error codes
#define NO_ERROR					0x01
#define PACKETID_INVALID			0x02
#define UTC_ACCESS_ON_UPDATE		0x04
#define EOF_ACCESS_ON_UPDATE		0x08
#define PACKET_8FAB_LENGTH_WRONG	0x10
#define GPS_TIME_NOT_LOCKED			0x20
#define GPS_SERIAL_LOST				0x40

unsigned char command_packet[15];
unsigned char command_reply_packet[100];
unsigned char command_reply_cntr;
unsigned char command_stored_error_state;
unsigned char command_size;
unsigned char command_have_startbit;
unsigned char command_checking_DLE_stuffing; //flag to check if packet has DLE stuffed

void command_init(void);

#endif
