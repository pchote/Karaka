/*
 * A self-contained bootloader implementing the AVR109 protocol.
 * Based on "AppNote AVR109 - Self-programming" code by Atmel
 */

#include <stdint.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/eeprom.h>

/*
 * avr-libc 1.8.0 throws a poisoned identifier error
 * as soon as we include boot.h for the atmega128
 * To fix, modify boot.h:110-116 to the following:
 *
 *  #if defined(SPMCSR)
 *  #  define __SPM_REG     SPMCSR
 *  #elif !defined (__AVR_ATmega128__)
 *  #  if defined(SPMCR)
 *  #    define __SPM_REG   SPMCR
 *  #  endif
 *  #endif
*/
#include <avr/boot.h>

// For boot flag definitions
#include "main.h"

// Definitions for SPM control
// 128 words = 256 bytes
#define PAGESIZE  256

#if CPU_TYPE == CPU_ATMEGA1284p
#   define PARTCODE 0x43
#elif CPU_TYPE == CPU_ATMEGA128
#   define PARTCODE 0x74
#else
#   error Unknown CPU type
#endif

// Disable watchdog timer early in boot
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));
void wdt_init(void)
{
#if CPU_TYPE == CPU_ATMEGA128
    MCUCSR = 0;
#elif CPU_TYPE == CPU_ATMEGA1284p
    MCUSR = 0;
#else
#   error Unknown CPU type
#endif
    wdt_disable();
}

void initbootuart()
{
    // Set 9600 baud rate
    UBRR0H = 0;
#if CPU_MHZ == 16
    UBRR0L = 0x67;
#elif CPU_MHZ == 10
    UCSR0A = _BV(U2X0);
    UBRR0L = 0x81;
#else
    #error Unknown CPU Frequency
#endif

    // Enable receive, transmit
    UCSR0B = _BV(RXEN0)|_BV(TXEN0);

    // Set 8-bit data frame
    UCSR0C = _BV(UCSZ01)|_BV(UCSZ00);
}

void sendchar(uint8_t c)
{
    UDR0 = c;
    loop_until_bit_is_set(UCSR0A, TXC0);
    UCSR0A |= _BV(TXC0);
}

uint8_t recchar()
{
    loop_until_bit_is_set(UCSR0A, RXC0);
    return UDR0;
}

// Write a block of data to EEPROM or Flash
uint8_t block_write(uint16_t size, uint8_t mem, uint32_t *address)
{
    switch (mem)
    {
        // EEPROM
        case 'E':
        {
            uint8_t buffer[PAGESIZE];

            // Fill buffer first, as EEPROM is too slow to copy with UART speed
            for(uint32_t i = 0; i < size; i++)
                buffer[i] = recchar();

            boot_spm_busy_wait();
            eeprom_update_block(buffer, (uint8_t *)((uint16_t)*address), size);
        }
        return '\r';

        // Flash
        case 'F':
            // NOTE: For flash programming, 'address' is given in words
            // Convert address to bytes temporarily
            (*address) <<= 1;
            uint32_t tempaddress = (*address);

            do
            {
                uint16_t data = recchar();
                data |= (recchar() << 8);
                boot_page_fill(*address, data);

                // Select next word
                (*address) += 2;
                size -= 2;
            } while (size);

            boot_page_write(tempaddress);
            boot_spm_busy_wait();
            boot_rww_enable();

            // Convert address back to Flash words again.
            (*address) >>= 1;
        return '\r';

        // Unknown
        default:
            return '?';
    }
}

// Read a block of data from EEPROM or Flash
void block_read(uint16_t size, uint8_t mem, uint32_t *address)
{
    switch (mem)
    {
        // EEPROM
        case 'E':
            do
            {
                sendchar(eeprom_read_byte((uint8_t *)((uint16_t)(*address))));
                size--;
            } while (size);
        return;

        // Flash
        case 'F':
            // Convert address to bytes temporarily
            (*address) <<= 1;
            do
            {
                sendchar(pgm_read_byte_far(*address));
                sendchar(pgm_read_byte_far((*address) + 1));

                 // Select next word
                (*address) += 2;
                size -= 2;
            } while (size);

            // Convert address back to Flash words again
            (*address) >>= 1;
        return;
    }
}

