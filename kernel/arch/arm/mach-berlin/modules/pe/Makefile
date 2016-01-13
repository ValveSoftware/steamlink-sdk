include $(srctree)/arch/arm/mach-berlin/mv88de3100.mk

ifeq ($(CONFIG_BERLIN2CD),y)
        MV_DEFINE += -DBERLIN_CHIP_VERSION=BERLIN_BG2CD_A0
endif

HEADER_PATH +=  \
	-I$(srctree)/arch/arm/mach-berlin/modules/shm \
	-I$(srctree)/arch/arm/mach-berlin/modules/cc  \
	-I$(srctree)/arch/arm/mach-berlin/modules/nfc  \
	-I$(srctree)/arch/arm/mach-berlin/modules/pe/gsinc \

ifeq ($(CONFIG_BERLIN2CD),y)
       HEADER_PATH += -I$(srctree)/arch/arm/mach-berlin/modules/pe/gsinc/Firmware_Berlin_BG2CD_A0
endif

ccflags-y       += $(MV_DEFINE) $(HEADER_PATH)

ccflags-y       += -DBERLIN_SINGLE_CPU


obj-$(CONFIG_BERLIN_PE) += galois_pe.o
galois_pe-objs := pe_driver.o avio_dhub_drv.o pe_agent_driver.o

