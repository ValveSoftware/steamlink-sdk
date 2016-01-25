# uncomment the following lines to include a CPU core
CPUS+=Z80@
#CPUS+=Z80GB@
CPUS+=8080@
CPUS+=8085A@
CPUS+=M6502@
CPUS+=M65C02@
#CPUS+=M65SC02@
#CPUS+=M65CE02@
#CPUS+=M6509@
CPUS+=M6510@
#CPUS+=M6510T@
#CPUS+=M7501@
#CPUS+=M8502@
#CPUS+=M4510@
CPUS+=N2A03@
CPUS+=H6280@
CPUS+=I86@
#CPUS+=I88@
CPUS+=I186@
#CPUS+=I188@
#CPUS+=I288@
CPUS+=V20@
CPUS+=V30@
CPUS+=V33@
CPUS+=I8035@
CPUS+=I8039@
CPUS+=I8048@
CPUS+=N7751@
CPUS+=M6800@
CPUS+=M6801@
CPUS+=M6802@
CPUS+=M6803@
CPUS+=M6808@
CPUS+=HD63701@
CPUS+=NSC8105@
CPUS+=M6805@
CPUS+=M68705@
CPUS+=HD63705@
CPUS+=HD6309@
CPUS+=M6809@
CPUS+=KONAMI@
CPUS+=M68000@
CPUS+=M68010@
CPUS+=M68EC020@
CPUS+=M68020@
CPUS+=T11@
CPUS+=S2650@
CPUS+=TMS34010@
#CPUS+=TMS9900@
#CPUS+=TMS9940@
CPUS+=TMS9980@
#CPUS+=TMS9985@
#CPUS+=TMS9989@
#CPUS+=TMS9995@
#CPUS+=TMS99105A@
#CPUS+=TMS99110A@
CPUS+=Z8000@
CPUS+=TMS320C10@
CPUS+=CCPU@
#CPUS+=PDP1@
CPUS+=ADSP2100@
CPUS+=ADSP2105@
CPUS+=MIPS@
#CPUS+=SC61860@
#CPUS+=ARM@

# uncomment the following lines to include a sound core
SOUNDS+=CUSTOM@
SOUNDS+=SAMPLES@
SOUNDS+=DAC@
SOUNDS+=DISCRETE@
SOUNDS+=AY8910@
SOUNDS+=YM2203@
# enable only one of the following two
SOUNDS+=YM2151@
#SOUNDS+=YM2151_ALT@
SOUNDS+=YM2608@
SOUNDS+=YM2610@
SOUNDS+=YM2610B@
SOUNDS+=YM2612@
SOUNDS+=YM3438@
SOUNDS+=YM2413@
SOUNDS+=YM3812@
SOUNDS+=YMZ280B@
SOUNDS+=YM3526@
SOUNDS+=Y8950@
SOUNDS+=SN76477@
SOUNDS+=SN76496@
SOUNDS+=POKEY@
#SOUNDS+=TIA@
SOUNDS+=NES@
SOUNDS+=ASTROCADE@
SOUNDS+=NAMCO@
SOUNDS+=TMS36XX@
SOUNDS+=TMS5220@
SOUNDS+=VLM5030@
SOUNDS+=ADPCM@
SOUNDS+=OKIM6295@
SOUNDS+=MSM5205@
SOUNDS+=UPD7759@
SOUNDS+=HC55516@
SOUNDS+=K005289@
SOUNDS+=K007232@
SOUNDS+=K051649@
SOUNDS+=K053260@
SOUNDS+=K054539@
SOUNDS+=SEGAPCM@
SOUNDS+=RF5C68@
SOUNDS+=CEM3394@
SOUNDS+=C140@
SOUNDS+=QSOUND@
#SOUNDS+=SPEAKER@
#SOUNDS+=WAVE@

$(OBJ)/drivers/pacman.o: src/machine/pacplus.cpp src/machine/theglob.cpp src/drivers/pacman.cpp
$(OBJ)/drivers/jrpacman.o: src/drivers/jrpacman.cpp src/vidhrdw/jrpacman.cpp
$(OBJ)/drivers/pengo.o: src/vidhrdw/pengo.cpp src/drivers/pengo.cpp
OBJ_PACMAN = $(OBJ)/drivers/pacman.o $(OBJ)/drivers/jrpacman.o $(OBJ)/drivers/pengo.o

$(OBJ)/drivers/cclimber.o: src/vidhrdw/cclimber.cpp src/sndhrdw/cclimber.cpp src/drivers/cclimber.cpp
$(OBJ)/drivers/seicross.o: src/vidhrdw/seicross.cpp src/drivers/seicross.cpp
$(OBJ)/drivers/wiping.o: src/vidhrdw/wiping.cpp src/sndhrdw/wiping.cpp src/drivers/wiping.cpp
$(OBJ)/drivers/cop01.o: src/vidhrdw/cop01.cpp src/drivers/cop01.cpp
$(OBJ)/drivers/terracre.o: src/vidhrdw/terracre.cpp src/drivers/terracre.cpp
$(OBJ)/drivers/galivan.o: src/vidhrdw/galivan.cpp src/drivers/galivan.cpp
$(OBJ)/drivers/armedf.o: src/vidhrdw/armedf.cpp src/drivers/armedf.cpp
OBJ_NICHIBUT = $(OBJ)/drivers/cclimber.o $(OBJ)/drivers/yamato.o $(OBJ)/drivers/seicross.o $(OBJ)/drivers/wiping.o \
    $(OBJ)/drivers/cop01.o $(OBJ)/drivers/terracre.o $(OBJ)/drivers/galivan.o $(OBJ)/drivers/armedf.o

$(OBJ)/drivers/phoenix.o: src/vidhrdw/phoenix.cpp src/sndhrdw/phoenix.cpp src/drivers/phoenix.cpp
$(OBJ)/drivers/naughtyb.o: src/vidhrdw/naughtyb.cpp src/drivers/naughtyb.cpp
OBJ_PHOENIX = $(OBJ)/drivers/safarir.o $(OBJ)/drivers/phoenix.o $(OBJ)/sndhrdw/pleiads.o $(OBJ)/drivers/naughtyb.o

$(OBJ)/drivers/geebee.o: src/machine/geebee.cpp src/vidhrdw/geebee.cpp src/sndhrdw/geebee.cpp src/drivers/geebee.cpp
$(OBJ)/drivers/warpwarp.o: src/vidhrdw/warpwarp.cpp src/sndhrdw/warpwarp.cpp src/drivers/warpwarp.cpp
$(OBJ)/drivers/tankbatt.o: src/vidhrdw/tankbatt.cpp src/drivers/tankbatt.cpp
$(OBJ)/drivers/galaxian.o: src/vidhrdw/galaxian.cpp src/sndhrdw/galaxian.cpp src/drivers/galaxian.cpp
$(OBJ)/drivers/rallyx.o: src/vidhrdw/rallyx.cpp src/drivers/rallyx.cpp
$(OBJ)/drivers/bosco.o:	src/machine/bosco.cpp src/sndhrdw/bosco.cpp src/vidhrdw/bosco.cpp src/drivers/bosco.cpp
$(OBJ)/drivers/galaga.o: src/machine/galaga.cpp src/vidhrdw/galaga.cpp src/drivers/galaga.cpp
$(OBJ)/drivers/digdug.o: src/machine/digdug.cpp src/vidhrdw/digdug.cpp src/drivers/digdug.cpp
$(OBJ)/drivers/xevious.o: src/vidhrdw/xevious.cpp src/machine/xevious.cpp src/drivers/xevious.cpp
$(OBJ)/drivers/superpac.o: src/machine/superpac.cpp src/vidhrdw/superpac.cpp src/drivers/superpac.cpp
$(OBJ)/drivers/phozon.o: src/machine/phozon.cpp src/vidhrdw/phozon.cpp src/drivers/phozon.cpp
$(OBJ)/drivers/mappy.o:	src/machine/mappy.cpp src/vidhrdw/mappy.cpp src/drivers/mappy.cpp
$(OBJ)/drivers/grobda.o: src/machine/grobda.cpp src/vidhrdw/grobda.cpp src/drivers/grobda.cpp
$(OBJ)/drivers/gaplus.o: src/machine/gaplus.cpp src/vidhrdw/gaplus.cpp src/drivers/gaplus.cpp
$(OBJ)/drivers/toypop.o: src/machine/toypop.cpp src/vidhrdw/toypop.cpp src/drivers/toypop.cpp
$(OBJ)/drivers/polepos.o: src/machine/polepos.cpp src/vidhrdw/polepos.cpp src/sndhrdw/polepos.cpp src/drivers/polepos.cpp
$(OBJ)/drivers/pacland.o: src/vidhrdw/pacland.cpp src/drivers/pacland.cpp
$(OBJ)/drivers/skykid.o: src/vidhrdw/skykid.cpp src/drivers/skykid.cpp
$(OBJ)/drivers/baraduke.o: src/vidhrdw/baraduke.cpp src/drivers/baraduke.cpp
$(OBJ)/drivers/namcos86.o: src/vidhrdw/namcos86.cpp src/drivers/namcos86.cpp
$(OBJ)/drivers/namcos1.o: src/machine/namcos1.cpp src/vidhrdw/namcos1.cpp src/drivers/namcos1.cpp
$(OBJ)/drivers/namcos2.o: src/machine/namcos2.cpp src/vidhrdw/namcos2.cpp src/drivers/namcos2.cpp
OBJ_NAMCO = $(OBJ)/drivers/geebee.o $(OBJ)/drivers/warpwarp.o $(OBJ)/drivers/tankbatt.o $(OBJ)/drivers/galaxian.o \
	$(OBJ)/drivers/rallyx.o $(OBJ)/drivers/locomotn.o $(OBJ)/drivers/bosco.o $(OBJ)/drivers/galaga.o $(OBJ)/drivers/digdug.o \
	$(OBJ)/drivers/xevious.o $(OBJ)/drivers/superpac.o $(OBJ)/drivers/phozon.o $(OBJ)/drivers/mappy.o $(OBJ)/drivers/grobda.o \
	$(OBJ)/drivers/gaplus.o $(OBJ)/drivers/toypop.o $(OBJ)/drivers/polepos.o $(OBJ)/drivers/pacland.o $(OBJ)/drivers/skykid.o \
	$(OBJ)/drivers/baraduke.o $(OBJ)/drivers/namcos86.o $(OBJ)/drivers/namcos1.o $(OBJ)/drivers/namcos2.o

