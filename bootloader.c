/*
 * A self-contained bootloader implementing the AVR109 protocol.
 * Based on "AppNote AVR109 - Self-programming" code by Atmel
 */

#include <stdint.h>
#include <stdbool.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/boot.h>

// Bootloader bipass EEPROM parameters - must match definitions in main.c
#define BOOTLOADER_EEPROM_OFFSET (uint8_t *)(0x00)
#define BIPASS_DISABLED 0xFF
#define BIPASS_ENABLED 0x42

// Disable watchdog timer early in boot
uint8_t boot_mcusr __attribute__ ((section(".noinit")));
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));
void wdt_init(void)
{
    boot_mcusr = MCUSR;
    MCUSR = 0;
    wdt_disable();
}

void initbootuart()
{
    // Set 9600 baud rate
#define BAUD 9600
#include <util/setbaud.h>
    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;
#if USE_2X
    UCSR0A = _BV(U2X0);
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
    uint32_t address = 0;
    uint16_t temp_int = 0;
    uint8_t val = 0;

    // Skip bootloader if reset by watchdog
	bool skip_bootloader = !(boot_mcusr & _BV(EXTRF));

	// Allow the main application to request a one-time bipass of the bootloader
	// for enabling relay mode (the GPS programs toggle DTR when opening the port)
	if (eeprom_read_byte(BOOTLOADER_EEPROM_OFFSET) == BIPASS_ENABLED)
	{
		eeprom_update_byte(BOOTLOADER_EEPROM_OFFSET, BIPASS_DISABLED);
		skip_bootloader = true;
	}

	if (skip_bootloader)
    {
        boot_spm_busy_wait();
        boot_rww_enable();
        asm("jmp 0000");
    }

    initbootuart();

    // Exit bootloader if no data is recieved after ~1 second
    wdt_enable(WDTO_500MS);
    for (;;)
    {
        uint8_t b = recchar();
        wdt_disable();

        // Wait for command character
        switch (b)
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
                sendchar('\r');
                wdt_enable(WDTO_15MS);
                for(;;);
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
                // Ignore command and its parameter.
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