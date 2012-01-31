//***************************************************************************
//
//  File        : main.c
//  Copyright   : 2009-2012 Johnny McClymont, Paul Chote
//  Description : Main program logic
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <avr/interrupt.h>
#include "main.h"
#include "download.h"
#include "gps.h"
#include "display.h"
#include "command.h"
#include "monitor.h"

/* Hardware usage (ATmega1284p-AU):
 * PORTA:
 *    PA0: Display brightness input (ADC0)
 *    PA1-7: Unused
 * PORTB:
 *    PB0: Force-on (to GPS input buffer)
 *    PB1-4: Display select for writing data to display modules
 *    PB5: MOSI (data output to display modules)
 *    PB6: RS232 off (to GPS input buffer)
 *    PB7: SCK (SPI clock input)
 * PORTC:
 *    PC0-1: Unused
 *    PC2-5: TCK/TMS/TDO/TDI (to JTAG header)
 *    PC6-7: Unused
 * PORTD: 
 *    PD0: RXD0 (From PC)
 *    PD1: TXD0 (To PC)
 *    PD2: RXD1 (From GPS)
 *    PD3: TXD1 (To GPS)
 *    PD4: GPS PPS input
 *    PD5: INTG output
 *    PD6: Monitor input
 *    PD7: Spare output
 */

void reset_vars()
{
    start_countdown = stop_countdown = exposure_total = exposure_countdown = 0;
    countdown_mode = COUNTDOWN_DISABLED;
    monitor_mode = MONITOR_WAIT;
}

/*
 * Initialise the unit and wait for interrupts.
 */
int main(void)
{
    // Initialise global variables
    reset_vars();

    // Enable pin change interrupt for PPS input
    PCMSK3 |= _BV(PCINT28);
    PCICR |= _BV(PCIE3);

    // Set unused pins as inputs, enable pullup
    DDRA = 0x00;
    PORTA = 0xFF;

    // Set other init
    command_init();
    gps_init();
    download_init();
    monitor_init();
    display_init();

    // Enable interrupts
    sei();
    send_gps_config();
    unsigned char time_updated;
    unsigned int cycle = 0;
    // Main program loop
    for (;;)
    {
        monitor_tick();
        usart_process_buffer();
        time_updated = gps_process_buffer();

        // Force the display to refresh if the time hasn't updated in 65535 cycles
        if (time_updated || ++cycle == 0)
            update_display();
    }
}

/*
 * GPS time pulse interrupt handler
 * Fired on any level change from the PPS input (PD3)
 */
ISR(PCINT3_vect)
{
    // Ignore falling edge interrupt
    if (!bit_is_clear(PIND, PD4))
        return;

    // Don't count down unless we have a valid exposure time and the GPS is locked
    if (gps_state == GPS_ACTIVE) // Do we have a GPS lock?
    {
        if (countdown_mode == COUNTDOWN_ENABLED)
        {
            trigger_countdown();
            countdown_mode = COUNTDOWN_TRIGGERED;
        }
        else if (countdown_mode == COUNTDOWN_TRIGGERED)
            send_debug_string("Ignoring duplicate PPS pulse");
    }

    // Fixed time delay before starting an exposure sequence
    if (start_countdown > 0 && --start_countdown == 0)
        countdown_mode = COUNTDOWN_SYNCING;

    // Fixed time delay before stopping an exposure sequence
    if (stop_countdown > 0 && --stop_countdown == 0)
        send_stopexposure();
}

/*
 * Decrement the exposure counter and trigger a download if necessary
 */
void trigger_countdown()
{
    // End of exposure - send a syncpulse to the camera
    // and store a flag so the gps can save the synctime.
    if (--exposure_countdown == 0)
    {
        exposure_countdown = exposure_total;
        gps_record_synctime = TRUE;
        trigger_download();
    }
}
