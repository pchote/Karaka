##***************************************************************************
##
##  File        : Makefile
##  Author      : Paul Chote
##  Description : Makefile to build the project using avr-gcc and avrdude
##
##***************************************************************************

# System port to find the timer on for upgrading via the bootloader
PORT      := /dev/tty.usbserial-A5016EZP

##***************************************************************************

AVRDUDE = avrdude -c dragon_jtag -P usb -p $(DEVICE)
OBJECTS = usb.o gps.o camera.o main.o display.o

BOOTLOADER   = avrdude -c avr109 -p $(DEVICE) -b 9600 -P $(PORT)
BOOT_OBJECTS = bootloader.o
DEVICE       = atmega1284p
PARTCODE     = 0x43
BOOTSTART    = 0x1E000
PAGESIZE     = 256
F_CPU        = 10000000UL
HFUSE        = 0x18
LFUSE        = 0xF0

##***************************************************************************

COMPILE = avr-gcc -g -mmcu=$(DEVICE) -Wall -Wextra -Werror -Os -std=gnu99 -funsigned-bitfields -fshort-enums \
                  -DBOOTSTART=$(BOOTSTART) -DPAGESIZE=$(PAGESIZE) -DPARTCODE=$(PARTCODE) -DF_CPU=$(F_CPU)

all: main.hex bootloader.hex

.c.o:
	$(COMPILE) -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@

reset:
	$(CC) -o $@ reset.c

fuse:
	$(AVRDUDE) -U hfuse:w:$(HFUSE):m -U lfuse:w:$(LFUSE):m efuse:w:0xFF:m

install: main.hex reset
	./reset $(PORT)
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
