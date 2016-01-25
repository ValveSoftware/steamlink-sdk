# define NEOMAME
NEOMAME = 1

# tiny compile
COREDEFS = -DNEOMAME

# override executable name
EMULATOR_EXE = neomame.exe

# CPUs
CPUS+=Z80@
CPUS+=M68000@

# SOUNDs
SOUNDS+=AY8910@
SOUNDS+=YM2610@

$(OBJ)/drivers/neogeo.o: src/machine/neogeo.cpp src/machine/pd4990a.cpp src/vidhrdw/neogeo.cpp src/drivers/neogeo.cpp

DRVOBJS = $(OBJ)/drivers/neogeo.o

# MAME specific core objs
COREOBJS += $(OBJ)/driver.o

