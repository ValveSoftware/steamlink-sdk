# List of sound cores

# --------------
# FM sound cores
# --------------

# add AY8910
ifneq ($(strip $(findstring AY8910@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/ay8910.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ay8910.o
endif
endif
ifneq ($(strip $(findstring YM2203@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/ay8910.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ay8910.o
endif
endif
ifneq ($(strip $(findstring YM2608@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/ay8910.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ay8910.o
endif
endif
ifneq ($(strip $(findstring YM2610@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/ay8910.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ay8910.o
endif
endif
ifneq ($(strip $(findstring YM2610B@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/ay8910.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ay8910.o
endif
endif
ifneq ($(strip $(findstring YM2612@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/ay8910.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ay8910.o
endif
endif
ifneq ($(strip $(findstring YM3438@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/ay8910.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ay8910.o
endif
endif

# add interfaces

ifneq ($(strip $(findstring AY8910@,$(SOUNDS))),)
SOUNDDEFS += -DHAS_AY8910=1
endif

ifneq ($(strip $(findstring YM2203@,$(SOUNDS))),)
SOUNDDEFS += -DHAS_YM2203=1
ifeq ($(findstring $(OBJ)/sound/2203intf.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/2203intf.o
endif
endif

ifneq ($(strip $(findstring YM2151@,$(SOUNDS))),)
SOUNDDEFS += -DHAS_YM2151=1
ifeq ($(findstring $(OBJ)/sound/2151intf.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/2151intf.o
endif
endif

ifneq ($(strip $(findstring YM2151_ALT@,$(SOUNDS))),)
SOUNDDEFS += -DHAS_YM2151_ALT=1
ifeq ($(findstring $(OBJ)/sound/2151intf.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/2151intf.o
endif
ifeq ($(findstring $(OBJ)/sound/ym2151.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ym2151.o
endif
endif

ifneq ($(strip $(findstring YM2608@,$(SOUNDS))),)
SOUNDDEFS += -DHAS_YM2608=1
ifeq ($(findstring $(OBJ)/sound/2608intf.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/2608intf.o
endif
endif

ifneq ($(strip $(findstring YM2610@,$(SOUNDS))),)
SOUNDDEFS += -DHAS_YM2610=1
ifeq ($(findstring $(OBJ)/sound/2610intf.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/2610intf.o
endif
endif

ifneq ($(strip $(findstring YM2610B@,$(SOUNDS))),)
SOUNDDEFS += -DHAS_YM2610B=1
ifeq ($(findstring $(OBJ)/sound/2610intf.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/2610intf.o
endif
endif

ifneq ($(strip $(findstring YM2612@,$(SOUNDS))),)
SOUNDDEFS += -DHAS_YM2612=1
ifeq ($(findstring $(OBJ)/sound/2612intf.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/2612intf.o
endif
endif

ifneq ($(strip $(findstring YM3438@,$(SOUNDS))),)
SOUNDDEFS += -DHAS_YM3438=1
ifeq ($(findstring $(OBJ)/sound/2612intf.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/2612intf.o
endif
endif

# add FM and YMDELTAT
ifneq ($(strip $(findstring YM2203@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/fm.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/fm.o
endif
endif
ifneq ($(strip $(findstring YM2151@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/fm.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/fm.o
endif
endif
ifneq ($(strip $(findstring YM2151_ALT@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/fm.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/fm.o
endif
endif
ifneq ($(strip $(findstring YM2612@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/fm.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/fm.o
endif
endif
ifneq ($(strip $(findstring YM3438@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/fm.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/fm.o
endif
endif
ifneq ($(strip $(findstring YM2608@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/fm.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/fm.o
endif
ifeq ($(findstring $(OBJ)/sound/ymdeltat.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ymdeltat.o
endif
endif
ifneq ($(strip $(findstring YM2610@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/fm.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/fm.o
endif
ifeq ($(findstring $(OBJ)/sound/ymdeltat.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ymdeltat.o
endif
endif
ifneq ($(strip $(findstring YM2610B@,$(SOUNDS))),)
ifeq ($(findstring $(OBJ)/sound/fm.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/fm.o
endif
ifeq ($(findstring $(OBJ)/sound/ymdeltat.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ymdeltat.o
endif
endif

# -----------------
# FMOPL sound cores
# -----------------

ifneq ($(strip $(findstring YM2413@,$(SOUNDS))),)
SOUNDDEFS += -DHAS_YM2413=1
ifeq ($(findstring $(OBJ)/sound/3812intf.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/3812intf.o
endif
ifeq ($(findstring $(OBJ)/sound/ym2413.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ym2413.o
endif
ifeq ($(findstring $(OBJ)/sound/fmopl.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/fmopl.o
endif
endif

ifneq ($(strip $(findstring YM3812@,$(SOUNDS))),)
SOUNDDEFS += -DHAS_YM3812=1
ifeq ($(findstring $(OBJ)/sound/3812intf.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/3812intf.o
endif
ifeq ($(findstring $(OBJ)/sound/fmopl.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/fmopl.o
endif
endif

ifneq ($(strip $(findstring YM3526@,$(SOUNDS))),)
SOUNDDEFS += -DHAS_YM3526=1
ifeq ($(findstring $(OBJ)/sound/3812intf.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/3812intf.o
endif
ifeq ($(findstring $(OBJ)/sound/fmopl.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/fmopl.o
endif
endif

SOUND=$(strip $(findstring Y8950@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_Y8950=1
ifeq ($(findstring $(OBJ)/sound/3812intf.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/3812intf.o
endif
ifeq ($(findstring $(OBJ)/sound/fmopl.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/fmopl.o
endif
ifeq ($(findstring $(OBJ)/sound/ymdeltat.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/ymdeltat.o
endif
endif

# -----------------
# other sound cores
# -----------------

SOUND=$(strip $(findstring CUSTOM@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_CUSTOM=1
endif

SOUND=$(strip $(findstring YMZ280B@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_YMZ280B=1
SOUNDOBJS += $(OBJ)/sound/ymz280b.o
endif

SOUND=$(strip $(findstring POKEY@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_POKEY=1
SOUNDOBJS += $(OBJ)/sound/pokey.o
endif

SOUND=$(strip $(findstring QSOUND@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_QSOUND=1
SOUNDOBJS += $(OBJ)/sound/qsound.o
endif

SOUND=$(strip $(findstring SN76477@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_SN76477=1
SOUNDOBJS += $(OBJ)/sound/sn76477.o
endif

SOUND=$(strip $(findstring SN76496@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_SN76496=1
SOUNDOBJS += $(OBJ)/sound/sn76496.o
endif

SOUND=$(strip $(findstring TMS36XX@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_TMS36XX=1
SOUNDOBJS += $(OBJ)/sound/tms36xx.o
endif

SOUND=$(strip $(findstring TMS5220@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_TMS5220=1
$(OBJ)/sound/5220intf.o: src/sound/tms5220.cpp src/sound/5220intf.cpp
SOUNDOBJS += $(OBJ)/sound/5220intf.o
endif

SOUND=$(strip $(findstring SEGAPCM@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_SEGAPCM=1
SOUNDOBJS += $(OBJ)/sound/segapcm.o
endif

SOUND=$(strip $(findstring DISCRETE@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_DISCRETE=1
SOUNDOBJS += $(OBJ)/sound/discrete.o
endif

SOUND=$(strip $(findstring DAC@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_DAC=1
SOUNDOBJS += $(OBJ)/sound/dac.o
endif

SOUND=$(strip $(findstring ADPCM@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_ADPCM=1
ifeq ($(findstring $(OBJ)/sound/adpcm.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/adpcm.o
endif
endif

SOUND=$(strip $(findstring OKIM6295@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_OKIM6295=1
ifeq ($(findstring $(OBJ)/sound/adpcm.o,$(SOUNDOBJS)),)
SOUNDOBJS += $(OBJ)/sound/adpcm.o
endif
endif

SOUND=$(strip $(findstring SAMPLES@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_SAMPLES=1
SOUNDOBJS += $(OBJ)/sound/samples.o
endif

SOUND=$(strip $(findstring ASTROCADE@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_ASTROCADE=1
SOUNDOBJS += $(OBJ)/sound/astrocde.o
endif

SOUND=$(strip $(findstring TIA@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_TIA=1
$(OBJ)/sound/tiaintf.o: src/sound/tiasound.cpp src/sound/tiaintf.cpp
SOUNDOBJS += $(OBJ)/sound/tiaintf.o
endif

SOUND=$(strip $(findstring NES@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_NES=1
SOUNDOBJS += $(OBJ)/sound/nes_apu.o
endif

SOUND=$(strip $(findstring NAMCO@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_NAMCO=1
SOUNDOBJS += $(OBJ)/sound/namco.o
endif

SOUND=$(strip $(findstring VLM5030@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_VLM5030=1
SOUNDOBJS += $(OBJ)/sound/vlm5030.o
endif

SOUND=$(strip $(findstring MSM5205@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_MSM5205=1
SOUNDOBJS += $(OBJ)/sound/msm5205.o
endif

SOUND=$(strip $(findstring UPD7759@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_UPD7759=1
SOUNDOBJS += $(OBJ)/sound/upd7759.o
endif

SOUND=$(strip $(findstring HC55516@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_HC55516=1
SOUNDOBJS += $(OBJ)/sound/hc55516.o
endif

SOUND=$(strip $(findstring K005289@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_K005289=1
SOUNDOBJS += $(OBJ)/sound/k005289.o
endif

SOUND=$(strip $(findstring K007232@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_K007232=1
SOUNDOBJS += $(OBJ)/sound/k007232.o
endif

SOUND=$(strip $(findstring K051649@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_K051649=1
SOUNDOBJS += $(OBJ)/sound/k051649.o
endif

SOUND=$(strip $(findstring K053260@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_K053260=1
SOUNDOBJS += $(OBJ)/sound/k053260.o
endif

SOUND=$(strip $(findstring K054539@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_K054539=1
SOUNDOBJS += $(OBJ)/sound/k054539.o
endif

SOUND=$(strip $(findstring RF5C68@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_RF5C68=1
SOUNDOBJS += $(OBJ)/sound/rf5c68.o
endif

SOUND=$(strip $(findstring CEM3394@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_CEM3394=1
SOUNDOBJS += $(OBJ)/sound/cem3394.o
endif

SOUND=$(strip $(findstring C140@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_C140=1
SOUNDOBJS += $(OBJ)/sound/c140.o
endif

SOUND=$(strip $(findstring SPEAKER@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_SPEAKER=1
SOUNDOBJS += $(OBJ)/sound/speaker.o
endif

SOUND=$(strip $(findstring WAVE@,$(SOUNDS)))
ifneq ($(SOUND),)
SOUNDDEFS += -DHAS_WAVE=1
SOUNDOBJS += $(OBJ)/sound/wave.o
endif
