//***************************************************************************
//
//  File........: gps.h
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Parses the serial stream from a Trimble GPS unit
//                and extracts timestamp information
//
//***************************************************************************

#include "main.h"

#ifndef KARAKA_GPS_H
#define KARAKA_GPS_H

// gps_usart_state values
#define	SYNCING_PACKETS	0
#define	CAPTURE_PACKETS	1

// gps_sync_state values
#define LOOK_FOR_DLE		0
#define LOOK_FOR_ETX		1
#define LOOK_FOR_2ND_DLE	2
#define LOOK_FOR_NO_DLE		3

// gps_state values
#define SYNCING					0	// Trying to sync packets of GPS unit
#define SETUP_GPS				1	// Send packets to GPS to set up auto broadcast.
#define CHECK_GPS_TIME_VALID	2	// Check to see timing packets from GPS are from GPS clock
#define GPS_TIME_GOOD			3	// Gps is locked and working.
#define INVALID                 4   // Invalid state


unsigned char gps_timeout_count; // Number of counts since the last gps packet was recieved. Incremented by display.c
unsigned char gps_state;         // 
unsigned char gps_usart_state;   // State used to determine if we are synced to GPS packets
unsigned char gps_sync_state;    // State to check usart data string to try and sync with GPS packet
unsigned char nextPacketisEOF;		//flag to inform GPS module to record next packet as EOF time


timestamp gps_last_timestamp;
timestamp gps_last_synctime;

unsigned char gps_trimble_packet[100];
unsigned char gps_last_trimble_packet[101];   // [0] holds packet size
unsigned char gps_trimble_error_packet[102];  // [0] holds packet size, [1] holds error code resulting in packet storage

unsigned char gps_record_synctime;            // Flag to indicate next time stamp is EOF timestamp
unsigned char gps_processing_packet;          // Flag to indicate when the gps is currently processing a packet

unsigned char gps_checking_DLE_stuffing;      // Flag to check if packet has DLE stuffed
unsigned char gps_received_startbit;          // Flag to indicate we a recording a GPS packet
unsigned char gps_packet_cntr;


void gps_init(void);
void gps_timeout(void);
void gps_transmit_byte(unsigned char);
unsigned char gps_receive_byte(void);
void store_error_packet(unsigned char code);
void gps_send_trimble_init(void);
void gps_process_trimble_packet(void);

#endif