int main(void)
{
    // Set up function pointer to RESET vector.
    uint32_t address = 0;
    uint16_t temp_int = 0;
    uint8_t val = 0;

    initbootuart();

    // Boot application unless PC7 is pulled high
    PORTC |= _BV(PC7);

    if (bit_is_set(PINC, PC7) &&
        eeprom_read_byte(BOOTFLAG_EEPROM_OFFSET) == BOOTFLAG_BOOT)
    {
        boot_spm_busy_wait();        
        boot_rww_enable();
        asm("jmp 0000");
    }

    for (;;)
    {
        // Wait for command character
        switch (recchar())
        {
            // Check autoincrement status
            case 'a': 
                sendchar('Y');
            break;

            // Set address
            case 'A':
                    address = (recchar() << 8) | recchar();
                sendchar('\r');
            break;

            // Chip erase
            case 'e':
                for (uint32_t i = 0; i < BOOTSTART-1; i += PAGESIZE)
                {
                    // NOTE: Here we use address as a byte-address,
                    // not word-address, for convenience
                    boot_spm_busy_wait();    
                    boot_page_erase(i);
                }
                sendchar('\r');
            break;

            // Check block load support
            case 'b':
                sendchar('Y');

                // MSB first
                sendchar((PAGESIZE >> 8) & 0xFF);

                // Report PAGESIZE (bytes)
                sendchar(PAGESIZE & 0xFF);
            break;

            // Start block load
            case 'B':
                // Block size
                temp_int = (recchar() << 8) | recchar();

                // Mem type
                val = recchar();
                sendchar(block_write(temp_int, val, &address));
            break;

            // Start block read
            case 'g':
                // Block Size
                temp_int = (recchar() << 8) | recchar();
                
                // Mem type
                val = recchar();
                block_read(temp_int, val, &address);
            break;

            // Read program memory
            case 'R':        
                boot_spm_busy_wait();        
                boot_rww_enable();

                // Send high byte, then low byte of flash word
                sendchar(pgm_read_byte_far((address << 1) + 1));
                sendchar(pgm_read_byte_far(address << 1));

                // Advance to next Flash word
                address++;
            break;

            // Write program memory, low byte       
            case 'c':
                // Get low byte for later boot_page_fill
                // NOTE: Always use this command before sending high byte
                temp_int = recchar(); 
                sendchar('\r');
            break;

            // Write program memory, high byte
            case 'C':
                // Get and insert high byte
                temp_int |= recchar() << 8; 
                boot_spm_busy_wait();
                boot_page_fill(address << 1, temp_int);
                address++;
                sendchar('\r');
            break;

            // Write page
            case 'm':
                // Protect bootloader area
                if (address >= ((BOOTSTART-1) >> 1))
                    sendchar('?');
                else
                {
                    boot_spm_busy_wait();        
                    boot_page_write(address << 1);
                }

                sendchar('\r');
            break;

            // Write EEPROM memory
            case 'D':
                boot_spm_busy_wait();
                eeprom_update_byte((uint8_t *)((uint16_t)address), recchar());
                address++;
                sendchar('\r');
            break;

            // Read EEPROM memory
            case 'd':
                sendchar(eeprom_read_byte((uint8_t *)((uint16_t)address)));
                address++;
            break;

            // Write lock bits
            case 'l':
                boot_spm_busy_wait();        
                boot_lock_bits_set(~recchar());
                sendchar('\r');
            break;

            // Read lock bits
            case 'r':
                boot_spm_busy_wait();        
                sendchar(boot_lock_fuse_bits_get(GET_LOCK_BITS));
            break;

            // Read fuse bits
            case 'F':
                boot_spm_busy_wait();        
                sendchar(boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS));
            break;

            // Read high fuse bits
            case 'N':
                boot_spm_busy_wait();        
                sendchar(boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS));
            break;

            // Read extended fuse bits
            case 'Q':
                boot_spm_busy_wait();        
                sendchar(boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS));
            break;

            // Enter and leave programming mode
            case 'P':
            case 'L':
                sendchar('\r');
            break;

            // Exit bootloader
            case 'E':
                boot_spm_busy_wait();        
                boot_rww_enable();
                sendchar('\r');
                asm("jmp 0000");
            break;

            // Get programmer type
            case 'p':
                sendchar('S');
            break;

            // Return supported device codes
            case 't':
                sendchar(PARTCODE);
                sendchar(0);
            break;

            // Set LED, clear LED and set device type
            case 'x':
            case 'y':
            case 'T':
                // Ignore command and it's parameter.
                recchar();
                sendchar('\r');
            break;

            // Return programmer identifier
            case 'S':
                sendchar('A');
                sendchar('V');
                sendchar('R');
                sendchar('B');
                sendchar('O');
                sendchar('O');
                sendchar('T');
            break;

            // Return software version
            case 'V':
                sendchar('1');
                sendchar('5');
            break;

            // Return signature bytes
            case 's':
                sendchar(SIGNATURE_2);
                sendchar(SIGNATURE_1);
                sendchar(SIGNATURE_0);
            break;

            // Synchronization
            case 0x1b:
            break;

            // Unrecognised command
            default:
                sendchar('?');
            break;
        }
    }
}