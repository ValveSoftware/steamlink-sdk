ifeq ($(TARGET),)
TARGET = mame
endif

# set this the operating system you're building for
# (actually you'll probably need your own main makefile anyways)
# MAMEOS = msdos
#MAMEOS = gp2x
MAMEOS = rpi

# extension for executables
# EXE = .exe
EXE =

# CPU core include paths
VPATH=src $(wildcard src/cpu/*)

# compiler, linker and utilities
MD = @mkdir -p
#RM = rm -f
#CC  = gcc
#CPP = g++
#AS  = as
LD  = $(CXX)
STRIP = :
CPP = $(CXX)

EMULATOR = $(TARGET)$(EXE)

DEFS = -DGP2X -DLSB_FIRST -DALIGN_INTS -DALIGN_SHORTS -DINLINE="static __inline" -Dasm="__asm__ __volatile__" -DMAME_UNDERCLOCK -DENABLE_AUTOFIRE -DBIGCASE
##sq DEFS = -DGP2X -DLSB_FIRST -DALIGN_INTS -DALIGN_SHORTS -DINLINE="static __inline" -Dasm="__asm__ __volatile__" -DMAME_UNDERCLOCK -DMAME_FASTSOUND -DENABLE_AUTOFIRE -DBIGCASE

OPTIMIZE := -g -O2
CFLAGS += -fsigned-char $(DEVLIBS) \
	-Isrc -Isrc/$(MAMEOS) -Isrc/zlib \
	-I$(MARVELL_ROOTFS)/usr/include/SDL2 \
	$(OPTIMIZE) -ffast-math -fno-builtin -fsingle-precision-constant \
	-Wall -Wno-sign-compare -Wunused -Wpointer-arith -Wcast-align -Waggregate-return -Wshadow -Wno-write-strings -Wno-narrowing

LIBS = -lm -lpthread -lSDL2 -lGLESv2 -lEGL -lasound -lrt

OBJ = obj_$(TARGET)_$(MAMEOS)
OBJDIRS = $(OBJ) $(OBJ)/cpu $(OBJ)/sound $(OBJ)/$(MAMEOS) \
	$(OBJ)/drivers $(OBJ)/machine $(OBJ)/vidhrdw $(OBJ)/sndhrdw \
	$(OBJ)/zlib

all:	maketree $(EMULATOR)

# include the various .mak files
include src/core.mak
include src/$(TARGET).mak
include src/rules.mak
include src/sound.mak
include src/$(MAMEOS)/$(MAMEOS).mak

# combine the various definitions to one
CDEFS = $(DEFS) $(COREDEFS) $(CPUDEFS) $(SOUNDDEFS)

$(EMULATOR): $(COREOBJS) $(OSOBJS) $(DRVOBJS)
	$(LD) $(LDFLAGS) $(COREOBJS) $(OSOBJS) $(LIBS) $(DRVOBJS) -o $@
	$(STRIP) $(EMULATOR)	

$(OBJ)/%.o: src/%.c
	@echo Compiling $<...
	$(CC) $(CDEFS) $(CFLAGS) -c $< -o $@

$(OBJ)/%.o: src/%.cpp
	@echo Compiling $<...
	$(CPP) $(CDEFS) $(CFLAGS) -fno-rtti -c $< -o $@

$(OBJ)/%.o: src/%.s
	@echo Compiling $<...
	$(CPP) $(CDEFS) $(CFLAGS) -c $< -o $@

$(OBJ)/%.o: src/%.S
	@echo Compiling $<...
	$(CPP) $(CDEFS) $(CFLAGS) -c $< -o $@

$(sort $(OBJDIRS)):
	$(MD) $@

maketree: $(sort $(OBJDIRS))

clean:
	$(RM) -r $(OBJ)
	$(RM) $(EMULATOR)
