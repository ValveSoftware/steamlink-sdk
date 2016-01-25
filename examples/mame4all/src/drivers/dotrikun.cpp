#include "../vidhrdw/dotrikun.cpp"

/***************************************************************************

Dottori Kun (Head On's mini game)
(c)1990 SEGA

Driver by Takahiro Nogi (nogi@kt.rim.or.jp) 1999/12/15 -


CPU   : Z-80 (4MHz)
SOUND : (none)

14479.MPR  ; PRG (FIRST VER)
14479A.MPR ; PRG (NEW VER)

* This game is only for the test of cabinet
* BackRaster = WHITE on the FIRST version.
* BackRaster = BLACK on the NEW version.
* On the NEW version, push COIN-SW as TEST MODE.
* 0000-3FFF:ROM 8000-85FF:VRAM(128x96) 8600-87FF:WORK-RAM

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"


WRITE_HANDLER( dotrikun_videoram_w );
void dotrikun_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
int dotrikun_vh_start(void);
void dotrikun_vh_stop(void);

WRITE_HANDLER( dotrikun_color_w );


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x8000, 0x87ff, dotrikun_videoram_w, &videoram, &videoram_size },
	{ -1 }  /* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, dotrikun_color_w },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( dotrikun )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )
INPUT_PORTS_END


static struct MachineDriver machine_driver_dotrikun =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,		 /* 4 Mhz */
			readmem, writemem, readport, writeport,
			interrupt, 1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,					/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	256, 256, { 0, 256-1, 0, 192-1 },
	0,
	2, 0,
	0,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY | VIDEO_MODIFIES_PALETTE,
	0,
	0,
	0,
	dotrikun_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( dotrikun )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "14479a.mpr",	0x0000, 0x4000, 0xb77a50db )
ROM_END

ROM_START( dotriku2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "14479.mpr",	0x0000, 0x4000, 0xa6aa7fa5 )
ROM_END


GAME( 1990, dotrikun, 0,        dotrikun, dotrikun, 0, ROT0, "Sega", "Dottori Kun (new version)" )
GAME( 1990, dotriku2, dotrikun, dotrikun, dotrikun, 0, ROT0, "Sega", "Dottori Kun (old version)" )
