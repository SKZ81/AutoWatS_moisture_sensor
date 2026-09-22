# License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)

# ------------------------------------------------------------------------------
# Firmware version (v0.1)
# ------------------------------------------------------------------------------

FIRMWARE_VERSION_MAJOR = 0x00
FIRMWARE_VERSION_MINOR = 0x01

# ------------------------------------------------------------------------------
# Configuration
# ------------------------------------------------------------------------------

DEVICE     = atmega88pa
F_CPU      = 16000000	# in Hz

# see below for fuse values for particular devices
# FUSE_L     = 0xE2 #0xAE(orig)
FUSE_L     = 0xFF # Crystal enabled
FUSE_H     = 0xDD #0xDF(orig)
FUSE_E     = 0xF9 #0xF5(orig)

PROGRAMMER = arduino_as_isp
PORT      ?= /dev/ttyUSB0
BAUDRATE   = 19200 # BAUDRATE is for PROGRAMMER (not firmware UART)

AVRDUDE = avrdude -c $(PROGRAMMER) -P $(PORT) -p $(DEVICE) -b $(BAUDRATE)


# I2C address stored in EEPROM
I2C_ADDRESS ?= 0x20
EXCITATION_FREQ ?=4

DEBUG ?= 0

# Main source file.
# Can be overridden from the command line, e.g.:
#   make MAIN=main_test.c
MAIN ?= main_moisture.c

SOURCES = $(MAIN)


# ------------------------------------------------------------------------------
# Libraries
# ------------------------------------------------------------------------------

LIBS     = avr_uart avr-i2c-slave
LIBS_DIR = ../../libs


LIB_EXCLUDE_SOURCES = \
	$(LIBS_DIR)/avr-i2c-slave/main.c #\
# Add other excluded files if necessary, eg:
#	$(LIBS_DIR)/foo/example.c \
#	$(LIBS_DIR)/bar/test.c

# ------------------------------------------------------------------------------
# Build directories / files
# ------------------------------------------------------------------------------

BUILD_DIR = build-$(DEVICE)

ELF = $(BUILD_DIR)/main.elf
HEX = $(BUILD_DIR)/main.hex


# Find all C sources in the configured libraries.
LIB_SOURCES = $(foreach lib,$(LIBS),$(wildcard $(LIBS_DIR)/$(lib)/*.c))
LIB_SOURCES := $(filter-out $(LIB_EXCLUDE_SOURCES),$(LIB_SOURCES))

# Generate list of target objects for current project directory
PROJECT_OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))


# Generate list of target objects for used libraries
LIB_OBJECTS = $(patsubst $(LIBS_DIR)/%.c,$(BUILD_DIR)/libs/%.o,$(LIB_SOURCES))


OBJECTS = $(PROJECT_OBJECTS) $(LIB_OBJECTS)


# ------------------------------------------------------------------------------
# Compiler / linker flags
# ------------------------------------------------------------------------------

CC = avr-gcc

CFLAGS = \
	-DF_CPU=$(F_CPU) \
	-mmcu=$(DEVICE) \
	-I. \
	$(foreach lib,$(LIBS),-I$(LIBS_DIR)/$(lib)) \
	-DDEBUG=$(DEBUG) \
	-DI2C_ADDRESS=$(I2C_ADDRESS) \
	-Os \
	-ffunction-sections \
	-fdata-sections \
	-save-temps \
	-MMD \
	-MP

LDFLAGS = \
	-mmcu=$(DEVICE) \
	-Wl,--gc-sections,--relax


# Automatically generated dependency files.
-include $(OBJECTS:.o=.d)


# ------------------------------------------------------------------------------
# Symbolic targets
# ------------------------------------------------------------------------------

help:
	@echo "This Makefile has no default rule. Use one of the following:"
	@echo
	@echo " * Command line options:"
	@echo "   DEBUG=$(DEBUG)"
	@echo "   MAIN=$(MAIN)"
	@echo "   I2C_ADDRESS=$(I2C_ADDRESS)"
	@echo "   PORT=$(PORT)"
	@echo
	@echo " * Rules:"
	@echo "make hex ..................... to build firmware hex"
	@echo "make program ................. to flash firmware, fuses and EEPROM"
	@echo "make flash ................... to flash only the firmware"
	@echo "make fuse .................... to flash only the fuses"
	@echo "make eeprom .................. to flash only the EEPROM (I2C address)"
	@echo "make clean ................... to delete build files"
	@echo "make all ..................... short for: make clean, make hex, make program"
	@echo "make disasm .................. dump firmware assembler code"


all: clean hex program

hex: $(HEX)

program: flash fuse eeprom


# ------------------------------------------------------------------------------
# Fuses
# ------------------------------------------------------------------------------

fuse:
	@[ "$(FUSE_H)" != "" -a "$(FUSE_L)" != "" -a "$(FUSE_E)" != "" ] || \
		{ echo "*** Edit Makefile and choose values for FUSE_L, FUSE_H, and FUSE_E !"; exit 1; }

	$(AVRDUDE) \
		-U hfuse:w:$(strip $(FUSE_H)):m \
		-U lfuse:w:$(strip $(FUSE_L)):m \
		-U efuse:w:$(strip $(FUSE_E)):m


# ------------------------------------------------------------------------------
# Firmware
# ------------------------------------------------------------------------------

flash: $(HEX)
	$(AVRDUDE) -U flash:w:$(HEX):i


# ------------------------------------------------------------------------------
# EEPROM
# ------------------------------------------------------------------------------

# Flash I2C address into EEPROM.
#
# EEPROM layout:
#   address 0x00 : 0xFF
#   address 0x01 : I2C address
#
# printf accepts both decimal and hexadecimal values for I2C_ADDRESS,
# e.g.:
#   I2C_ADDRESS=32
#   I2C_ADDRESS=0x20

eeprom:

	printf '%b' '\x$$(printf '%02X' $$(( $(FIRMWARE_VERSION_MAJOR) )))\x$$(printf '%02X' $$(( $(FIRMWARE_VERSION_MINOR) )))\x'$$(printf '%02X' $$(( $(I2C_ADDRESS) )))'\x'$$(printf '%02X' $$(( $(EXCITATION_FREQ) ))) | \
	$(AVRDUDE) -U eeprom:w:-:r

# ------------------------------------------------------------------------------
# Cleaning
# ------------------------------------------------------------------------------

clean:
	rm -rf $(BUILD_DIR)


# ------------------------------------------------------------------------------
# Generic compilation rules
# ------------------------------------------------------------------------------

# Compile Project sources
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


# Compile Library sources, keeping relative path, eg:
#   $(LIBS_DIR)/libABC/file.c
#        ->
#   $(BUILD_DIR)/libs/libABC/file.o

$(BUILD_DIR)/libs/%.o: $(LIBS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


# Generic rule for assembling Assembler source files.
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -x assembler-with-cpp -c $< -o $@


# Generic rule for compiling C to assembler, used for debugging only.
$(BUILD_DIR)/%.s: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -S $< -o $@


# ------------------------------------------------------------------------------
# Linking / HEX generation
# ------------------------------------------------------------------------------

$(ELF): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)


$(HEX): $(ELF)
	avr-objcopy -j .text -j .data -O ihex $< $@
	avr-size $<


# ------------------------------------------------------------------------------
# Debugging
# ------------------------------------------------------------------------------

disasm: $(ELF)
	avr-objdump -d $(ELF)