$(OBJ)/drivers/cosmic.o: src/vidhrdw/cosmic.cpp src/drivers/cosmic.cpp
$(OBJ)/drivers/cheekyms.o: src/vidhrdw/cheekyms.cpp src/drivers/cheekyms.cpp
$(OBJ)/drivers/ladybug.o: src/vidhrdw/ladybug.cpp src/drivers/ladybug.cpp
$(OBJ)/drivers/mrdo.o: src/vidhrdw/mrdo.cpp src/drivers/mrdo.cpp
$(OBJ)/drivers/docastle.o: src/machine/docastle.cpp src/vidhrdw/docastle.cpp src/drivers/docastle.cpp
OBJ_UNIVERS = $(OBJ)/drivers/cosmic.o $(OBJ)/drivers/cheekyms.o $(OBJ)/drivers/ladybug.o $(OBJ)/drivers/mrdo.o $(OBJ)/drivers/docastle.o

$(OBJ)/drivers/dkong.o: src/vidhrdw/dkong.cpp src/sndhrdw/dkong.cpp src/drivers/dkong.cpp
$(OBJ)/drivers/mario.o: src/vidhrdw/mario.cpp src/sndhrdw/mario.cpp src/drivers/mario.cpp
$(OBJ)/drivers/popeye.o: src/vidhrdw/popeye.cpp src/drivers/popeye.cpp
$(OBJ)/drivers/punchout.o: src/vidhrdw/punchout.cpp src/drivers/punchout.cpp
OBJ_NINTENDO = $(OBJ)/drivers/dkong.o $(OBJ)/drivers/mario.o $(OBJ)/drivers/popeye.o $(OBJ)/drivers/punchout.o

$(OBJ)/drivers/8080bw.o: src/machine/8080bw.cpp src/machine/74123.cpp src/vidhrdw/8080bw.cpp src/sndhrdw/8080bw.cpp src/drivers/8080bw.cpp
$(OBJ)/drivers/m79amb.o: src/vidhrdw/m79amb.cpp src/drivers/m79amb.cpp
$(OBJ)/drivers/z80bw.o: src/sndhrdw/z80bw.cpp src/drivers/z80bw.cpp
OBJ_MIDW8080 = $(OBJ)/drivers/8080bw.o $(OBJ)/drivers/m79amb.o $(OBJ)/drivers/z80bw.o

$(OBJ)/drivers/lazercmd.o: src/drivers/lazercmd.cpp src/vidhrdw/lazercmd.cpp
$(OBJ)/drivers/meadows.o: src/drivers/meadows.cpp src/sndhrdw/meadows.cpp src/vidhrdw/meadows.cpp
OBJ_MEADOWS = $(OBJ)/drivers/lazercmd.o $(OBJ)/drivers/meadows.o

$(OBJ)/drivers/astrocde.o: src/machine/astrocde.cpp src/vidhrdw/astrocde.cpp src/sndhrdw/astrocde.cpp src/drivers/astrocde.cpp
$(OBJ)/drivers/mcr68.o: src/vidhrdw/mcr68.cpp src/drivers/mcr68.cpp
$(OBJ)/drivers/balsente.o: src/vidhrdw/balsente.cpp src/drivers/balsente.cpp
OBJ_MIDWAY = $(OBJ)/drivers/astrocde.o $(OBJ)/sndhrdw/gorf.o $(OBJ)/machine/mcr.o $(OBJ)/sndhrdw/mcr.o $(OBJ)/vidhrdw/mcr12.o $(OBJ)/vidhrdw/mcr3.o \
	$(OBJ)/drivers/mcr1.o $(OBJ)/drivers/mcr2.o $(OBJ)/drivers/mcr3.o $(OBJ)/drivers/mcr68.o $(OBJ)/drivers/balsente.o

$(OBJ)/drivers/skychut.o: src/vidhrdw/skychut.cpp src/drivers/skychut.cpp
$(OBJ)/drivers/mpatrol.o: src/vidhrdw/mpatrol.cpp src/drivers/mpatrol.cpp
$(OBJ)/drivers/troangel.o: src/vidhrdw/troangel.cpp src/drivers/troangel.cpp
$(OBJ)/drivers/yard.o: src/vidhrdw/yard.cpp src/drivers/yard.cpp
$(OBJ)/drivers/travrusa.o: src/vidhrdw/travrusa.cpp src/drivers/travrusa.cpp
$(OBJ)/drivers/m62.o: src/vidhrdw/m62.cpp src/drivers/m62.cpp
$(OBJ)/drivers/vigilant.o: src/vidhrdw/vigilant.cpp src/drivers/vigilant.cpp
$(OBJ)/vidhrdw/m72.o: src/vidhrdw/m72.cpp src/sndhrdw/m72.cpp src/drivers/m72.cpp
$(OBJ)/drivers/shisen.o: src/vidhrdw/shisen.cpp src/drivers/shisen.cpp
$(OBJ)/drivers/m92.o: src/vidhrdw/m92.cpp src/drivers/m92.cpp
$(OBJ)/drivers/m107.o: src/vidhrdw/m107.cpp src/drivers/m107.cpp
OBJ_IREM = $(OBJ)/drivers/skychut.o $(OBJ)/drivers/olibochu.o $(OBJ)/sndhrdw/irem.o $(OBJ)/drivers/mpatrol.o $(OBJ)/drivers/troangel.o \
	$(OBJ)/drivers/yard.o $(OBJ)/drivers/travrusa.o $(OBJ)/drivers/m62.o $(OBJ)/drivers/vigilant.o $(OBJ)/drivers/m72.o $(OBJ)/drivers/shisen.o \
	$(OBJ)/drivers/m92.o $(OBJ)/drivers/m97.o $(OBJ)/drivers/m107.o

$(OBJ)/drivers/gottlieb.o: src/vidhrdw/gottlieb.cpp src/sndhrdw/gottlieb.cpp src/drivers/gottlieb.cpp
OBJ_GOTTLIEB = $(OBJ)/drivers/gottlieb.o

$(OBJ)/drivers/crbaloon.o: src/vidhrdw/crbaloon.cpp src/drivers/crbaloon.cpp
$(OBJ)/drivers/qix.o: src/machine/qix.cpp src/vidhrdw/qix.cpp src/drivers/qix.cpp
$(OBJ)/drivers/taitosj.o: src/machine/taitosj.cpp src/vidhrdw/taitosj.cpp src/drivers/taitosj.cpp
$(OBJ)/drivers/bking2.o: src/vidhrdw/bking2.cpp src/drivers/bking2.cpp
$(OBJ)/drivers/gsword.o: src/vidhrdw/gsword.cpp src/machine/tait8741.cpp src/drivers/gsword.cpp
$(OBJ)/drivers/retofinv.o: src/vidhrdw/retofinv.cpp src/drivers/retofinv.cpp
$(OBJ)/drivers/tsamurai.o: src/vidhrdw/tsamurai.cpp src/drivers/tsamurai.cpp
$(OBJ)/drivers/flstory.o: src/machine/flstory.cpp src/vidhrdw/flstory.cpp src/drivers/flstory.cpp
$(OBJ)/drivers/gladiatr.o: src/vidhrdw/gladiatr.cpp src/drivers/gladiatr.cpp
$(OBJ)/drivers/lsasquad.o: src/machine/lsasquad.cpp src/vidhrdw/lsasquad.cpp src/drivers/lsasquad.cpp
$(OBJ)/drivers/bublbobl.o: src/machine/bublbobl.cpp src/vidhrdw/bublbobl.cpp src/drivers/bublbobl.cpp
$(OBJ)/drivers/mexico86.o: src/machine/mexico86.cpp src/vidhrdw/mexico86.cpp src/drivers/mexico86.cpp
$(OBJ)/drivers/rastan.o: src/vidhrdw/rastan.cpp src/sndhrdw/rastan.cpp src/drivers/rastan.cpp
$(OBJ)/drivers/rainbow.o: src/machine/rainbow.cpp src/drivers/rainbow.cpp
$(OBJ)/drivers/arkanoid.o: src/machine/arkanoid.cpp src/vidhrdw/arkanoid.cpp src/drivers/arkanoid.cpp
$(OBJ)/drivers/superqix.o: src/vidhrdw/superqix.cpp src/drivers/superqix.cpp
$(OBJ)/drivers/superman.o: src/vidhrdw/superman.cpp src/drivers/superman.cpp
$(OBJ)/drivers/minivadr.o: src/vidhrdw/minivadr.cpp src/drivers/minivadr.cpp
$(OBJ)/drivers/tnzs.o: src/machine/tnzs.cpp src/vidhrdw/tnzs.cpp src/drivers/tnzs.cpp
$(OBJ)/drivers/lkage.o: src/machine/lkage.cpp src/vidhrdw/lkage.cpp src/drivers/lkage.cpp
$(OBJ)/drivers/taito_l.o: src/vidhrdw/taito_l.cpp src/drivers/taito_l.cpp src/sndhrdw/taitosnd.cpp
$(OBJ)/drivers/taito_b.o: src/vidhrdw/taito_b.cpp src/drivers/taito_b.cpp
$(OBJ)/drivers/taito_f2.o: src/vidhrdw/taito_f2.cpp src/drivers/taito_f2.cpp
OBJ_TAITO = $(OBJ)/drivers/crbaloon.o $(OBJ)/drivers/qix.o $(OBJ)/drivers/taitosj.o $(OBJ)/drivers/bking2.o $(OBJ)/drivers/gsword.o \
    $(OBJ)/drivers/retofinv.o $(OBJ)/drivers/tsamurai.o $(OBJ)/drivers/flstory.o $(OBJ)/drivers/gladiatr.o $(OBJ)/drivers/lsasquad.o \
	$(OBJ)/drivers/bublbobl.o $(OBJ)/drivers/mexico86.o $(OBJ)/drivers/rastan.o $(OBJ)/drivers/rainbow.o $(OBJ)/drivers/arkanoid.o \
	$(OBJ)/drivers/superqix.o $(OBJ)/drivers/superman.o $(OBJ)/drivers/minivadr.o $(OBJ)/drivers/tnzs.o $(OBJ)/drivers/lkage.o \
	$(OBJ)/drivers/taito_l.o $(OBJ)/drivers/taito_b.o $(OBJ)/vidhrdw/taitoic.o $(OBJ)/drivers/taito_f2.o $(OBJ)/machine/cchip.o

