CPUDEFS += -DHAS_CYCLONE=1 -DHAS_DRZ80=1
OBJDIRS += $(OBJ)/cpu/m68000_cyclone $(OBJ)/cpu/z80_drz80
CPUOBJS += $(OBJ)/cpu/m68000_cyclone/cyclone.o $(OBJ)/cpu/m68000_cyclone/c68000.o
CPUOBJS += $(OBJ)/cpu/z80_drz80/drz80.o $(OBJ)/cpu/z80_drz80/drz80_z80.o

#OBJDIRS += $(OBJ)/cpu/nec_armnec
#CPUDEFS += -DHAS_ARMNEC
#CPUOBJS += $(OBJ)/cpu/nec_armnec/armV30.o $(OBJ)/cpu/nec_armnec/armV33.o $(OBJ)/cpu/nec_armnec/armnecintrf.o

OSOBJS = $(OBJ)/gp2x/minimal.o $(OBJ)/gp2x/uppermem.o $(OBJ)/gp2x/cpuctrl.o \
	$(OBJ)/gp2x/squidgehack.o $(OBJ)/gp2x/flushcache.o \
	$(OBJ)/gp2x/memcmp.o $(OBJ)/gp2x/memcpy.o $(OBJ)/gp2x/memset.o \
	$(OBJ)/gp2x/strcmp.o $(OBJ)/gp2x/strlen.o $(OBJ)/gp2x/strncmp.o \
	$(OBJ)/gp2x/gp2x.o $(OBJ)/gp2x/video.o $(OBJ)/gp2x/blit.o \
	$(OBJ)/gp2x/sound.o $(OBJ)/gp2x/input.o $(OBJ)/gp2x/fileio.o \
	$(OBJ)/gp2x/usbjoy.o $(OBJ)/gp2x/usbjoy_mame.o \
	$(OBJ)/gp2x/config.o $(OBJ)/gp2x/fronthlp.o

mame.gpe:
	$(LD) $(LDFLAGS) src/gp2x/cpuctrl.cpp \
		src/gp2x/flushcache.s src/gp2x/minimal.cpp \
		src/gp2x/squidgehack.cpp src/gp2x/usbjoy.cpp \
		src/gp2x/usbjoy_mame.cpp src/gp2x/uppermem.cpp \
		src/gp2x/gp2x_frontend.cpp $(LIBS) -o $@
