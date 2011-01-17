//***************************************************************************
//
//  File........: gps.h
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Parses the serial stream from a Trimble GPS unit
//                and extracts timestamp information
//
//***************************************************************************

#ifndef KARAKA_GPS_H
#define KARAKA_GPS_H

#define	SYNCING_PACKETS	0
#define	CAPTURE_PACKETS	1

#define LOOK_FOR_DLE		0
#define	LOOK_FOR_ETX		1
#define LOOK_FOR_2ND_DLE	2
#define LOOK_FOR_NO_DLE		3

unsigned char gps_trimble_packet[100];
unsigned char gps_last_trimble_packet[101];		//[0] holds packet size
unsigned char gps_trimble_error_packet[102];	//[0] holds packet size, [1] holds error code resulting in packet storage

unsigned char gps_record_synctime;		//flag to indicate next time stamp is EOF timestamp
unsigned char gps_packet_proccessed;	//flag to indicate if CCD pulse was set halfway through processing packet

unsigned char gps_checking_DLE_stuffing;		//flag to check if packet has DLE stuffed
unsigned char gps_received_startbit;		//flag to indicate we a recording a GPS packet
unsigned char gps_packet_cntr;
unsigned char gps_usart_state;		//state used to determine if we are synced to GPS packets
unsigned char gps_sync_state;			//state to check usart data string to try and sync with GPS packet


void gps_init(void);
void gps_transmit_byte(unsigned char);
unsigned char gps_receive_byte(void);
void store_error_packet(unsigned char code);
void gps_send_trimble_init(void);
void gps_process_trimble_packet(void);

#endif
