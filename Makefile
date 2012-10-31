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

HARDWARE_VERSION := 3

PROGRAMMER = -c dragon_jtag -P usb
OBJECTS    = command.o gps.o download.o monitor.o main.o


ifeq (1, $(shell if [ "${HARDWARE_VERSION}" -gt "2" ]; then echo 1; fi))
	DEVICE = atmega1284p
    FUSES  = -U hfuse:w:0x19:m -U lfuse:w:0xFF:m efuse:w:0xFF:m
    OBJECTS += display_led.o
else
	DEVICE = atmega128
    FUSES  = -U hfuse:w:0x09:m -U lfuse:w:0xFF:m efuse:w:0xFF:m
    OBJECTS += display_lcd.o
endif

# Tune the lines below only if you know what you are doing:
AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE)
COMPILE = avr-gcc -g -mmcu=$(DEVICE) -Wall -Wextra -Werror -Os -std=gnu99 -funsigned-bitfields -fshort-enums -DHARDWARE_VERSION=${HARDWARE_VERSION}

all: main.hex

.c.o:
	$(COMPILE) -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@

flash:	all
	$(AVRDUDE) -U flash:w:main.hex:i

fuse:
	$(AVRDUDE) $(FUSES)

install: flash fuse

clean:
	rm -f main.hex main.elf $(OBJECTS)

main.elf: $(OBJECTS)
	$(COMPILE) -o main.elf $(OBJECTS)

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex

disasm:	main.elf
	avr-objdump -d main.elf

size: main.elf
	avr-size -C --mcu=$(DEVICE) main.elf

debug: main.elf
	avarice -g --part $(DEVICE) --dragon --jtag usb --file main.elf :4242
