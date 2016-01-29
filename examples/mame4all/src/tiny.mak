# a tiny compile is without Neogeo games
COREDEFS += -DTINY_COMPILE=1 -DTINY_NAME=driver_olibochu

# uses these CPUs
CPUS+=Z80@
CPUS+=M68705@

# uses these SOUNDs
SOUNDS+=AY8910@
SOUNDS+=YM2203@

OBJS = $(OBJ)/drivers/olibochu.o

# MAME specific core objs
COREOBJS += $(OBJ)/driver.o
