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
#include <avr/pgmspace.h>

#include "main.h"
#include "download.h"
#include "gps.h"
#include "display.h"
#include "command.h"
#include "monitor.h"
#include "fakecamera.h"

char msg_ignored_duplicate_pulse[] PROGMEM = "Ignoring duplicate PPS pulse";

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

/*
 * Reset internal state to startup values
 */
void set_initial_state()
{
    cli();
    exposure_total = exposure_countdown = 0;
    countdown_mode = COUNTDOWN_DISABLED;
    monitor_init_state();
    fake_camera_init_state();
    sei();

    // Send config to attached GPS
    // Requires interrupts to be enabled
    gps_send_config();
}

/*
 * Initialize the unit and wait for interrupts.
 */
int main(void)
{
    // Enable pin change interrupt for PPS input
    PCMSK3 |= _BV(PCINT28);
    PCICR |= _BV(PCIE3);

    // Enable pullup resistor on unused pins
    PORTA = 0xFF;

    // Set other init
    command_init_hardware();
    gps_init_hardware();
    download_init_hardware();
    monitor_init_hardware();
    display_init_hardware();
    fake_camera_init_hardware();

    // Enable interrupts
    sei();

    // Initialize global variables
    set_initial_state();

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
            send_debug_string_P(msg_ignored_duplicate_pulse);
    }
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
