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
#include <avr/eeprom.h>
#include <util/atomic.h>

#include "main.h"
#include "download.h"
#include "gps.h"
#include "display.h"
#include "command.h"
#include "monitor.h"

const char msg_duplicate_pulse[] PROGMEM = "Duplicate PPS pulse detected";
const char msg_no_serial[]       PROGMEM = "GPS serial connection lost";

uint8_t exposure_total = 0;
uint8_t align_boundary = 0;
volatile uint8_t exposure_countdown = 0;
volatile countdownstate countdown_mode = COUNTDOWN_DISABLED;
volatile interruptflags interrupt_flags = 0;

/* Hardware usage (ATmega12-15AI) - Hardware versions 1-2:
 * PORTA:
 *    PA0: INTG output
 *    PA1-7: Unused
 * PORTB:
 *    PB0: Force-on (to GPS input buffer)
 *    PB1: SCK (to FT232 chip)
 *    PB2-7: Unused
 * PORTC:
 *    PC0-7: 8-bit data to display
 * PORTD:
 *    PD0: GPS PPS input
 *    PD1: Force-off (to RS232 chip)
 *    PD2: RXD1 (From GPS)
 *    PD3: TXD1 (To GPS)
 *    PD4-7: Unused
 * PORTE:
 *    PE0: RXD0 (From PC)
 *    PE1: TXD0 (To PC)
 *    PE2-3: Unused
 *    PE4: Monitor input (Version 2 only)
 *    PE5-7: Unused
 * PORTF:
 *    PF0: Display register select
 *    PF1: Display read/write select
 *    PF2: Display read/write start
 *    PF3: Unused
 *    PF4-7: TCK/TMS/TDO/TDI (to JTAG header)
 * PORTG:
 *    PG0-2: Unused
 *
 * Timer0: Download pulse length
 * Timer1: Unused
 * Timer2: GPS serial timeout
 * Timer3: Monitor debounce and simulated level delays
 *
 * Usart0: USB <-> Acquisition PC
 * Usart1: RS232 <-> GPS
 */

/* Hardware usage (ATmega1284p-AU) - Hardware version 3:
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
 *
 * Timer0: Download pulse length
 * Timer1: Unused
 * Timer2: GPS serial timeout
 * Timer3: Monitor debounce and simulated level delays
 *
 * Usart0: USB <-> Acquisition PC
 * Usart1: RS232 <-> GPS
 */

/*
 * Reset internal state to startup values
 */
void set_initial_state()
{
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
        exposure_total = exposure_countdown = 0;
        countdown_mode = COUNTDOWN_DISABLED;
        interrupt_flags = 0;
        command_init_state();
        monitor_init_state();
    }

    // Send config to attached GPS
    // Requires interrupts to be enabled
    gps_init_state();
}

/*
 * Initialize the unit and wait for interrupts.
 */
int main(void)
{
#if CPU_TYPE == CPU_ATMEGA128
    // Set INT0 to be rising edge triggered
    EICRA = _BV(ISC01);
    EIMSK |= _BV(INT0);
#elif CPU_TYPE == CPU_ATMEGA1284p
    // Enable pin change interrupt for PPS input
    PCMSK3 |= _BV(PCINT28);
    PCICR |= _BV(PCIE3);

    // Enable pullup resistor on unused pins
    PORTA = 0xFF;
#else
#   error Unknown CPU type
#endif

    // Set other init
    command_init_hardware();
    gps_init_hardware();
    download_init_hardware();
    monitor_init_hardware();
    display_init_hardware();

    // Enable interrupts
    sei();

    // Initialize global variables
    set_initial_state();

    // Enable regular boot if we just updated via the bootloader
    eeprom_update_byte(BOOTFLAG_EEPROM_OFFSET, BOOTFLAG_BOOT);

    // Main program loop
    for (;;)
    {
        // Handle message flags set via interrupt
        if (interrupt_flags)
        {
            uint8_t temp_int_flags = 0;
            ATOMIC_BLOCK(ATOMIC_FORCEON)
            {
                temp_int_flags = interrupt_flags;
                interrupt_flags = 0;
            }

            if (temp_int_flags & FLAG_DOWNLOAD_COMPLETE)
                send_status(TIMER_EXPOSING);

            if (temp_int_flags & FLAG_BEGIN_ALIGN)
                send_status(TIMER_ALIGN);

            if (temp_int_flags & FLAG_STOP_EXPOSURE)
            {
                send_status(TIMER_IDLE);
                send_stopexposure();
            }

            if (temp_int_flags & FLAG_NO_SERIAL)
                send_debug_string_P(msg_no_serial);

            if (temp_int_flags & FLAG_DUPLICATE_PULSE)
                send_debug_string_P(msg_duplicate_pulse);
        }

        monitor_tick();
        usart_process_buffer();
        gps_process_buffer();
        update_display();
    }
}

/*
 * GPS time pulse interrupt handler
 * Fired on any level change from the PPS input (PD3)
 */

#if CPU_TYPE == CPU_ATMEGA128
ISR(INT0_vect)
{
#elif CPU_TYPE == CPU_ATMEGA1284p
ISR(PCINT3_vect)
{
    // Ignore falling edge interrupt
    if (!bit_is_clear(PIND, PD4))
        return;
#else
#   error Unknown CPU type
#endif

    // Don't count down unless we have a valid exposure time and the GPS is locked
    if (gps_state == GPS_ACTIVE && countdown_mode != COUNTDOWN_RELAY)
    {
        // Send a warning about the duplicate pulse
        if (countdown_mode == COUNTDOWN_TRIGGERED)
            interrupt_flags |= FLAG_DUPLICATE_PULSE;

        if (countdown_mode == COUNTDOWN_ENABLED || countdown_mode == COUNTDOWN_TRIGGERED)
        {
            // End of exposure - send a syncpulse to the camera
            // and store a flag so the gps can save the synctime.
            if (--exposure_countdown == 0)
            {
                exposure_countdown = exposure_total;
                gps_record_synctime = true;
                trigger_download();
            }

            countdown_mode = COUNTDOWN_TRIGGERED;
        }
        else if (countdown_mode == COUNTDOWN_ALIGNED)
        {
            exposure_countdown = exposure_total;
            gps_record_synctime = true;
            trigger_download();

            countdown_mode = COUNTDOWN_TRIGGERED;
        }
    }
    else if (countdown_mode == COUNTDOWN_RELAY)
        trigger_download();
}
