
$(OBJ)/memory.o: src/memory.cpp src/memory.h src/memory_read.h src/memory_write.h
$(OBJ)/tilemap.o: src/tilemap.cpp src/tilemap.h src/tilemap_draw.h

# the core object files (without target specific objects;
# those are added in the target.mak files)
COREOBJS = $(OBJ)/version.o $(OBJ)/info.o $(OBJ)/audit.o $(OBJ)/datafile.o \
    $(OBJ)/state.o $(OBJ)/png.o $(OBJ)/artwork.o $(OBJ)/unzip.o \
    $(OBJ)/zlib/adler32.o $(OBJ)/zlib/compress.o $(OBJ)/zlib/crc32.o \
	$(OBJ)/zlib/gzio.o $(OBJ)/zlib/uncompr.o $(OBJ)/zlib/deflate.o \
	$(OBJ)/zlib/trees.o $(OBJ)/zlib/zutil.o $(OBJ)/zlib/inflate.o \
	$(OBJ)/zlib/infback.o $(OBJ)/zlib/inftrees.o $(OBJ)/zlib/inffast.o \
	$(OBJ)/profiler.o $(OBJ)/cheat.o $(OBJ)/hiscore.o \
	$(OBJ)/input.o $(OBJ)/inptport.o \
    $(OBJ)/mame.o $(OBJ)/usrintrf.o $(OBJ)/ui_text.o \
	$(OBJ)/tilemap.o $(OBJ)/sprite.o $(OBJ)/gfxobj.o \
	$(OBJ)/drawgfx.o $(OBJ)/palette.o $(OBJ)/common.o \
	$(OBJ)/cpuintrf.o $(OBJ)/memory.o $(OBJ)/timer.o \
	$(sort $(CPUOBJS)) \
	$(OBJ)/sndintrf.o $(OBJ)/sound/streams.o $(OBJ)/sound/mixer.o \
	$(SOUNDOBJS) \
	$(OBJ)/machine/z80fmly.o $(OBJ)/machine/6821pia.o \
	$(OBJ)/machine/8255ppi.o $(OBJ)/machine/ticket.o $(OBJ)/machine/eeprom.o \
	$(OBJ)/vidhrdw/generic.o $(OBJ)/vidhrdw/vector.o \
	$(OBJ)/vidhrdw/avgdvg.o $(OBJ)/machine/mathbox.o \
	$(OBJ)/sound/votrax.o

TOOLS = romcmp$(EXE)
TEXTS = gamelist.txt

