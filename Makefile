##***************************************************************************
##
##  File        : Makefile
##  Author      : Paul Chote
##  Description : Makefile to build the project using avr-gcc and avrdude
##
##***************************************************************************

# Hardware versions
# 1 - Original board design, no monitor
# 2 - Original board design plus monitor circuit
# 3 - New board design
# 4 - New board design, underclocked to 10MHz

PORT := /dev/tty.usbserial-00001004
HARDWARE_VERSION := 3

AVRDUDE = avrdude -c dragon_jtag -P usb -p $(DEVICE)
OBJECTS = command.o gps.o download.o monitor.o main.o

BOOTLOADER   = avrdude -c avr109 -p $(DEVICE) -b 9600 -P $(PORT)
BOOT_OBJECTS = bootloader.o
BOOTSTART    = 0x1E000

ifeq (${HARDWARE_VERSION}, 4)
		DEVICE = atmega1284p
		# Set fuses to use external clock source
	    FUSES  = -U hfuse:w:0x18:m -U lfuse:w:0xF0:m efuse:w:0xFF:m
	    OBJECTS += display_led.o
else
	ifeq (1, $(shell if [ "${HARDWARE_VERSION}" -gt "2" ]; then echo 1; fi))
		DEVICE   = atmega1284p
	    FUSES    = -U hfuse:w:0x18:m -U lfuse:w:0xFF:m efuse:w:0xFF:m
	    OBJECTS += display_led.o
	else
		DEVICE   = atmega128
	    FUSES    = -U hfuse:w:0x09:m -U lfuse:w:0xFF:m efuse:w:0xFF:m
	    OBJECTS += display_lcd.o
	endif
endif

COMPILE = avr-gcc -g -mmcu=$(DEVICE) -Wall -Wextra -Werror -Os -std=gnu99 -funsigned-bitfields -fshort-enums \
                  -DHARDWARE_VERSION=${HARDWARE_VERSION} -DBOOTSTART=$(BOOTSTART)

all: main.hex bootloader.hex

.c.o:
	$(COMPILE) -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@

fuse:
	$(AVRDUDE) $(FUSES)

install: main.hex
	$(BOOTLOADER) -U flash:w:main.hex:i

bootloader: bootloader.hex
	$(AVRDUDE) -U flash:w:bootloader.hex:i

clean:
	rm -f main.hex main.elf bootloader.hex bootloader.elf $(OBJECTS) $(BOOT_OBJECTS)

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

disasm:	main.elf
	avr-objdump -d main.elf

size: main.elf
	avr-size -C --mcu=$(DEVICE) main.elf

debug: main.elf
	avarice -g --part $(DEVICE) --dragon --jtag usb --file main.elf :4242

debug-bootloader: bootloader.elf
	avarice -g --part $(DEVICE) --dragon --jtag usb --file bootloader.elf :4242