$(OBJ)/drivers/slapfght.o: src/machine/slapfght.cpp src/vidhrdw/slapfght.cpp src/drivers/slapfght.cpp
$(OBJ)/drivers/twincobr.o: src/machine/twincobr.cpp src/vidhrdw/twincobr.cpp src/drivers/twincobr.cpp
$(OBJ)/drivers/toaplan1.o: src/machine/toaplan1.cpp src/vidhrdw/toaplan1.cpp src/drivers/toaplan1.cpp
$(OBJ)/drivers/snowbros.o: src/vidhrdw/snowbros.cpp src/drivers/snowbros.cpp
$(OBJ)/drivers/toaplan2.o: src/vidhrdw/toaplan2.cpp src/drivers/toaplan2.cpp
OBJ_TOAPLAN = $(OBJ)/drivers/slapfght.o $(OBJ)/drivers/twincobr.o $(OBJ)/drivers/wardner.o $(OBJ)/drivers/toaplan1.o \
    $(OBJ)/drivers/snowbros.o $(OBJ)/drivers/toaplan2.o

$(OBJ)/drivers/cave.o: src/vidhrdw/cave.cpp src/drivers/cave.cpp
OBJ_CAVE = $(OBJ)/drivers/cave.o

$(OBJ)/drivers/kyugo.o: src/vidhrdw/kyugo.cpp src/drivers/kyugo.cpp
OBJ_KYUGO = $(OBJ)/drivers/kyugo.o

$(OBJ)/drivers/williams.o: src/machine/williams.cpp src/vidhrdw/williams.cpp src/sndhrdw/williams.cpp src/drivers/williams.cpp
OBJ_WILLIAMS = $(OBJ)/drivers/williams.o

$(OBJ)/drivers/vulgus.o: src/vidhrdw/vulgus.cpp src/drivers/vulgus.cpp
$(OBJ)/drivers/sonson.o: src/vidhrdw/sonson.cpp src/drivers/sonson.cpp
$(OBJ)/drivers/higemaru.o: src/vidhrdw/higemaru.cpp src/drivers/higemaru.cpp
$(OBJ)/drivers/1942.o: src/vidhrdw/1942.cpp src/drivers/1942.cpp
$(OBJ)/drivers/exedexes.o: src/vidhrdw/exedexes.cpp src/drivers/exedexes.cpp
$(OBJ)/drivers/commando.o: src/vidhrdw/commando.cpp src/drivers/commando.cpp
$(OBJ)/drivers/gng.o: src/vidhrdw/gng.cpp src/drivers/gng.cpp
$(OBJ)/drivers/gunsmoke.o: src/vidhrdw/gunsmoke.cpp src/drivers/gunsmoke.cpp
$(OBJ)/drivers/srumbler.o: src/vidhrdw/srumbler.cpp src/drivers/srumbler.cpp
$(OBJ)/drivers/lwings.o: src/machine/lwings.cpp src/vidhrdw/lwings.cpp src/drivers/lwings.cpp
$(OBJ)/drivers/sidearms.o: src/vidhrdw/sidearms.cpp src/drivers/sidearms.cpp
$(OBJ)/drivers/bionicc.o: src/vidhrdw/bionicc.cpp src/drivers/bionicc.cpp
$(OBJ)/drivers/1943.o: src/vidhrdw/1943.cpp src/drivers/1943.cpp
$(OBJ)/drivers/blktiger.o: src/vidhrdw/blktiger.cpp src/drivers/blktiger.cpp
$(OBJ)/drivers/tigeroad.o: src/vidhrdw/tigeroad.cpp src/drivers/tigeroad.cpp
$(OBJ)/drivers/lastduel.o: src/vidhrdw/lastduel.cpp src/drivers/lastduel.cpp
$(OBJ)/drivers/sf1.o: src/vidhrdw/sf1.cpp src/drivers/sf1.cpp
$(OBJ)/drivers/mitchell.o: src/vidhrdw/mitchell.cpp src/drivers/mitchell.cpp
$(OBJ)/drivers/cbasebal.o: src/vidhrdw/cbasebal.cpp src/drivers/cbasebal.cpp
$(OBJ)/drivers/cps1.o: src/vidhrdw/cps1.cpp src/drivers/cps1.cpp
OBJ_CAPCOM = $(OBJ)/drivers/vulgus.o $(OBJ)/drivers/sonson.o $(OBJ)/drivers/higemaru.o $(OBJ)/drivers/1942.o $(OBJ)/drivers/exedexes.o \
	$(OBJ)/drivers/commando.o $(OBJ)/drivers/gng.o $(OBJ)/drivers/gunsmoke.o $(OBJ)/drivers/srumbler.o $(OBJ)/drivers/lwings.o \
	$(OBJ)/drivers/sidearms.o $(OBJ)/drivers/bionicc.o $(OBJ)/drivers/1943.o $(OBJ)/drivers/blktiger.o $(OBJ)/drivers/tigeroad.o \
	$(OBJ)/drivers/lastduel.o $(OBJ)/drivers/sf1.o $(OBJ)/machine/kabuki.o $(OBJ)/drivers/mitchell.o $(OBJ)/drivers/cbasebal.o \
	$(OBJ)/drivers/cps1.o $(OBJ)/drivers/zn.o

$(OBJ)/drivers/capbowl.o: src/machine/capbowl.cpp src/vidhrdw/capbowl.cpp src/vidhrdw/tms34061.cpp src/drivers/capbowl.cpp
OBJ_CAPBOWL = $(OBJ)/drivers/capbowl.o

$(OBJ)/drivers/blockade.o: src/vidhrdw/blockade.cpp src/drivers/blockade.cpp
OBJ_GREMLIN = $(OBJ)/drivers/blockade.o

$(OBJ)/drivers/vicdual.o: src/vidhrdw/vicdual.cpp src/drivers/vicdual.cpp
OBJ_VICDUAL = $(OBJ)/sndhrdw/carnival.o $(OBJ)/sndhrdw/depthch.o $(OBJ)/sndhrdw/invinco.o $(OBJ)/sndhrdw/pulsar.o $(OBJ)/drivers/vicdual.o

$(OBJ)/drivers/sega.o: src/vidhrdw/sega.cpp src/sndhrdw/sega.cpp src/machine/sega.cpp src/drivers/sega.cpp
$(OBJ)/drivers/segar.o: src/vidhrdw/segar.cpp src/sndhrdw/segar.cpp src/machine/segar.cpp src/drivers/segar.cpp
$(OBJ)/drivers/zaxxon.o: src/vidhrdw/zaxxon.cpp src/sndhrdw/zaxxon.cpp src/drivers/zaxxon.cpp
$(OBJ)/drivers/turbo.o: src/machine/turbo.cpp src/vidhrdw/turbo.cpp src/drivers/turbo.cpp
$(OBJ)/drivers/suprloco.o: src/vidhrdw/suprloco.cpp src/drivers/suprloco.cpp
$(OBJ)/drivers/appoooh.o: src/vidhrdw/appoooh.cpp src/drivers/appoooh.cpp
$(OBJ)/drivers/bankp.o: src/vidhrdw/bankp.cpp src/drivers/bankp.cpp
$(OBJ)/drivers/dotrikun.o: src/vidhrdw/dotrikun.cpp src/drivers/dotrikun.cpp
$(OBJ)/drivers/system1.o: src/vidhrdw/system1.cpp src/drivers/system1.cpp
$(OBJ)/drivers/system16.o: src/machine/system16.cpp src/vidhrdw/system16.cpp src/sndhrdw/system16.cpp src/drivers/system16.cpp
OBJ_SEGA = $(OBJ)/machine/segacrpt.o $(OBJ)/drivers/sega.o $(OBJ)/drivers/segar.o $(OBJ)/drivers/zaxxon.o $(OBJ)/drivers/congo.o \
	$(OBJ)/drivers/turbo.o $(OBJ)/drivers/kopunch.o $(OBJ)/drivers/suprloco.o $(OBJ)/drivers/appoooh.o $(OBJ)/drivers/bankp.o \
	$(OBJ)/drivers/dotrikun.o $(OBJ)/drivers/system1.o $(OBJ)/drivers/system16.o

$(OBJ)/drivers/deniam.o: src/vidhrdw/deniam.cpp src/drivers/deniam.cpp
OBJ_DENIAM = $(OBJ)/drivers/deniam.o

