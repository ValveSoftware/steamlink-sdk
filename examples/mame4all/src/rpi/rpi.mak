CPUDEFS += -DHAS_CYCLONE=1 -DHAS_DRZ80=1

OBJDIRS += $(OBJ)/cpu/m68000_cyclone $(OBJ)/cpu/z80_drz80
CPUOBJS += $(OBJ)/cpu/m68000_cyclone/cyclone.o $(OBJ)/cpu/m68000_cyclone/c68000.o
CPUOBJS += $(OBJ)/cpu/z80_drz80/drz80.o $(OBJ)/cpu/z80_drz80/drz80_z80.o

#OBJDIRS += $(OBJ)/cpu/nec_armnec
#CPUDEFS += -DHAS_ARMNEC

#CPUOBJS += $(OBJ)/cpu/nec_armnec/armV30.o $(OBJ)/cpu/nec_armnec/armV33.o $(OBJ)/cpu/nec_armnec/armnecintrf.o

OSOBJS = $(OBJ)/rpi/minimal.o \
	$(OBJ)/rpi/rpi.o $(OBJ)/rpi/video.o $(OBJ)/rpi/blit.o \
	$(OBJ)/rpi/sound.o $(OBJ)/rpi/input.o $(OBJ)/rpi/fileio.o \
	$(OBJ)/rpi/config.o $(OBJ)/rpi/fronthlp.o \
	$(OBJ)/rpi/gp2x_frontend.o $(OBJ)/rpi/glib_ini_file.o
