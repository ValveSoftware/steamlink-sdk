# List of CPU core (and, for a debug build, disassembler) object files

CPU=$(strip $(findstring Z80@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/z80
CPUDEFS += -DHAS_Z80=1
CPUOBJS += $(OBJ)/cpu/z80/z80.o
$(OBJ)/cpu/z80/z80.o: z80.cpp z80.h z80ops.h
endif

CPU=$(strip $(findstring Z80GB@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/z80gb
CPUDEFS += -DHAS_Z80GB=1
CPUOBJS += $(OBJ)/cpu/z80gb/z80gb.o
$(OBJ)/cpu/z80gb/z80gb.o: z80gb.cpp z80gb.h daa_tab.h opc_cb.h opc_main.h
endif

CPU=$(strip $(findstring 8080@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/i8085
CPUDEFS += -DHAS_8080=1
CPUOBJS += $(OBJ)/cpu/i8085/i8085.o
$(OBJ)/cpu/i8085/i8085.o: i8085.cpp i8085.h i8085cpu.h i8085daa.h
endif

CPU=$(strip $(findstring 8085A@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/i8085
CPUDEFS += -DHAS_8085A=1
CPUOBJS += $(OBJ)/cpu/i8085/i8085.o
$(OBJ)/cpu/i8085/i8085.o: i8085.cpp i8085.h i8085cpu.h i8085daa.h
endif

CPU=$(strip $(findstring M6502@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6502
CPUDEFS += -DHAS_M6502=1
CPUOBJS += $(OBJ)/cpu/m6502/m6502.o
$(OBJ)/cpu/m6502/m6502.o: m6502.cpp m6502.h ops02.h t6502.cpp t65c02.cpp t65sc02.cpp t6510.cpp
endif

CPU=$(strip $(findstring M65C02@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6502
CPUDEFS += -DHAS_M65C02=1
CPUOBJS += $(OBJ)/cpu/m6502/m6502.o
$(OBJ)/cpu/m6502/m6502.o: m6502.cpp m6502.h ops02.h t6502.cpp t65c02.cpp t65sc02.cpp t6510.cpp
endif

CPU=$(strip $(findstring M65SC02@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6502
CPUDEFS += -DHAS_M65SC02=1
CPUOBJS += $(OBJ)/cpu/m6502/m6502.o
$(OBJ)/cpu/m6502/m6502.o: m6502.cpp m6502.h ops02.h t6502.cpp t65c02.cpp t65sc02.cpp t6510.cpp
endif

CPU=$(strip $(findstring M65CE02@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6502
CPUDEFS += -DHAS_M65CE02=1
CPUOBJS += $(OBJ)/cpu/m6502/m65ce02.o
$(OBJ)/cpu/m6502/m65ce02.o: m65ce02.cpp m65ce02.h opsce02.h t65ce02.cpp
endif

CPU=$(strip $(findstring M6509@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6502
CPUDEFS += -DHAS_M6509=1
CPUOBJS += $(OBJ)/cpu/m6502/m6509.o
$(OBJ)/cpu/m6502/m6509.o: m6509.cpp m6509.h ops09.h t6509.cpp
endif

CPU=$(strip $(findstring M6510@,$(CPUS)))
ifneq ($(CPU),)
CPUDEFS += -DHAS_M6510=1
OBJDIRS += $(OBJ)/cpu/m6502
CPUOBJS += $(OBJ)/cpu/m6502/m6502.o
$(OBJ)/cpu/m6502/m6502.o: m6502.cpp m6502.h ops02.h t6502.cpp t65c02.cpp t65sc02.cpp t6510.cpp
endif

CPU=$(strip $(findstring M6510T@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6502
CPUDEFS += -DHAS_M6510=1 -DHAS_M6510T=1
CPUOBJS += $(OBJ)/cpu/m6502/m6502.o
$(OBJ)/cpu/m6502/m6502.o: m6502.cpp m6502.h ops02.h t6502.cpp t65c02.cpp t65sc02.cpp t6510.cpp
endif

CPU=$(strip $(findstring M7501@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6502
CPUDEFS += -DHAS_M6510=1 -DHAS_M7501=1
CPUOBJS += $(OBJ)/cpu/m6502/m6502.o
$(OBJ)/cpu/m6502/m6502.o: m6502.cpp m6502.h ops02.h t6502.cpp t65c02.cpp t65sc02.cpp t6510.cpp
endif

CPU=$(strip $(findstring M8502@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6502
CPUDEFS += -DHAS_M6510=1 -DHAS_M8502=1
CPUOBJS += $(OBJ)/cpu/m6502/m6502.o
$(OBJ)/cpu/m6502/m6502.o: m6502.cpp m6502.h ops02.h t6502.cpp t65c02.cpp t65sc02.cpp t6510.cpp
endif

CPU=$(strip $(findstring M4510@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6502
CPUDEFS += -DHAS_M4510=1
CPUOBJS += $(OBJ)/cpu/m6502/m4510.o
endif

CPU=$(strip $(findstring N2A03@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6502
CPUDEFS += -DHAS_N2A03=1
CPUOBJS += $(OBJ)/cpu/m6502/m6502.o
$(OBJ)/cpu/m6502/m6502.o: m6502.cpp m6502.h ops02.h t6502.cpp t65c02.cpp t65sc02.cpp t6510.cpp
endif

CPU=$(strip $(findstring H6280@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/h6280
CPUDEFS += -DHAS_H6280=1
CPUOBJS += $(OBJ)/cpu/h6280/h6280.o
$(OBJ)/cpu/h6280/h6280.o: h6280.cpp h6280.h h6280ops.h tblh6280.cpp
endif

CPU=$(strip $(findstring I86@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/i86
CPUDEFS += -DHAS_I86=1
CPUOBJS += $(OBJ)/cpu/i86/i86.o
$(OBJ)/cpu/i86/i86.o: i86.cpp instr86.cpp i86.h i86intf.h ea.h host.h modrm.h
endif

CPU=$(strip $(findstring I88@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/i86
CPUDEFS += -DHAS_I88=1
CPUOBJS += $(OBJ)/cpu/i86/i86.o
$(OBJ)/cpu/i86/i86.o: i86.cpp instr86.cpp i86.h i86intf.h ea.h host.h modrm.h
endif

CPU=$(strip $(findstring I186@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/i86
CPUDEFS += -DHAS_I186=1
CPUOBJS += $(OBJ)/cpu/i86/i86.o
$(OBJ)/cpu/i86/i86.o: i86.cpp instr186.cpp i86.h i186intf.h ea.h host.h modrm.h
endif

CPU=$(strip $(findstring I188@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/i86
CPUDEFS += -DHAS_I188=1
CPUOBJS += $(OBJ)/cpu/i86/i86.o
$(OBJ)/cpu/i86/i86.o: i86.cpp instr186.cpp i86.h i186intf.h ea.h host.h modrm.h
endif

CPU=$(strip $(findstring I286@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/i86
CPUDEFS += -DHAS_I286=1
CPUOBJS += $(OBJ)/cpu/i86/i286.o
$(OBJ)/cpu/i86/i86.o: i86.cpp instr286.cpp i86.h i286intf.h ea.h host.h modrm.h
endif

CPU=$(strip $(findstring V20@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/nec
CPUDEFS += -DHAS_V20=1
CPUOBJS += $(OBJ)/cpu/nec/nec.o
$(OBJ)/cpu/nec/nec.o: nec.cpp nec.h necintrf.h necea.h nechost.h necinstr.h necmodrm.h
endif

CPU=$(strip $(findstring V30@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/nec
CPUDEFS += -DHAS_V30=1
CPUOBJS += $(OBJ)/cpu/nec/nec.o
$(OBJ)/cpu/nec/nec.o: nec.cpp nec.h necintrf.h necea.h nechost.h necinstr.h necmodrm.h
endif

CPU=$(strip $(findstring V33@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/nec
CPUDEFS += -DHAS_V33=1
CPUOBJS += $(OBJ)/cpu/nec/nec.o
$(OBJ)/cpu/nec/nec.o: nec.cpp nec.h necintrf.h necea.h nechost.h necinstr.h necmodrm.h
endif

CPU=$(strip $(findstring I8035@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/i8039
CPUDEFS += -DHAS_I8035=1
CPUOBJS += $(OBJ)/cpu/i8039/i8039.o
$(OBJ)/cpu/i8039/i8039.o: i8039.cpp i8039.h
endif

CPU=$(strip $(findstring I8039@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/i8039
CPUDEFS += -DHAS_I8039=1
CPUOBJS += $(OBJ)/cpu/i8039/i8039.o
$(OBJ)/cpu/i8039/i8039.o: i8039.cpp i8039.h
endif

CPU=$(strip $(findstring I8048@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/i8039
CPUDEFS += -DHAS_I8048=1
CPUOBJS += $(OBJ)/cpu/i8039/i8039.o
$(OBJ)/cpu/i8039/i8039.o: i8039.cpp i8039.h
endif

CPU=$(strip $(findstring N7751@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/i8039
CPUDEFS += -DHAS_N7751=1
CPUOBJS += $(OBJ)/cpu/i8039/i8039.o
$(OBJ)/cpu/i8039/i8039.o: i8039.cpp i8039.h
endif

CPU=$(strip $(findstring M6800@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6800
CPUDEFS += -DHAS_M6800=1
CPUOBJS += $(OBJ)/cpu/m6800/m6800.o
$(OBJ)/cpu/m6800/m6800.o: m6800.cpp m6800.h 6800ops.cpp 6800tbl.cpp
endif

CPU=$(strip $(findstring M6801@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6800
CPUDEFS += -DHAS_M6801=1
CPUOBJS += $(OBJ)/cpu/m6800/m6800.o
$(OBJ)/cpu/m6800/m6800.o: m6800.cpp m6800.h 6800ops.cpp 6800tbl.cpp
endif

CPU=$(strip $(findstring M6802@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6800
CPUDEFS += -DHAS_M6802=1
CPUOBJS += $(OBJ)/cpu/m6800/m6800.o
$(OBJ)/cpu/m6800/m6800.o: m6800.cpp m6800.h 6800ops.cpp 6800tbl.cpp
endif

CPU=$(strip $(findstring M6803@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6800
CPUDEFS += -DHAS_M6803=1
CPUOBJS += $(OBJ)/cpu/m6800/m6800.o
$(OBJ)/cpu/m6800/m6800.o: m6800.cpp m6800.h 6800ops.cpp 6800tbl.cpp
endif

CPU=$(strip $(findstring M6808@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6800
CPUDEFS += -DHAS_M6808=1
CPUOBJS += $(OBJ)/cpu/m6800/m6800.o
$(OBJ)/cpu/m6800/m6800.o: m6800.cpp m6800.h 6800ops.cpp 6800tbl.cpp
endif

CPU=$(strip $(findstring HD63701@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6800
CPUDEFS += -DHAS_HD63701=1
CPUOBJS += $(OBJ)/cpu/m6800/m6800.o
$(OBJ)/cpu/m6800/m6800.o: m6800.cpp m6800.h 6800ops.cpp 6800tbl.cpp
endif

CPU=$(strip $(findstring NSC8105@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6800
CPUDEFS += -DHAS_NSC8105=1
CPUOBJS += $(OBJ)/cpu/m6800/m6800.o
$(OBJ)/cpu/m6800/m6800.o: m6800.cpp m6800.h 6800ops.cpp 6800tbl.cpp
endif

CPU=$(strip $(findstring M6805@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6805
CPUDEFS += -DHAS_M6805=1
CPUOBJS += $(OBJ)/cpu/m6805/m6805.o
$(OBJ)/cpu/m6805/m6805.o: m6805.cpp m6805.h 6805ops.cpp
endif

CPU=$(strip $(findstring M68705@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6805
CPUDEFS += -DHAS_M68705=1
CPUOBJS += $(OBJ)/cpu/m6805/m6805.o
$(OBJ)/cpu/m6805/m6805.o: m6805.cpp m6805.h 6805ops.cpp
endif

CPU=$(strip $(findstring HD63705@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6805
CPUDEFS += -DHAS_HD63705=1
CPUOBJS += $(OBJ)/cpu/m6805/m6805.o
$(OBJ)/cpu/m6805/m6805.o: m6805.cpp m6805.h 6805ops.cpp
endif

CPU=$(strip $(findstring HD6309@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6809
CPUDEFS += -DHAS_HD6309=1
CPUOBJS += $(OBJ)/cpu/m6809/m6809.o
$(OBJ)/cpu/m6809/m6809.o: m6809.cpp m6809.h 6809ops.h
endif

CPU=$(strip $(findstring M6809@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m6809
CPUDEFS += -DHAS_M6809=1
CPUOBJS += $(OBJ)/cpu/m6809/m6809.o
$(OBJ)/cpu/m6809/m6809.o: m6809.cpp m6809.h 6809ops.h
endif

CPU=$(strip $(findstring KONAMI@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/konami
CPUDEFS += -DHAS_KONAMI=1
CPUOBJS += $(OBJ)/cpu/konami/konami.o
$(OBJ)/cpu/konami/konami.o: konami.cpp konami.h konamops.cpp konamtbl.cpp
endif

CPU=$(strip $(findstring M68000@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m68000
CPUDEFS += -DHAS_M68000=1
OBJDIRS += $(OBJ)/cpu/m68000
$(OBJ)/cpu/m68000/m68kmame.o: m68k.h m68kconf.h m68kcpu.cpp m68kcpu.h m68kmame.cpp m68kmame.h m68kopac.cpp m68kopdm.cpp m68kopnz.cpp m68kops.cpp m68kops.h m68000.h
CPUOBJS += $(OBJ)/cpu/m68000/m68kmame.o
endif

CPU=$(strip $(findstring M68010@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m68000
CPUDEFS += -DHAS_M68010=1
OBJDIRS += $(OBJ)/cpu/m68000
$(OBJ)/cpu/m68000/m68kmame.o: m68k.h m68kconf.h m68kcpu.cpp m68kcpu.h m68kmame.cpp m68kmame.h m68kopac.cpp m68kopdm.cpp m68kopnz.cpp m68kops.cpp m68kops.h m68000.h
CPUOBJS += $(OBJ)/cpu/m68000/m68kmame.o
endif

CPU=$(strip $(findstring M68EC020@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m68000
CPUDEFS += -DHAS_M68EC020=1
OBJDIRS += $(OBJ)/cpu/m68000
$(OBJ)/cpu/m68000/m68kmame.o: m68k.h m68kconf.h m68kcpu.cpp m68kcpu.h m68kmame.cpp m68kmame.h m68kopac.cpp m68kopdm.cpp m68kopnz.cpp m68kops.cpp m68kops.h m68000.h
CPUOBJS += $(OBJ)/cpu/m68000/m68kmame.o
endif

CPU=$(strip $(findstring M68020@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/m68000
CPUDEFS += -DHAS_M68020=1
OBJDIRS += $(OBJ)/cpu/m68000
$(OBJ)/cpu/m68000/m68kmame.o: m68k.h m68kconf.h m68kcpu.cpp m68kcpu.h m68kmame.cpp m68kmame.h m68kopac.cpp m68kopdm.cpp m68kopnz.cpp m68kops.cpp m68kops.h m68000.h
CPUOBJS += $(OBJ)/cpu/m68000/m68kmame.o
endif

CPU=$(strip $(findstring T11@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/t11
CPUDEFS += -DHAS_T11=1
CPUOBJS += $(OBJ)/cpu/t11/t11.o
$(OBJ)/cpu/t11/t11.o: t11.cpp t11.h t11ops.cpp t11table.cpp
endif

CPU=$(strip $(findstring S2650@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/s2650
CPUDEFS += -DHAS_S2650=1
CPUOBJS += $(OBJ)/cpu/s2650/s2650.o
$(OBJ)/cpu/s2650/s2650.o: s2650.cpp s2650.h s2650cpu.h
endif

CPU=$(strip $(findstring TMS34010@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/tms34010
CPUDEFS += -DHAS_TMS34010=1
CPUOBJS += $(OBJ)/cpu/tms34010/tms34010.o
$(OBJ)/cpu/tms34010/tms34010.o: tms34010.cpp tms34010.h 34010ops.cpp 34010tbl.cpp 34010fld.cpp
endif

CPU=$(strip $(findstring TMS9900@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/tms9900
CPUDEFS += -DHAS_TMS9900=1
CPUOBJS += $(OBJ)/cpu/tms9900/tms9900.o
$(OBJ)/cpu/tms9900/tms9900.o: tms9900.cpp tms9900.h 99xxcore.h 9900stat.h
endif

CPU=$(strip $(findstring TMS9940@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/tms9900
CPUDEFS += -DHAS_TMS9940=1
CPUOBJS += $(OBJ)/cpu/tms9900/tms9900.o
$(OBJ)/cpu/tms9900/tms9900.o: tms9900.cpp tms9900.h 99xxcore.h 9900stat.h
endif

CPU=$(strip $(findstring TMS9980@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/tms9900
CPUDEFS += -DHAS_TMS9980=1
CPUOBJS += $(OBJ)/cpu/tms9900/tms9980a.o
$(OBJ)/cpu/tms9900/tms9980a.o: tms9980a.cpp tms9900.h 99xxcore.h 99xxstat.h
endif

CPU=$(strip $(findstring TMS9985@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/tms9900
CPUDEFS += -DHAS_TMS9985=1
CPUOBJS += $(OBJ)/cpu/tms9900/tms9980a.o
$(OBJ)/cpu/tms9900/tms9980a.o: tms9980a.cpp tms9900.h 99xxcore.h 99xxstat.h
endif

CPU=$(strip $(findstring TMS9989@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/tms9900
CPUDEFS += -DHAS_TMS9989=1
CPUOBJS += $(OBJ)/cpu/tms9900/tms9980a.o
$(OBJ)/cpu/tms9900/tms9980a.o: tms9980a.cpp tms9900.h 99xxcore.h 99xxstat.h
endif

CPU=$(strip $(findstring TMS9995@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/tms9900
CPUDEFS += -DHAS_TMS9995=1
CPUOBJS += $(OBJ)/cpu/tms9900/tms9995.o
$(OBJ)/cpu/tms9900/tms9995.o: tms9995.cpp tms9900.h 99xxcore.h 99xxstat.h
endif

CPU=$(strip $(findstring TMS99105A@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/tms9900
CPUDEFS += -DHAS_TMS99105A=1
CPUOBJS += $(OBJ)/cpu/tms9900/tms9995.o
$(OBJ)/cpu/tms9900/tms9995.o: tms9995.cpp tms9900.h 99xxcore.h 99xxstat.h
endif

CPU=$(strip $(findstring TMS99105A@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/tms9900
CPUDEFS += -DHAS_TMS99105A=1
CPUOBJS += $(OBJ)/cpu/tms9900/tms9995.o
$(OBJ)/cpu/tms9900/tms9995.o: tms9995.cpp tms9900.h 99xxcore.h 99xxstat.h
endif

CPU=$(strip $(findstring Z8000@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/z8000
CPUDEFS += -DHAS_Z8000=1
CPUOBJS += $(OBJ)/cpu/z8000/z8000.o
$(OBJ)/cpu/z8000/z8000.o: z8000.cpp z8000.h z8000cpu.h z8000dab.h z8000ops.cpp z8000tbl.cpp
endif

CPU=$(strip $(findstring TMS320C10@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/tms32010
CPUDEFS += -DHAS_TMS320C10=1
CPUOBJS += $(OBJ)/cpu/tms32010/tms32010.o
$(OBJ)/cpu/tms32010/tms32010.o: tms32010.cpp tms32010.h
endif

CPU=$(strip $(findstring CCPU@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/ccpu
CPUDEFS += -DHAS_CCPU=1
CPUOBJS += $(OBJ)/cpu/ccpu/ccpu.o $(OBJ)/vidhrdw/cinemat.o
$(OBJ)/cpu/ccpu/ccpu.o: ccpu.cpp ccpu.h ccputabl.cpp
endif

CPU=$(strip $(findstring ADSP2100@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/adsp2100
CPUDEFS += -DHAS_ADSP2100=1
CPUOBJS += $(OBJ)/cpu/adsp2100/adsp2100.o
$(OBJ)/cpu/adsp2100/adsp2100.o: adsp2100.cpp adsp2100.h 2100ops.cpp
endif

CPU=$(strip $(findstring ADSP2105@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/adsp2100
CPUDEFS += -DHAS_ADSP2105=1
CPUOBJS += $(OBJ)/cpu/adsp2100/adsp2100.o
$(OBJ)/cpu/adsp2100/adsp2100.o: adsp2100.cpp adsp2100.h 2100ops.cpp
endif

CPU=$(strip $(findstring PDP1@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/pdp1
CPUDEFS += -DHAS_PDP1=1
CPUOBJS += $(OBJ)/cpu/pdp1/pdp1.o
$(OBJ)/cpu/pdp1/pdp1.o: pdp1.cpp pdp1.h
endif

CPU=$(strip $(findstring MIPS@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/mips
CPUDEFS += -DHAS_MIPS=1
CPUOBJS += $(OBJ)/cpu/mips/mips.o
$(OBJ)/cpu/mips/mips.o: mips.cpp mips.h
endif

CPU=$(strip $(findstring SC61860@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/sc61860
CPUDEFS += -DHAS_SC61860=1
CPUOBJS += $(OBJ)/cpu/sc61860/sc61860.o
$(OBJ)/cpu/sc61860/sc61860.o: src/cpu/sc61860/sc61860.h \
	src/cpu/sc61860/sc.h src/cpu/sc61860/ops.cpp src/cpu/sc61860/table.cpp
endif

CPU=$(strip $(findstring ARM@,$(CPUS)))
ifneq ($(CPU),)
OBJDIRS += $(OBJ)/cpu/arm
CPUDEFS += -DHAS_ARM=1
CPUOBJS += $(OBJ)/cpu/arm/arm.o
$(OBJ)/cpu/arm/arm.o: src/cpu/arm/arm.h
endif