$(OBJ)/drivers/btime.o: src/machine/btime.cpp src/vidhrdw/btime.cpp src/drivers/btime.cpp
$(OBJ)/drivers/astrof.o: src/vidhrdw/astrof.cpp src/sndhrdw/astrof.cpp src/drivers/astrof.cpp
$(OBJ)/drivers/kchamp.o: src/vidhrdw/kchamp.cpp src/drivers/kchamp.cpp
$(OBJ)/drivers/firetrap.o: src/vidhrdw/firetrap.cpp src/drivers/firetrap.cpp
$(OBJ)/drivers/brkthru.o: src/vidhrdw/brkthru.cpp src/drivers/brkthru.cpp
$(OBJ)/drivers/shootout.o: src/vidhrdw/shootout.cpp src/drivers/shootout.cpp
$(OBJ)/drivers/sidepckt.o: src/vidhrdw/sidepckt.cpp src/drivers/sidepckt.cpp
$(OBJ)/drivers/exprraid.o: src/vidhrdw/exprraid.cpp src/drivers/exprraid.cpp
$(OBJ)/drivers/pcktgal.o: src/vidhrdw/pcktgal.cpp src/drivers/pcktgal.cpp
$(OBJ)/drivers/battlera.o: src/vidhrdw/battlera.cpp src/drivers/battlera.cpp
$(OBJ)/drivers/actfancr.o: src/vidhrdw/actfancr.cpp src/drivers/actfancr.cpp
$(OBJ)/drivers/dec8.o: src/vidhrdw/dec8.cpp src/drivers/dec8.cpp
$(OBJ)/drivers/karnov.o: src/vidhrdw/karnov.cpp src/drivers/karnov.cpp
$(OBJ)/drivers/dec0.o: src/machine/dec0.cpp src/vidhrdw/dec0.cpp src/drivers/dec0.cpp
$(OBJ)/drivers/stadhero.o: src/vidhrdw/stadhero.cpp src/drivers/stadhero.cpp
$(OBJ)/drivers/madmotor.o: src/vidhrdw/madmotor.cpp src/drivers/madmotor.cpp
$(OBJ)/drivers/vaportra.o: src/vidhrdw/vaportra.cpp src/drivers/vaportra.cpp
$(OBJ)/drivers/cbuster.o: src/vidhrdw/cbuster.cpp src/drivers/cbuster.cpp
$(OBJ)/drivers/darkseal.o: src/vidhrdw/darkseal.cpp src/drivers/darkseal.cpp
$(OBJ)/drivers/supbtime.o: src/vidhrdw/supbtime.cpp src/drivers/supbtime.cpp
$(OBJ)/drivers/cninja.o: src/vidhrdw/cninja.cpp src/drivers/cninja.cpp
$(OBJ)/drivers/tumblep.o: src/vidhrdw/tumblep.cpp src/drivers/tumblep.cpp
$(OBJ)/drivers/funkyjet.o: src/vidhrdw/funkyjet.cpp src/drivers/funkyjet.cpp
OBJ_DATAEAST = $(OBJ)/drivers/btime.o $(OBJ)/drivers/astrof.o $(OBJ)/drivers/kchamp.o $(OBJ)/drivers/firetrap.o $(OBJ)/drivers/brkthru.o \
	$(OBJ)/drivers/shootout.o $(OBJ)/drivers/sidepckt.o $(OBJ)/drivers/exprraid.o $(OBJ)/drivers/pcktgal.o $(OBJ)/drivers/battlera.o \
	$(OBJ)/drivers/actfancr.o $(OBJ)/drivers/dec8.o $(OBJ)/drivers/karnov.o $(OBJ)/drivers/dec0.o $(OBJ)/drivers/stadhero.o \
	$(OBJ)/drivers/madmotor.o $(OBJ)/drivers/vaportra.o $(OBJ)/drivers/cbuster.o $(OBJ)/drivers/darkseal.o $(OBJ)/drivers/supbtime.o \
	$(OBJ)/drivers/cninja.o $(OBJ)/drivers/tumblep.o $(OBJ)/drivers/funkyjet.o

$(OBJ)/drivers/senjyo.o: src/sndhrdw/senjyo.cpp src/vidhrdw/senjyo.cpp src/drivers/senjyo.cpp
$(OBJ)/drivers/bombjack.o: src/vidhrdw/bombjack.cpp src/drivers/bombjack.cpp
$(OBJ)/drivers/pbaction.o: src/vidhrdw/pbaction.cpp src/drivers/pbaction.cpp
$(OBJ)/drivers/tehkanwc.o: src/vidhrdw/tehkanwc.cpp src/drivers/tehkanwc.cpp
$(OBJ)/drivers/solomon.o: src/vidhrdw/solomon.cpp src/drivers/solomon.cpp
$(OBJ)/drivers/tecmo.o: src/vidhrdw/tecmo.cpp src/drivers/tecmo.cpp
$(OBJ)/drivers/gaiden.o: src/vidhrdw/gaiden.cpp src/drivers/gaiden.cpp
$(OBJ)/drivers/wc90.o: src/vidhrdw/wc90.cpp src/drivers/wc90.cpp
$(OBJ)/drivers/wc90b.o: src/vidhrdw/wc90b.cpp src/drivers/wc90b.cpp
$(OBJ)/drivers/tecmo16.o: src/vidhrdw/tecmo16.cpp src/drivers/tecmo16.cpp
OBJ_TEHKAN = $(OBJ)/drivers/senjyo.o $(OBJ)/drivers/bombjack.o $(OBJ)/drivers/pbaction.o $(OBJ)/drivers/tehkanwc.o $(OBJ)/drivers/solomon.o \
	$(OBJ)/drivers/tecmo.o $(OBJ)/drivers/gaiden.o $(OBJ)/drivers/wc90.o $(OBJ)/drivers/wc90b.o $(OBJ)/drivers/tecmo16.o

