##***************************************************************************
##
##  File        : Makefile
##  Author      : Paul Chote
##  Description : Makefile to build the project using avr-gcc and avrdude
##
##***************************************************************************

# System port to find the timer on for upgrading via the bootloader
PORT      := /dev/tty.usbserial-00001004

# CPU Frequency; 16 or 10
CPU_MHZ   := 16

# CPU Clock type: 0 for internal oscillator, 1 for external GPS signal
CPU_CLOCK := 0

# Processor type: 0 for original atmega128 board, 1 for newer atmega1284p boards
CPU_TYPE  := 1

##***************************************************************************

AVRDUDE = avrdude -c dragon_jtag -P usb -p $(DEVICE)
OBJECTS = command.o gps.o download.o monitor.o main.o

BOOTLOADER   = avrdude -c avr109 -p $(DEVICE) -b 9600 -P $(PORT)
BOOT_OBJECTS = bootloader.o
BOOTSTART    = 0x1E000

ifeq (${CPU_CLOCK}, 0)
    LFUSE = 0xFF
else
    ifeq (${CPU_CLOCK}, 1)
        LFUSE = 0xF0
    else
        $(error Unknown clock type)
    endif
endif

ifeq (${CPU_TYPE}, 0)
    DEVICE = atmega128
    HFUSE = 0x09
    OBJECTS += display_lcd.o
else
    ifeq (${CPU_TYPE}, 1)
        DEVICE = atmega1284p
        HFUSE = 0x18
        OBJECTS += display_led.o
    else
        $(error Unknown CPU type)
    endif
endif

COMPILE = avr-gcc -g -mmcu=$(DEVICE) -Wall -Wextra -Werror -Os -std=gnu99 -funsigned-bitfields -fshort-enums \
                  -DBOOTSTART=$(BOOTSTART) -DCPU_MHZ=$(CPU_MHZ) -DCPU_TYPE=$(CPU_TYPE)

all: main.hex bootloader.hex

.c.o:
	$(COMPILE) -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@

fuse:
	$(AVRDUDE) -U hfuse:w:$(HFUSE):m -U lfuse:w:$(LFUSE):m efuse:w:0xFF:m

install: main.hex
	$(BOOTLOADER) -U flash:w:main.hex:i

jtag: combined.hex
	$(AVRDUDE) -U flash:w:combined.hex:i

clean:
	rm -f main.hex main.elf bootloader.hex bootloader.elf $(OBJECTS) $(BOOT_OBJECTS)

disasm:	main.elf
	avr-objdump -d main.elf

size: main.elf
	avr-size -C --mcu=$(DEVICE) main.elf

debug: main.elf
	avarice -g --part $(DEVICE) --dragon --jtag usb --file main.elf :4242

debug-bootloader: bootloader.elf
	avarice -g --part $(DEVICE) --dragon --jtag usb --file bootloader.elf :4242

main.elf: $(OBJECTS)
	$(COMPILE) -o main.elf $(OBJECTS)

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex

bootloader.elf: bootloader.o
	$(COMPILE) -Wl,--section-start=.text=$(BOOTSTART) -o bootloader.elf $(BOOT_OBJECTS)

bootloader.hex: bootloader.elf
	rm -f bootloader.hex
	avr-objcopy -j .text -j .data -O ihex bootloader.elf bootloader.hex

combined.hex: bootloader.hex main.hex
	srec_cat bootloader.hex -I main.hex -I -o combined.hex -I
