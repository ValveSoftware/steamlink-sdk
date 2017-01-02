ifeq ($(STEAMLINK),1)
CPUDEFS += -DHAS_CYCLONE=1 -DHAS_DRZ80=1

OBJDIRS += $(OBJ)/cpu/m68000_cyclone $(OBJ)/cpu/z80_drz80
CPUOBJS += $(OBJ)/cpu/m68000_cyclone/cyclone.o $(OBJ)/cpu/m68000_cyclone/c68000.o
CPUOBJS += $(OBJ)/cpu/z80_drz80/drz80.o $(OBJ)/cpu/z80_drz80/drz80_z80.o

#OBJDIRS += $(OBJ)/cpu/nec_armnec
#CPUDEFS += -DHAS_ARMNEC

#CPUOBJS += $(OBJ)/cpu/nec_armnec/armV30.o $(OBJ)/cpu/nec_armnec/armV33.o $(OBJ)/cpu/nec_armnec/armnecintrf.o
endif

OSOBJS = $(OBJ)/sdl/minimal.o \
	$(OBJ)/sdl/sdl.o $(OBJ)/sdl/video.o $(OBJ)/sdl/blit.o \
	$(OBJ)/sdl/sound.o $(OBJ)/sdl/input.o $(OBJ)/sdl/fileio.o \
	$(OBJ)/sdl/config.o $(OBJ)/sdl/fronthlp.o \
	$(OBJ)/sdl/gp2x_frontend.o $(OBJ)/sdl/glib_ini_file.o