$(OBJ)/drivers/scramble.o: src/machine/scramble.cpp src/sndhrdw/scramble.cpp src/drivers/scramble.cpp
$(OBJ)/drivers/frogger.o: src/vidhrdw/frogger.cpp src/sndhrdw/frogger.cpp src/drivers/frogger.cpp
$(OBJ)/drivers/amidar.o: src/vidhrdw/amidar.cpp src/drivers/amidar.cpp
$(OBJ)/drivers/fastfred.o: src/vidhrdw/fastfred.cpp src/drivers/fastfred.cpp
$(OBJ)/drivers/tutankhm.o: src/vidhrdw/tutankhm.cpp src/drivers/tutankhm.cpp
$(OBJ)/drivers/pooyan.o: src/vidhrdw/pooyan.cpp src/drivers/pooyan.cpp
$(OBJ)/drivers/timeplt.o: src/sndhrdw/timeplt.cpp src/vidhrdw/timeplt.cpp src/drivers/timeplt.cpp
$(OBJ)/drivers/megazone.o: src/vidhrdw/megazone.cpp src/drivers/megazone.cpp
$(OBJ)/drivers/pandoras.o: src/vidhrdw/pandoras.cpp src/drivers/pandoras.cpp
$(OBJ)/drivers/gyruss.o: src/sndhrdw/gyruss.cpp src/vidhrdw/gyruss.cpp src/drivers/gyruss.cpp
$(OBJ)/drivers/trackfld.o: src/vidhrdw/trackfld.cpp src/sndhrdw/trackfld.cpp src/drivers/trackfld.cpp
$(OBJ)/drivers/rocnrope.o: src/vidhrdw/rocnrope.cpp src/drivers/rocnrope.cpp
$(OBJ)/drivers/circusc.o: src/vidhrdw/circusc.cpp src/drivers/circusc.cpp
$(OBJ)/drivers/tp84.o: src/vidhrdw/tp84.cpp src/drivers/tp84.cpp
$(OBJ)/drivers/hyperspt.o: src/vidhrdw/hyperspt.cpp src/drivers/hyperspt.cpp
$(OBJ)/drivers/sbasketb.o: src/vidhrdw/sbasketb.cpp src/drivers/sbasketb.cpp
$(OBJ)/drivers/mikie.o: src/vidhrdw/mikie.cpp src/drivers/mikie.cpp
$(OBJ)/drivers/yiear.o: src/vidhrdw/yiear.cpp src/drivers/yiear.cpp
$(OBJ)/drivers/shaolins.o: src/vidhrdw/shaolins.cpp src/drivers/shaolins.cpp
$(OBJ)/drivers/pingpong.o: src/vidhrdw/pingpong.cpp src/drivers/pingpong.cpp
$(OBJ)/drivers/gberet.o: src/vidhrdw/gberet.cpp src/drivers/gberet.cpp
$(OBJ)/drivers/jailbrek.o: src/vidhrdw/jailbrek.cpp src/drivers/jailbrek.cpp
$(OBJ)/drivers/finalizr.o: src/vidhrdw/finalizr.cpp src/drivers/finalizr.cpp
$(OBJ)/drivers/ironhors.o: src/vidhrdw/ironhors.cpp src/drivers/ironhors.cpp
$(OBJ)/drivers/jackal.o: src/machine/jackal.cpp src/vidhrdw/jackal.cpp src/drivers/jackal.cpp
$(OBJ)/drivers/ddrible.o: src/machine/ddrible.cpp src/vidhrdw/ddrible.cpp src/drivers/ddrible.cpp
$(OBJ)/drivers/contra.o: src/vidhrdw/contra.cpp src/drivers/contra.cpp
$(OBJ)/drivers/combatsc.o: src/vidhrdw/combatsc.cpp src/drivers/combatsc.cpp
$(OBJ)/drivers/hcastle.o: src/vidhrdw/hcastle.cpp src/drivers/hcastle.cpp
$(OBJ)/drivers/nemesis.o: src/vidhrdw/nemesis.cpp src/drivers/nemesis.cpp
$(OBJ)/drivers/rockrage.o: src/vidhrdw/rockrage.cpp src/drivers/rockrage.cpp
$(OBJ)/drivers/flkatck.o: src/vidhrdw/flkatck.cpp src/drivers/flkatck.cpp
$(OBJ)/drivers/fastlane.o: src/vidhrdw/fastlane.cpp src/drivers/fastlane.cpp
$(OBJ)/drivers/labyrunr.o: src/vidhrdw/labyrunr.cpp src/drivers/labyrunr.cpp
$(OBJ)/drivers/battlnts.o: src/vidhrdw/battlnts.cpp src/drivers/battlnts.cpp
$(OBJ)/drivers/bladestl.o: src/vidhrdw/bladestl.cpp src/drivers/bladestl.cpp
$(OBJ)/drivers/ajax.o: src/machine/ajax.cpp src/vidhrdw/ajax.cpp src/drivers/ajax.cpp
$(OBJ)/drivers/thunderx.o: src/vidhrdw/thunderx.cpp src/drivers/thunderx.cpp
$(OBJ)/drivers/mainevt.o: src/vidhrdw/mainevt.cpp src/drivers/mainevt.cpp
$(OBJ)/drivers/88games.o: src/vidhrdw/88games.cpp src/drivers/88games.cpp
$(OBJ)/drivers/gbusters.o: src/vidhrdw/gbusters.cpp src/drivers/gbusters.cpp
$(OBJ)/drivers/crimfght.o: src/vidhrdw/crimfght.cpp src/drivers/crimfght.cpp
$(OBJ)/drivers/spy.o: src/vidhrdw/spy.cpp src/drivers/spy.cpp
$(OBJ)/drivers/bottom9.o: src/vidhrdw/bottom9.cpp src/drivers/bottom9.cpp
$(OBJ)/drivers/blockhl.o: src/vidhrdw/blockhl.cpp src/drivers/blockhl.cpp
$(OBJ)/drivers/aliens.o: src/vidhrdw/aliens.cpp src/drivers/aliens.cpp
$(OBJ)/drivers/surpratk.o: src/vidhrdw/surpratk.cpp src/drivers/surpratk.cpp
$(OBJ)/drivers/parodius.o: src/vidhrdw/parodius.cpp src/drivers/parodius.cpp
$(OBJ)/drivers/rollerg.o: src/vidhrdw/rollerg.cpp src/drivers/rollerg.cpp
$(OBJ)/drivers/xexex.o: src/vidhrdw/xexex.cpp src/drivers/xexex.cpp
$(OBJ)/drivers/simpsons.o: src/machine/simpsons.cpp src/vidhrdw/simpsons.cpp src/drivers/simpsons.cpp
$(OBJ)/drivers/vendetta.o: src/vidhrdw/vendetta.cpp src/drivers/vendetta.cpp
$(OBJ)/drivers/twin16.o: src/vidhrdw/twin16.cpp src/drivers/twin16.cpp
$(OBJ)/drivers/gradius3.o: src/vidhrdw/gradius3.cpp src/drivers/gradius3.cpp
$(OBJ)/drivers/tmnt.o: src/vidhrdw/tmnt.cpp src/drivers/tmnt.cpp
$(OBJ)/drivers/xmen.o: src/vidhrdw/xmen.cpp src/drivers/xmen.cpp
$(OBJ)/drivers/wecleman.o: src/vidhrdw/wecleman.cpp src/drivers/wecleman.cpp
$(OBJ)/drivers/chqflag.o: src/vidhrdw/chqflag.cpp src/drivers/chqflag.cpp
$(OBJ)/drivers/ultraman.o: src/vidhrdw/ultraman.cpp src/drivers/ultraman.cpp
OBJ_KONAMI = $(OBJ)/drivers/scramble.o $(OBJ)/drivers/frogger.o $(OBJ)/drivers/scobra.o $(OBJ)/drivers/amidar.o $(OBJ)/drivers/fastfred.o \
	$(OBJ)/drivers/tutankhm.o $(OBJ)/drivers/junofrst.o $(OBJ)/drivers/pooyan.o $(OBJ)/drivers/timeplt.o $(OBJ)/drivers/megazone.o \
	$(OBJ)/drivers/pandoras.o $(OBJ)/drivers/gyruss.o $(OBJ)/machine/konami.o $(OBJ)/drivers/trackfld.o $(OBJ)/drivers/rocnrope.o \
	$(OBJ)/drivers/circusc.o $(OBJ)/drivers/tp84.o $(OBJ)/drivers/hyperspt.o $(OBJ)/drivers/sbasketb.o $(OBJ)/drivers/mikie.o \
	$(OBJ)/drivers/yiear.o $(OBJ)/drivers/shaolins.o $(OBJ)/drivers/pingpong.o $(OBJ)/drivers/gberet.o $(OBJ)/drivers/jailbrek.o \
	$(OBJ)/drivers/finalizr.o $(OBJ)/drivers/ironhors.o $(OBJ)/drivers/jackal.o $(OBJ)/drivers/ddrible.o $(OBJ)/drivers/contra.o \
	$(OBJ)/drivers/combatsc.o $(OBJ)/drivers/hcastle.o $(OBJ)/drivers/nemesis.o $(OBJ)/vidhrdw/konamiic.o $(OBJ)/drivers/rockrage.o \
	$(OBJ)/drivers/flkatck.o $(OBJ)/drivers/fastlane.o $(OBJ)/drivers/labyrunr.o $(OBJ)/drivers/battlnts.o $(OBJ)/drivers/bladestl.o \
	$(OBJ)/drivers/ajax.o $(OBJ)/drivers/thunderx.o $(OBJ)/drivers/mainevt.o $(OBJ)/drivers/88games.o $(OBJ)/drivers/gbusters.o \
	$(OBJ)/drivers/crimfght.o $(OBJ)/drivers/spy.o $(OBJ)/drivers/bottom9.o $(OBJ)/drivers/blockhl.o $(OBJ)/drivers/aliens.o \
	$(OBJ)/drivers/surpratk.o $(OBJ)/drivers/parodius.o $(OBJ)/drivers/rollerg.o $(OBJ)/drivers/xexex.o $(OBJ)/drivers/simpsons.o \
	$(OBJ)/drivers/vendetta.o $(OBJ)/drivers/twin16.o $(OBJ)/drivers/gradius3.o $(OBJ)/drivers/tmnt.o $(OBJ)/drivers/xmen.o \
	$(OBJ)/drivers/wecleman.o $(OBJ)/drivers/chqflag.o $(OBJ)/drivers/ultraman.o

$(OBJ)/drivers/exidy.o: src/sndhrdw/targ.cpp src/vidhrdw/exidy.cpp src/sndhrdw/exidy.cpp src/drivers/exidy.cpp
$(OBJ)/drivers/circus.o: src/vidhrdw/circus.cpp src/drivers/circus.cpp
$(OBJ)/drivers/starfire.o: src/vidhrdw/starfire.cpp src/drivers/starfire.cpp
$(OBJ)/drivers/victory.o: src/vidhrdw/victory.cpp src/drivers/victory.cpp
$(OBJ)/drivers/exidy440.o: src/sndhrdw/exidy440.cpp src/vidhrdw/exidy440.cpp src/drivers/exidy440.cpp
OBJ_EXIDY = $(OBJ)/drivers/exidy.o $(OBJ)/drivers/circus.o $(OBJ)/drivers/starfire.o $(OBJ)/drivers/victory.o $(OBJ)/drivers/exidy440.o

