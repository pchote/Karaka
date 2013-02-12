//***************************************************************************
//
//  File        : usb.h
//  Copyright   : 2013 Paul Chote
//  Description : Handles communication with the Acquisition PC via serial
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <stdarg.h>
#include <stdint.h>

#ifndef KARAKA_USB_H
#define KARAKA_USB_H

typedef enum
{
    TIMER_IDLE,
    TIMER_WAITING,
    TIMER_ALIGN,
    TIMER_EXPOSING,
    TIMER_READOUT
} TimerMode;

void usb_initialize();
void usb_tick();

void usb_send_message_P(const char *string);
void usb_send_message_fmt_P(const char *fmt, ...);
void usb_send_raw(uint8_t *data, uint8_t length);
void usb_send_timestamp();
void usb_send_trigger();
void usb_send_status(TimerMode mode);
void usb_stop_exposure();

void usb_send_byte(uint8_t b);

#endif
