include $(srctree)/arch/arm/mach-berlin/mv88de3100.mk

MWD=$(srctree)/arch/arm/mach-berlin
PWD=$(MWD)/modules/fastlogo

HEADER_PATH :=  \
	-I$(MWD)/include \
	-I$(MWD)/include/mach \
	-I$(MWD)/modules/shm \
	-I$(MWD)/modules/cc  \
	-I$(MWD)/modules/nfc \
	-I$(MWD)/modules/pe \
	-I$(MWD)/modules/pe/gsinc \
	-I$(MWD)/modules/pe/gsinc/$(FIRMWARE) \
	-I$(PWD)/Common/include \
	-I$(PWD)/Common/include/$(FIRMWARE)

ccflags-y	+= -DLOGO_ENABLE_MAIN=1 -DLOGO_ENABLE_PIP=0 -DLOGO_ENABLE_AUX=0
ccflags-y	+= -DLOGO_PROC_FS=1 -DLOGO_TIME_PROFILE=1
ifeq ($(CONFIG_BERLIN_SHM),y)
ccflags-y	+= -DLOGO_USE_SHM=1
endif
ccflags-y	+= $(MV_DEFINE) $(HEADER_PATH)

ccflags-y	+= -I$(PWD)/THINVPP/source/include/include_BG2_A0

ccflags-y 	+= -I$(PWD)/THINVPP/include
ccflags-y 	+= -I$(PWD)/THINVPP/source/include
ccflags-y 	+= -I$(PWD)/THINVPP/source/MISC/include
asflags-y 	+= -I$(PWD)


FASTLOGO_OBJ += THINVPP/source/thinvpp_api.o
FASTLOGO_OBJ += THINVPP/source/thinvpp_apifuncs.o
FASTLOGO_OBJ += THINVPP/source/thinvpp_isr.o
FASTLOGO_OBJ += THINVPP/source/MISC/source/thinvpp_bcmbuf.o
ifeq ($(CONFIG_BERLIN_PE_MODULE),y)
#use the avio_dhub_drv in pe module
else
FASTLOGO_OBJ += DHUB/source/avio_dhub_drv.o
endif

obj-$(CONFIG_BERLIN_FASTLOGO) += galois_fastlogo.o
galois_fastlogo-objs := fastlogo_driver.o $(FASTLOGO_OBJ)