$(OBJ)/drivers/asteroid.o: src/machine/asteroid.cpp src/sndhrdw/asteroid.cpp src/drivers/asteroid.cpp
$(OBJ)/drivers/bzone.o: src/sndhrdw/bzone.cpp src/drivers/bzone.cpp
$(OBJ)/drivers/starwars.o: src/machine/starwars.cpp src/machine/swmathbx.cpp src/sndhrdw/starwars.cpp src/drivers/starwars.cpp
$(OBJ)/drivers/mhavoc.o: src/machine/mhavoc.cpp src/drivers/mhavoc.cpp
$(OBJ)/drivers/quantum.o: src/machine/quantum.cpp src/drivers/quantum.cpp
$(OBJ)/drivers/atarifb.o: src/machine/atarifb.cpp src/vidhrdw/atarifb.cpp src/drivers/atarifb.cpp
$(OBJ)/drivers/sprint2.o: src/machine/sprint2.cpp src/vidhrdw/sprint2.cpp src/drivers/sprint2.cpp
$(OBJ)/drivers/sbrkout.o: src/machine/sbrkout.cpp src/vidhrdw/sbrkout.cpp src/drivers/sbrkout.cpp
$(OBJ)/drivers/dominos.o: src/machine/dominos.cpp src/vidhrdw/dominos.cpp src/drivers/dominos.cpp
$(OBJ)/drivers/nitedrvr.o: src/vidhrdw/nitedrvr.cpp src/machine/nitedrvr.cpp src/drivers/nitedrvr.cpp
$(OBJ)/drivers/bsktball.o: src/vidhrdw/bsktball.cpp src/machine/bsktball.cpp src/drivers/bsktball.cpp
$(OBJ)/drivers/copsnrob.o: src/vidhrdw/copsnrob.cpp src/machine/copsnrob.cpp src/drivers/copsnrob.cpp
$(OBJ)/drivers/avalnche.o: src/machine/avalnche.cpp src/vidhrdw/avalnche.cpp src/drivers/avalnche.cpp
$(OBJ)/drivers/subs.o: src/machine/subs.cpp src/vidhrdw/subs.cpp src/drivers/subs.cpp
$(OBJ)/drivers/canyon.o: src/vidhrdw/canyon.cpp src/drivers/canyon.cpp
$(OBJ)/drivers/skydiver.o: src/vidhrdw/skydiver.cpp src/drivers/skydiver.cpp
$(OBJ)/drivers/warlord.o: src/vidhrdw/warlord.cpp src/drivers/warlord.cpp
$(OBJ)/drivers/centiped.o: src/vidhrdw/centiped.cpp src/drivers/centiped.cpp
$(OBJ)/drivers/milliped.o: src/vidhrdw/milliped.cpp src/drivers/milliped.cpp
$(OBJ)/drivers/qwakprot.o: src/vidhrdw/qwakprot.cpp src/drivers/qwakprot.cpp
$(OBJ)/drivers/kangaroo.o: src/vidhrdw/kangaroo.cpp src/drivers/kangaroo.cpp
$(OBJ)/drivers/arabian.o: src/vidhrdw/arabian.cpp src/drivers/arabian.cpp
$(OBJ)/drivers/missile.o: src/machine/missile.cpp src/vidhrdw/missile.cpp src/drivers/missile.cpp
$(OBJ)/drivers/foodf.o: src/machine/foodf.cpp src/vidhrdw/foodf.cpp src/drivers/foodf.cpp
$(OBJ)/drivers/liberatr.o: src/vidhrdw/liberatr.cpp src/drivers/liberatr.cpp
$(OBJ)/drivers/ccastles.o: src/vidhrdw/ccastles.cpp src/drivers/ccastles.cpp
$(OBJ)/drivers/cloak.o: src/vidhrdw/cloak.cpp src/drivers/cloak.cpp
$(OBJ)/drivers/cloud9.o: src/vidhrdw/cloud9.cpp src/drivers/cloud9.cpp
$(OBJ)/drivers/jedi.o: src/machine/jedi.cpp src/vidhrdw/jedi.cpp src/sndhrdw/jedi.cpp src/drivers/jedi.cpp
$(OBJ)/drivers/atarisy1.o: src/vidhrdw/atarisy1.cpp src/drivers/atarisy1.cpp
$(OBJ)/drivers/atarisy2.o: src/vidhrdw/atarisy2.cpp src/drivers/atarisy2.cpp
$(OBJ)/drivers/gauntlet.o: src/vidhrdw/gauntlet.cpp src/drivers/gauntlet.cpp
$(OBJ)/drivers/atetris.o: src/vidhrdw/atetris.cpp src/drivers/atetris.cpp
$(OBJ)/drivers/toobin.o: src/vidhrdw/toobin.cpp src/drivers/toobin.cpp
$(OBJ)/drivers/vindictr.o: src/vidhrdw/vindictr.cpp src/drivers/vindictr.cpp
$(OBJ)/drivers/klax.o: src/vidhrdw/klax.cpp src/drivers/klax.cpp
$(OBJ)/drivers/blstroid.o: src/vidhrdw/blstroid.cpp src/drivers/blstroid.cpp
$(OBJ)/drivers/xybots.o: src/vidhrdw/xybots.cpp src/drivers/xybots.cpp
$(OBJ)/drivers/eprom.o: src/vidhrdw/eprom.cpp src/drivers/eprom.cpp
$(OBJ)/drivers/skullxbo.o: src/vidhrdw/skullxbo.cpp src/drivers/skullxbo.cpp
$(OBJ)/drivers/badlands.o: src/vidhrdw/badlands.cpp src/drivers/badlands.cpp
$(OBJ)/drivers/cyberbal.o: src/vidhrdw/cyberbal.cpp src/drivers/cyberbal.cpp
$(OBJ)/drivers/rampart.o: src/vidhrdw/rampart.cpp src/drivers/rampart.cpp
$(OBJ)/drivers/shuuz.o: src/vidhrdw/shuuz.cpp src/drivers/shuuz.cpp
$(OBJ)/drivers/hydra.o: src/vidhrdw/hydra.cpp src/drivers/hydra.cpp
$(OBJ)/drivers/thunderj.o: src/vidhrdw/thunderj.cpp src/drivers/thunderj.cpp
$(OBJ)/drivers/batman.o: src/vidhrdw/batman.cpp src/drivers/batman.cpp
$(OBJ)/drivers/relief.o: src/vidhrdw/relief.cpp src/drivers/relief.cpp
$(OBJ)/drivers/offtwall.o: src/vidhrdw/offtwall.cpp src/drivers/offtwall.cpp
$(OBJ)/drivers/arcadecl.o: src/vidhrdw/arcadecl.cpp src/drivers/arcadecl.cpp
$(OBJ)/drivers/firetrk.o: src/drivers/firetrk.cpp
OBJ_ATARI = $(OBJ)/machine/atari_vg.o $(OBJ)/drivers/asteroid.o $(OBJ)/vidhrdw/llander.o $(OBJ)/sndhrdw/llander.o \
    $(OBJ)/drivers/bwidow.o $(OBJ)/drivers/bzone.o $(OBJ)/sndhrdw/redbaron.o $(OBJ)/drivers/tempest.o \
	$(OBJ)/drivers/starwars.o $(OBJ)/drivers/mhavoc.o $(OBJ)/drivers/quantum.o $(OBJ)/drivers/atarifb.o $(OBJ)/drivers/sprint2.o \
	$(OBJ)/drivers/sbrkout.o $(OBJ)/drivers/dominos.o $(OBJ)/drivers/nitedrvr.o $(OBJ)/drivers/bsktball.o $(OBJ)/drivers/copsnrob.o \
	$(OBJ)/drivers/avalnche.o $(OBJ)/drivers/subs.o $(OBJ)/drivers/canyon.o $(OBJ)/drivers/skydiver.o $(OBJ)/drivers/warlord.o \
	$(OBJ)/drivers/centiped.o $(OBJ)/drivers/milliped.o $(OBJ)/drivers/qwakprot.o $(OBJ)/drivers/kangaroo.o $(OBJ)/drivers/arabian.o \
	$(OBJ)/drivers/missile.o $(OBJ)/drivers/foodf.o $(OBJ)/drivers/liberatr.o $(OBJ)/drivers/ccastles.o $(OBJ)/drivers/cloak.o \
	$(OBJ)/drivers/cloud9.o $(OBJ)/drivers/jedi.o $(OBJ)/machine/atarigen.o $(OBJ)/sndhrdw/atarijsa.o $(OBJ)/machine/slapstic.o \
	$(OBJ)/drivers/atarisy1.o $(OBJ)/drivers/atarisy2.o $(OBJ)/drivers/gauntlet.o $(OBJ)/drivers/atetris.o $(OBJ)/drivers/toobin.o \
	$(OBJ)/drivers/vindictr.o $(OBJ)/drivers/klax.o $(OBJ)/drivers/blstroid.o $(OBJ)/drivers/xybots.o $(OBJ)/drivers/eprom.o \
	$(OBJ)/drivers/skullxbo.o $(OBJ)/drivers/badlands.o $(OBJ)/drivers/cyberbal.o $(OBJ)/drivers/rampart.o $(OBJ)/drivers/shuuz.o \
	$(OBJ)/drivers/hydra.o $(OBJ)/drivers/thunderj.o $(OBJ)/drivers/batman.o $(OBJ)/drivers/relief.o $(OBJ)/drivers/offtwall.o \
	$(OBJ)/drivers/arcadecl.o $(OBJ)/drivers/firetrk.o

$(OBJ)/drivers/rockola.o: src/vidhrdw/rockola.cpp src/sndhrdw/rockola.cpp src/drivers/rockola.cpp
$(OBJ)/drivers/lasso.o: src/vidhrdw/lasso.cpp src/drivers/lasso.cpp
$(OBJ)/drivers/munchmo.o: src/vidhrdw/munchmo.cpp src/drivers/munchmo.cpp
$(OBJ)/drivers/marvins.o: src/vidhrdw/marvins.cpp src/drivers/marvins.cpp
$(OBJ)/drivers/snk.o: src/vidhrdw/snk.cpp src/drivers/snk.cpp
$(OBJ)/drivers/snk68.o: src/vidhrdw/snk68.cpp src/drivers/snk68.cpp
$(OBJ)/drivers/prehisle.o: src/vidhrdw/prehisle.cpp src/drivers/prehisle.cpp
OBJ_SNK = $(OBJ)/drivers/rockola.o $(OBJ)/drivers/lasso.o $(OBJ)/drivers/munchmo.o $(OBJ)/drivers/marvins.o $(OBJ)/drivers/hal21.o \
	$(OBJ)/drivers/snk.o $(OBJ)/drivers/snk68.o $(OBJ)/drivers/prehisle.o

$(OBJ)/drivers/alpha68k.o: src/vidhrdw/alpha68k.cpp src/drivers/alpha68k.cpp
$(OBJ)/drivers/champbas.o: src/vidhrdw/champbas.cpp src/drivers/champbas.cpp
$(OBJ)/drivers/exctsccr.o: src/machine/exctsccr.cpp src/vidhrdw/exctsccr.cpp src/drivers/exctsccr.cpp
OBJ_ALPHA = $(OBJ)/drivers/alpha68k.o $(OBJ)/drivers/champbas.o $(OBJ)/drivers/exctsccr.o

$(OBJ)/drivers/tagteam.o: src/vidhrdw/tagteam.cpp src/drivers/tagteam.cpp
$(OBJ)/drivers/ssozumo.o: src/vidhrdw/ssozumo.cpp src/drivers/ssozumo.cpp
$(OBJ)/drivers/mystston.o: src/vidhrdw/mystston.cpp src/drivers/mystston.cpp
$(OBJ)/drivers/bogeyman.o: src/vidhrdw/bogeyman.cpp src/drivers/bogeyman.cpp
$(OBJ)/drivers/matmania.o: src/machine/maniach.cpp src/vidhrdw/matmania.cpp src/drivers/matmania.cpp
$(OBJ)/drivers/renegade.o: src/vidhrdw/renegade.cpp src/drivers/renegade.cpp
$(OBJ)/drivers/xain.o: src/vidhrdw/xain.cpp src/drivers/xain.cpp
$(OBJ)/drivers/battlane.o: src/vidhrdw/battlane.cpp src/drivers/battlane.cpp
$(OBJ)/drivers/ddragon.o: src/vidhrdw/ddragon.cpp src/drivers/ddragon.cpp
$(OBJ)/drivers/ddragon3.o: src/vidhrdw/ddragon3.cpp src/drivers/ddragon3.cpp
$(OBJ)/drivers/blockout.o: src/vidhrdw/blockout.cpp src/drivers/blockout.cpp
OBJ_TECHNOS = $(OBJ)/drivers/scregg.o $(OBJ)/drivers/tagteam.o $(OBJ)/drivers/ssozumo.o $(OBJ)/drivers/mystston.o \
    $(OBJ)/drivers/bogeyman.o $(OBJ)/drivers/matmania.o $(OBJ)/drivers/renegade.o $(OBJ)/drivers/xain.o $(OBJ)/drivers/battlane.o \
    $(OBJ)/drivers/ddragon.o $(OBJ)/drivers/ddragon3.o $(OBJ)/drivers/blockout.o

