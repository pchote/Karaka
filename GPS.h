//***************************************************************************
//
//  File........: GPS.c
//  Author(s)...: Johnny McClymont, Paul Chote
//  Description.: Trimble GPS listener
//
//***************************************************************************

#ifndef KARAKA_GPS_TRIMBLE_H
#define KARAKA_GPS_TRIMBLE_H

unsigned char GPS_Packet[100];
unsigned char Last_Packet[101];		//[0] holds packet size
unsigned char Error_Packet[102];	//[0] holds packet size, [1] holds error code resulting in packet storage

unsigned char endOfFrame_pulse;		//flag to indicate next time stamp is EOF timestamp
unsigned char packet_proccessed;	//flag to indicate if CCD pulse was set halfway through processing packet

void GPS_Init(void);
void GPS_processPacket(void);
void GPS_sendPacketMask(void);
void store_error_packet(unsigned char code);

#endif
