#include "../vidhrdw/minivadr.cpp"

/***************************************************************************

Minivader (Space Invaders's mini game)
(c)1990 Taito Corporation

Driver by Takahiro Nogi (nogi@kt.rim.or.jp) 1999/12/19 -

This is a test board sold together with the cabinet (as required by law in
Japan). It has no sound.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"


WRITE_HANDLER( minivadr_videoram_w );
void minivadr_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void minivadr_init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom);


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0xa000, 0xbfff, MRA_RAM },
	{ 0xe008, 0xe008, input_port_0_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0xa000, 0xbfff, minivadr_videoram_w, &videoram, &videoram_size },
	{ 0xe008, 0xe008, MWA_NOP },		// ???
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( minivadr )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END


static struct MachineDriver machine_driver_minivadr =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			24000000 / 6,		 /* 4 Mhz ? */
			readmem, writemem, 0, 0,
			interrupt, 1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,					/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	256, 256, { 0, 256-1, 16, 240-1 },
	0,
	2, 0,
	minivadr_init_palette,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	0,
	0,
	minivadr_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( minivadr )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "d26-01.bin",	0x0000, 0x2000, 0xa96c823d )
ROM_END


GAME( 1990, minivadr, 0, minivadr, minivadr, 0, ROT0, "Taito Corporation", "Minivader" )