$(OBJ)/drivers/suna8.o: src/vidhrdw/suna8.cpp src/drivers/suna8.cpp
OBJ_SUNA = $(OBJ)/drivers/suna8.o

$(OBJ)/drivers/berzerk.o: src/machine/berzerk.cpp src/vidhrdw/berzerk.cpp src/sndhrdw/berzerk.cpp src/drivers/berzerk.cpp
OBJ_BERZERK = $(OBJ)/drivers/berzerk.o

$(OBJ)/drivers/gameplan.o: src/vidhrdw/gameplan.cpp src/drivers/gameplan.cpp
OBJ_GAMEPLAN = $(OBJ)/drivers/gameplan.o

$(OBJ)/drivers/route16.o: src/vidhrdw/route16.cpp src/drivers/route16.cpp
$(OBJ)/drivers/ttmahjng.o: src/vidhrdw/ttmahjng.cpp src/drivers/ttmahjng.cpp
OBJ_STRATVOX = $(OBJ)/drivers/route16.o $(OBJ)/drivers/ttmahjng.o

$(OBJ)/drivers/zaccaria.o: src/vidhrdw/zaccaria.cpp src/drivers/zaccaria.cpp
OBJ_ZACCARIA = $(OBJ)/drivers/zaccaria.o

$(OBJ)/drivers/nova2001.o: src/vidhrdw/nova2001.cpp src/drivers/nova2001.cpp
$(OBJ)/drivers/pkunwar.o: src/vidhrdw/pkunwar.cpp src/drivers/pkunwar.cpp
$(OBJ)/drivers/ninjakd2.o: src/vidhrdw/ninjakd2.cpp src/drivers/ninjakd2.cpp
$(OBJ)/drivers/mnight.o: src/vidhrdw/mnight.cpp src/drivers/mnight.cpp
OBJ_UPL = $(OBJ)/drivers/nova2001.o $(OBJ)/drivers/pkunwar.o $(OBJ)/drivers/ninjakd2.o $(OBJ)/drivers/mnight.o

$(OBJ)/drivers/exterm.o: src/machine/exterm.cpp src/vidhrdw/exterm.cpp src/drivers/exterm.cpp
$(OBJ)/drivers/wmsyunit.o: src/machine/wmsyunit.cpp src/vidhrdw/wmsyunit.cpp src/drivers/wmsyunit.cpp
$(OBJ)/drivers/wmstunit.o: src/machine/wmstunit.cpp src/vidhrdw/wmstunit.cpp src/drivers/wmstunit.cpp
$(OBJ)/drivers/wmswolfu.o: src/machine/wmswolfu.cpp src/drivers/wmswolfu.cpp
OBJ_TMS = $(OBJ)/drivers/exterm.o $(OBJ)/drivers/wmsyunit.o $(OBJ)/drivers/wmstunit.o $(OBJ)/drivers/wmswolfu.o

$(OBJ)/drivers/jack.o: src/vidhrdw/jack.cpp src/drivers/jack.cpp
OBJ_CINEMAR = $(OBJ)/drivers/jack.o

$(OBJ)/drivers/cinemat.o: src/sndhrdw/cinemat.cpp src/drivers/cinemat.cpp
$(OBJ)/drivers/cchasm.o: src/machine/cchasm.cpp src/vidhrdw/cchasm.cpp src/sndhrdw/cchasm.cpp src/drivers/cchasm.cpp
OBJ_CINEMAV = $(OBJ)/drivers/cinemat.o $(OBJ)/drivers/cchasm.o

$(OBJ)/drivers/thepit.o: src/vidhrdw/thepit.cpp src/drivers/thepit.cpp
OBJ_THEPIT = $(OBJ)/drivers/thepit.o

$(OBJ)/drivers/bagman.o: src/machine/bagman.cpp src/vidhrdw/bagman.cpp src/drivers/bagman.cpp
OBJ_VALADON = $(OBJ)/drivers/bagman.o

$(OBJ)/drivers/wiz.o: src/vidhrdw/wiz.cpp src/drivers/wiz.cpp
$(OBJ)/drivers/stfight.o: src/machine/stfight.cpp src/vidhrdw/stfight.cpp src/drivers/stfight.cpp
$(OBJ)/drivers/dynduke.o: src/vidhrdw/dynduke.cpp src/drivers/dynduke.cpp
$(OBJ)/drivers/raiden.o: src/vidhrdw/raiden.cpp src/drivers/raiden.cpp
$(OBJ)/drivers/dcon.o: src/vidhrdw/dcon.cpp src/drivers/dcon.cpp
$(OBJ)/drivers/kncljoe.o: src/vidhrdw/kncljoe.cpp src/drivers/kncljoe.cpp
OBJ_SEIBU = $(OBJ)/drivers/wiz.o $(OBJ)/drivers/stfight.o $(OBJ)/sndhrdw/seibu.o $(OBJ)/drivers/dynduke.o \
    $(OBJ)/drivers/raiden.o $(OBJ)/drivers/dcon.o $(OBJ)/drivers/kncljoe.o

$(OBJ)/drivers/cabal.o: src/vidhrdw/cabal.cpp src/drivers/cabal.cpp
$(OBJ)/drivers/toki.o: src/vidhrdw/toki.cpp src/drivers/toki.cpp
$(OBJ)/drivers/bloodbro.o: src/vidhrdw/bloodbro.cpp src/drivers/bloodbro.cpp
OBJ_TAD = $(OBJ)/drivers/cabal.o $(OBJ)/drivers/toki.o $(OBJ)/drivers/bloodbro.o

$(OBJ)/drivers/exerion.o: src/vidhrdw/exerion.cpp src/drivers/exerion.cpp
$(OBJ)/drivers/aeroboto.o: src/vidhrdw/aeroboto.cpp src/drivers/aeroboto.cpp
$(OBJ)/drivers/citycon.o: src/vidhrdw/citycon.cpp src/drivers/citycon.cpp
$(OBJ)/drivers/pinbo.o: src/vidhrdw/pinbo.cpp src/drivers/pinbo.cpp
$(OBJ)/drivers/psychic5.o: src/vidhrdw/psychic5.cpp src/drivers/psychic5.cpp
$(OBJ)/drivers/ginganin.o: src/vidhrdw/ginganin.cpp src/drivers/ginganin.cpp
$(OBJ)/drivers/megasys1.o: src/vidhrdw/megasys1.cpp src/drivers/megasys1.cpp
$(OBJ)/drivers/cischeat.o: src/vidhrdw/cischeat.cpp src/drivers/cischeat.cpp
$(OBJ)/drivers/skyfox.o: src/vidhrdw/skyfox.cpp src/drivers/skyfox.cpp
$(OBJ)/drivers/argus.o: src/vidhrdw/argus.cpp src/drivers/argus.cpp
$(OBJ)/drivers/momoko.o: src/vidhrdw/momoko.cpp src/drivers/momoko.cpp
OBJ_JALECO = $(OBJ)/drivers/exerion.o $(OBJ)/drivers/aeroboto.o $(OBJ)/drivers/citycon.o $(OBJ)/drivers/pinbo.o $(OBJ)/drivers/psychic5.o \
	$(OBJ)/drivers/ginganin.o $(OBJ)/drivers/megasys1.o $(OBJ)/drivers/cischeat.o $(OBJ)/drivers/skyfox.o $(OBJ)/drivers/argus.o $(OBJ)/drivers/momoko.o

$(OBJ)/drivers/rpunch.o: src/vidhrdw/rpunch.cpp src/drivers/rpunch.cpp
$(OBJ)/drivers/tail2nos.o: src/vidhrdw/tail2nos.cpp src/drivers/tail2nos.cpp
$(OBJ)/drivers/pipedrm.o: src/vidhrdw/pipedrm.cpp src/drivers/pipedrm.cpp
$(OBJ)/drivers/aerofgt.o: src/vidhrdw/aerofgt.cpp src/drivers/aerofgt.cpp
OBJ_VSYSTEM = $(OBJ)/drivers/rpunch.o $(OBJ)/drivers/tail2nos.o $(OBJ)/drivers/pipedrm.o $(OBJ)/drivers/aerofgt.o

$(OBJ)/drivers/psikyo.o: src/vidhrdw/psikyo.cpp src/drivers/psikyo.cpp
OBJ_PSIKYO = $(OBJ)/drivers/psikyo.o

$(OBJ)/drivers/leland.o: src/vidhrdw/leland.cpp src/sndhrdw/leland.cpp src/drivers/leland.cpp
OBJ_LELAND = $(OBJ)/machine/8254pit.o $(OBJ)/drivers/leland.o $(OBJ)/drivers/ataxx.o

$(OBJ)/drivers/marineb.o: src/vidhrdw/marineb.cpp src/drivers/marineb.cpp
$(OBJ)/drivers/funkybee.o: src/vidhrdw/funkybee.cpp src/drivers/funkybee.cpp
$(OBJ)/drivers/zodiack.o: src/vidhrdw/zodiack.cpp src/drivers/zodiack.cpp
$(OBJ)/drivers/espial.o: src/vidhrdw/espial.cpp src/drivers/espial.cpp
$(OBJ)/drivers/vastar.o: src/vidhrdw/vastar.cpp src/drivers/vastar.cpp
OBJ_ORCA = $(OBJ)/drivers/marineb.o $(OBJ)/drivers/funkybee.o $(OBJ)/drivers/zodiack.o $(OBJ)/drivers/espial.o $(OBJ)/drivers/vastar.o

