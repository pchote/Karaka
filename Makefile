#***************************************************************************
#
#  File........: Makefile
#  Author(s)...: Paul Chote
#  Description.: Makefile to build the project using avr-gcc and avrdude
#
#***************************************************************************

DEVICE     = atmega128
PROGRAMMER = -c dragon_jtag -P usb
OBJECTS    = main.o command.o gps.o display.o msec_timer.o sync_pulse.o
FUSES      = -U hfuse:w:0x09:m -U lfuse:w:0xFF:m efuse:w:0xFF:m

# Tune the lines below only if you know what you are doing:
AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE)
COMPILE = avr-gcc -mmcu=$(DEVICE) -Wall -Os -std=gnu99 -funsigned-bitfields -fshort-enums

all:	main.hex

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
