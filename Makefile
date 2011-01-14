# Name: Makefile
# Author: Paul Chote
# Copyright: TBD
# License: TBD

DEVICE     = atmega128
PROGRAMMER = -c dragon_jtag -P usb:7d:af
OBJECTS    = main.o Command_Layer.o GPS.o LCD_LIB.o msec_timer.o sync_pulse.o timer1.o UART_Math.o usart.o usart1.o
FUSES      = -U hfuse:w:0x09:m -U lfuse:w:0xFF:m efuse:w:0xFF:m

# Tune the lines below only if you know what you are doing:

AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE)

# Everything after -Os is copied from the AVR studio output
COMPILE = avr-gcc -mmcu=$(DEVICE) -Wall -Os -std=gnu99 -funsigned-char -funsigned-bitfields -fshort-enums

# symbolic targets:
all:	main.hex

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

.c.s:
	$(COMPILE) -S $< -o $@

flash:	all
	$(AVRDUDE) -U flash:w:main.hex:i

fuse:
	$(AVRDUDE) $(FUSES)

# Xcode uses the Makefile targets "", "clean" and "install"
install: flash fuse

clean:
	rm -f main.hex main.elf $(OBJECTS)

# file targets:
main.elf: $(OBJECTS)
	$(COMPILE) -o main.elf $(OBJECTS)

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
# If you have an EEPROM section, you must also create a hex file for the
# EEPROM and add it to the "flash" target.

# Targets for code debugging and analysis:
disasm:	main.elf
	avr-objdump -d main.elf

cpp:
	$(COMPILE) -E main.c