$(OBJ)/drivers/gaelco.o: src/vidhrdw/gaelco.cpp src/drivers/gaelco.cpp
$(OBJ)/drivers/splash.o: src/vidhrdw/splash.cpp src/drivers/splash.cpp
OBJ_GAELCO = $(OBJ)/drivers/gaelco.o $(OBJ)/drivers/splash.o

$(OBJ)/drivers/kaneko16.o: src/vidhrdw/kaneko16.cpp src/drivers/kaneko16.cpp
$(OBJ)/drivers/galpanic.o: src/vidhrdw/galpanic.cpp src/drivers/galpanic.cpp
$(OBJ)/drivers/airbustr.o: src/vidhrdw/airbustr.cpp src/drivers/airbustr.cpp
OBJ_KANEKO = $(OBJ)/drivers/kaneko16.o $(OBJ)/drivers/galpanic.o $(OBJ)/drivers/airbustr.o

$(OBJ)/drivers/neogeo.o: src/machine/neogeo.cpp src/machine/pd4990a.cpp src/vidhrdw/neogeo.cpp src/drivers/neogeo.cpp
OBJ_NEOGEO = $(OBJ)/drivers/neogeo.o

$(OBJ)/drivers/hanaawas.o: src/vidhrdw/hanaawas.cpp src/drivers/hanaawas.cpp
$(OBJ)/drivers/seta.o: src/vidhrdw/seta.cpp src/sndhrdw/seta.cpp src/drivers/seta.cpp
OBJ_SETA = $(OBJ)/drivers/hanaawas.o $(OBJ)/drivers/seta.o

$(OBJ)/drivers/ohmygod.o: src/vidhrdw/ohmygod.cpp src/drivers/ohmygod.cpp
$(OBJ)/drivers/powerins.o: src/vidhrdw/powerins.cpp src/drivers/powerins.cpp
OBJ_ATLUS = $(OBJ)/drivers/ohmygod.o $(OBJ)/drivers/powerins.o

$(OBJ)/drivers/shangha3.o: src/vidhrdw/shangha3.cpp src/drivers/shangha3.cpp
OBJ_SUN = $(OBJ)/drivers/shanghai.o $(OBJ)/drivers/shangha3.o

$(OBJ)/drivers/gundealr.o: src/vidhrdw/gundealr.cpp src/drivers/gundealr.cpp
OBJ_DOOYONG = $(OBJ)/drivers/gundealr.o

$(OBJ)/drivers/macross.o: src/vidhrdw/macross.cpp src/drivers/macross.cpp
$(OBJ)/drivers/bjtwin.o: src/vidhrdw/bjtwin.cpp src/drivers/bjtwin.cpp
OBJ_NMK = $(OBJ)/drivers/macross.o $(OBJ)/drivers/bjtwin.o


$(OBJ)/drivers/spacefb.o: src/vidhrdw/spacefb.cpp src/drivers/spacefb.cpp
$(OBJ)/drivers/blueprnt.o: src/vidhrdw/blueprnt.cpp src/drivers/blueprnt.cpp
$(OBJ)/drivers/dday.o: src/vidhrdw/dday.cpp src/drivers/dday.cpp
$(OBJ)/drivers/leprechn.o: src/vidhrdw/leprechn.cpp src/drivers/leprechn.cpp
$(OBJ)/drivers/hexa.o: src/vidhrdw/hexa.cpp src/drivers/hexa.cpp
$(OBJ)/drivers/redalert.o: src/vidhrdw/redalert.cpp src/sndhrdw/redalert.cpp src/drivers/redalert.cpp
$(OBJ)/drivers/irobot.o: src/machine/irobot.cpp src/vidhrdw/irobot.cpp src/drivers/irobot.cpp
$(OBJ)/drivers/spiders.o: src/machine/spiders.cpp src/vidhrdw/crtc6845.cpp src/vidhrdw/spiders.cpp src/drivers/spiders.cpp
$(OBJ)/drivers/stactics.o: src/machine/stactics.cpp src/vidhrdw/stactics.cpp src/drivers/stactics.cpp
$(OBJ)/drivers/sharkatt.o: src/vidhrdw/sharkatt.cpp src/drivers/sharkatt.cpp
$(OBJ)/drivers/kingobox.o: src/vidhrdw/kingobox.cpp src/drivers/kingobox.cpp
$(OBJ)/drivers/zerozone.o: src/vidhrdw/zerozone.cpp src/drivers/zerozone.cpp
$(OBJ)/drivers/speedbal.o: src/vidhrdw/speedbal.cpp src/drivers/speedbal.cpp
$(OBJ)/drivers/sauro.o: src/vidhrdw/sauro.cpp src/drivers/sauro.cpp
$(OBJ)/drivers/ambush.o: src/vidhrdw/ambush.cpp src/drivers/ambush.cpp
$(OBJ)/drivers/starcrus.o: src/vidhrdw/starcrus.cpp src/drivers/starcrus.cpp
$(OBJ)/drivers/goindol.o: src/vidhrdw/goindol.cpp src/drivers/goindol.cpp
$(OBJ)/drivers/meteor.o: src/vidhrdw/meteor.cpp src/drivers/meteor.cpp
$(OBJ)/drivers/aztarac.o: src/vidhrdw/aztarac.cpp src/sndhrdw/aztarac.cpp src/drivers/aztarac.cpp
$(OBJ)/drivers/mole.o: src/vidhrdw/mole.cpp src/drivers/mole.cpp
$(OBJ)/drivers/gotya.o: src/vidhrdw/gotya.cpp src/sndhrdw/gotya.cpp src/drivers/gotya.cpp
$(OBJ)/drivers/mrjong.o: src/vidhrdw/mrjong.cpp src/drivers/mrjong.cpp
$(OBJ)/drivers/polyplay.o: src/vidhrdw/polyplay.cpp src/sndhrdw/polyplay.cpp src/drivers/polyplay.cpp
$(OBJ)/drivers/mermaid.o: src/vidhrdw/mermaid.cpp src/drivers/mermaid.cpp
$(OBJ)/drivers/magix.o: src/vidhrdw/magix.cpp src/drivers/magix.cpp
OBJ_OTHER = $(OBJ)/drivers/spacefb.o $(OBJ)/drivers/blueprnt.o $(OBJ)/drivers/omegrace.o $(OBJ)/drivers/dday.o $(OBJ)/drivers/leprechn.o \
	$(OBJ)/drivers/hexa.o $(OBJ)/drivers/redalert.o $(OBJ)/drivers/irobot.o $(OBJ)/drivers/spiders.o $(OBJ)/drivers/stactics.o \
	$(OBJ)/drivers/sharkatt.o $(OBJ)/drivers/kingobox.o $(OBJ)/drivers/zerozone.o $(OBJ)/drivers/speedbal.o $(OBJ)/drivers/sauro.o \
	$(OBJ)/drivers/ambush.o $(OBJ)/drivers/starcrus.o $(OBJ)/drivers/goindol.o $(OBJ)/drivers/dlair.o $(OBJ)/drivers/meteor.o \
	$(OBJ)/drivers/aztarac.o $(OBJ)/drivers/mole.o $(OBJ)/drivers/gotya.o $(OBJ)/drivers/mrjong.o $(OBJ)/drivers/polyplay.o \
	$(OBJ)/drivers/mermaid.o $(OBJ)/drivers/magix.o $(OBJ)/drivers/royalmah.o

DRVOBJS = \
	$(OBJ_PACMAN) $(OBJ_NICHIBUT) $(OBJ_PHOENIX) $(OBJ_NAMCO) $(OBJ_UNIVERS) $(OBJ_NINTENDO) $(OBJ_MIDW8080) $(OBJ_MEADOWS) $(OBJ_MIDWAY) \
	$(OBJ_IREM) $(OBJ_GOTTLIEB) $(OBJ_TAITO) $(OBJ_TOAPLAN) $(OBJ_CAVE) $(OBJ_KYUGO) $(OBJ_WILLIAMS) $(OBJ_GREMLIN) $(OBJ_VICDUAL) \
	$(OBJ_CAPCOM) $(OBJ_CAPBOWL) $(OBJ_LELAND) $(OBJ_SEGA) $(OBJ_DENIAM) $(OBJ_DATAEAST) $(OBJ_TEHKAN) $(OBJ_KONAMI) \
	$(OBJ_EXIDY) $(OBJ_ATARI) $(OBJ_SNK) $(OBJ_ALPHA) $(OBJ_TECHNOS) $(OBJ_BERZERK) $(OBJ_GAMEPLAN) $(OBJ_STRATVOX) $(OBJ_ZACCARIA) \
	$(OBJ_UPL) $(OBJ_TMS) $(OBJ_CINEMAR) $(OBJ_CINEMAV) $(OBJ_THEPIT) $(OBJ_VALADON) $(OBJ_SEIBU) $(OBJ_TAD) $(OBJ_JALECO) \
	$(OBJ_VSYSTEM) $(OBJ_PSIKYO) $(OBJ_ORCA) $(OBJ_GAELCO) $(OBJ_KANEKO) $(OBJ_SUNA) $(OBJ_SETA) $(OBJ_ATLUS) $(OBJ_SUN) \
	$(OBJ_DOOYONG) $(OBJ_NMK) $(OBJ_OTHER) $(OBJ_NEOGEO)

COREOBJS += $(OBJ)/driver.o

# generated text files
TEXTS += gamelist.txt

gamelist.txt: $(EMULATOR)
	@echo Generating $@...
	@$(EMULATOR) -gamelistheader -noclones > gamelist.txt
	@$(EMULATOR) -gamelist -noclones | sort >> gamelist.txt
	@$(EMULATOR) -gamelistfooter >> gamelist.txt

