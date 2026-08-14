# License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)

#TODO : better libs management

DEVICE     = atmega88pa
F_CPU      = 8000000	# in Hz
# see below for fuse values for particular devices
FUSE_L     = 0xE2 #0xAE(orig) / 0xEE for crystal ??
FUSE_H     = 0xDD #0xDF(orig)
FUSE_E     = 0xF9 #0xF5(orig)
PROGRAMMER = arduino_as_isp
PORT      ?= /dev/ttyUSB0
BAUDRATE   = 19200 # BAUDRATE is for PROGRAMMER (not firmware UART)
AVRDUDE    = avrdude -c $(PROGRAMMER) -P $(PORT) -p $(DEVICE) -b $(BAUDRATE)

I2C_ADDRESS ?= 0x20
DEBUG ?= 0

CFLAGS  = -Iavr-i2c-slave -Iuart -I. -DDEBUG=$(DEBUG)  -Os -DI2C_ADDRESS=$(I2C_ADDRESS)
CFLAGS += -ffunction-sections -fdata-sections -save-temps
LDFLAGS += -Wl,--gc-sections,--relax

OBJECTS = main.o avr-i2c-slave/I2CSlave.o uart/uart.o

COMPILE = avr-gcc -DF_CPU=$(F_CPU) $(CFLAGS) $(LDFLAGS) -mmcu=$(DEVICE)


# symbolic targets:
help:
	@echo "This Makefile has no default rule. Use one of the following:"
	@echo " * The following command line option are avaiable : "
	@echo "   for hex:                            DEBUG=$(DEBUG)"
	@echo "   for program, eeprom:                I2C_ADDRESS=$(I2C_ADDRESS)"
	@echo "   for progam, flash, fuse, eeprom:    PORT=$(PORT)"
	@echo " * rules:"
	@echo "make hex ..................... to build main.hex"
	@echo "make program ................. to flash firmware, fuses and eeprom"
	@echo "make flash ................... to flash only the firmware"
	@echo "make fuse .................... to flash only the fuses"
	@echo "make fuse .................... to flash only the eeprom (device i²c address)"
	@echo "make clean ................... to delete objects and hex file"
	@echo "make all ..................... short for : make clean, make hex, make program"
	@echo "make disam ................... dump firmware assembler code"

all: clean hex program

hex: main.hex

program: flash fuse eeprom

# rule for programming fuse bits:
fuse:
	@[ "$(FUSE_H)" != "" -a "$(FUSE_L)" != ""  -a "$(FUSE_E)" != "" ] || \
		{ echo "*** Edit Makefile and choose values for FUSE_L, FUSE_H, and FUSE_E !"; exit 1; }
	$(AVRDUDE) -U hfuse:w:$(strip $(FUSE_H)):m -U lfuse:w:$(strip $(FUSE_L)):m efuse:w:$(strip $(FUSE_E)):m

# rule for uploading firmware:
flash: main.hex
	$(AVRDUDE) -U flash:w:main.hex:i

# rule for flashing I²C Address into EEPROM
eeprom:
	printf '%b' '\xFF\x'$$(printf '%02X' $$(( $(I2C_ADDRESS) ))) | \
	    $(AVRDUDE) -U eeprom:w:-:r

# rule for deleting dependent files (those which can be built by Make):
clean:
	# TODO : clean libs ??
	rm -f main.hex main.o main.elf $(OBJECTS)

# Generic rule for compiling C files:
.c.o:
	$(COMPILE) -c $< -o $@

# Generic rule for assembling Assembler source files:
.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

# Generic rule for compiling C to assembler, used for debugging only.
.c.s:
	$(COMPILE) -S $< -o $@

# file targets:

main.elf: $(OBJECTS)
	$(COMPILE) -L /usr/lib/avr/lib/ldscripts/ -o main.elf $(OBJECTS)

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
	avr-size main.hex

# debugging targets:

disasm:	main.elf
	avr-objdump -d main.elf

