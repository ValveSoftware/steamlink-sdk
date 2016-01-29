/****************************************************************************

Safari Rally by SNK/Taito

Driver by Zsolt Vasvari


This hardware is a precursor to Phoenix.

----------------------------------

CPU board

76477        18MHz

              8080

Video board


 RL07  2114
       2114
       2114
       2114
       2114           RL01 RL02
       2114           RL03 RL04
       2114           RL05 RL06
 RL08  2114

11MHz

----------------------------------

TODO:

- SN76477 sound

****************************************************************************/

#include "driver.h"


unsigned char *safarir_ram1, *safarir_ram2;
size_t safarir_ram_size;

static unsigned char *safarir_ram;
static int safarir_scroll;



WRITE_HANDLER( safarir_ram_w )
{
	safarir_ram[offset] = data;
}

READ_HANDLER( safarir_ram_r )
{
	return safarir_ram[offset];
}


WRITE_HANDLER( safarir_scroll_w )
{
	safarir_scroll = data;
}

WRITE_HANDLER( safarir_ram_bank_w )
{
	safarir_ram = data ? safarir_ram1 : safarir_ram2;
}


void safarir_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
	int offs;


	for (offs = safarir_ram_size/2 - 1;offs >= 0;offs--)
	{
		int sx,sy;
		UINT8 code;


		sx = offs % 32;
		sy = offs / 32;

		code = safarir_ram[offs + safarir_ram_size/2];


		drawgfx(bitmap,Machine->gfx[0],
				code & 0x7f,
				code >> 7,
				0,0,
				(8*sx - safarir_scroll) & 0xff,8*sy,
				&Machine->visible_area,TRANSPARENCY_NONE,0);
	}


	/* draw the frontmost playfield. They are characters, but draw them as sprites */

	for (offs = safarir_ram_size/2 - 1;offs >= 0;offs--)
	{
		int sx,sy,transparency;
		UINT8 code;


		sx = offs % 32;
		sy = offs / 32;

		code = safarir_ram[offs];

		transparency = (sx >= 3) ? TRANSPARENCY_PEN : TRANSPARENCY_NONE;


		drawgfx(bitmap,Machine->gfx[1],
				code & 0x7f,
				code >> 7,
				0,0,
				8*sx,8*sy,
				&Machine->visible_area,transparency,0);
	}
}


static unsigned char palette[] =
{
	0x00,0x00,0x00, /* black */
	0x80,0x80,0x80, /* gray */
	0xff,0xff,0xff, /* white */
};
static unsigned short colortable[] =
{
	0x00, 0x01,
	0x00, 0x02,
};

static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
	memcpy(game_colortable,colortable,sizeof(colortable));
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x17ff, MRA_ROM },
	{ 0x2000, 0x27ff, safarir_ram_r },
	{ 0x3800, 0x38ff, input_port_0_r },
	{ 0x3c00, 0x3cff, input_port_1_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x17ff, MWA_ROM },
	{ 0x2000, 0x27ff, safarir_ram_w, &safarir_ram1, &safarir_ram_size },
	{ 0x2800, 0x28ff, safarir_ram_bank_w },
	{ 0x2c00, 0x2cff, safarir_scroll_w },
	{ 0x3000, 0x30ff, MWA_NOP },	/* goes to SN76477 */

	{ 0x8000, 0x87ff, MWA_NOP, &safarir_ram2 },	/* only here to initialize pointer */
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( safarir )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x04, "Acceleration Rate" )
	PORT_DIPSETTING(    0x00, "Slowest" )
	PORT_DIPSETTING(    0x04, "Slow" )
	PORT_DIPSETTING(    0x08, "Fast" )
	PORT_DIPSETTING(    0x0c, "Fastest" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "3000" )
	PORT_DIPSETTING(    0x20, "5000" )
	PORT_DIPSETTING(    0x40, "7000" )
	PORT_DIPSETTING(    0x60, "9000" )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 chars */
	128,	/* 128 characters */
	1,		/* 1 bit per pixel */
	{ 0 },
	{ 7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout, 0, 2 },
	{ REGION_GFX2, 0, &charlayout, 0, 2 },
	{ -1 } /* end of array */
};



static struct MachineDriver machine_driver_safarir =
{
	/* basic machine hardware */
	{
		{
			CPU_8080,
			3072000,	/* 3 Mhz ? */								\
			readmem,writemem,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,      /* single CPU, no need for interleaving */
	0,	/* init machine */

	/* video hardware */
	32*8, 32*8, { 0*8, 30*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	3,2*2,
	init_palette,

	VIDEO_TYPE_RASTER,
	0,
	0,
	0,
	safarir_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0
};

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( safarir )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for main CPU */
	ROM_LOAD( "rl01",		0x0000, 0x0400, 0xcf7703c9 )
	ROM_LOAD( "rl02",		0x0400, 0x0400, 0x1013ecd3 )
	ROM_LOAD( "rl03",		0x0800, 0x0400, 0x84545894 )
	ROM_LOAD( "rl04",		0x0c00, 0x0400, 0x5dd12f96 )
	ROM_LOAD( "rl05",		0x1000, 0x0400, 0x935ed469 )
	ROM_LOAD( "rl06",		0x1400, 0x0400, 0x24c1cd42 )

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rl08",		0x0000, 0x0400, 0xd6a50aac )

	ROM_REGION( 0x0400, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rl07",		0x0000, 0x0400, 0xba525203 )
ROM_END


GAMEX( ????, safarir, 0, safarir, safarir, 0, ROT90, "SNK", "Safari Rally", GAME_NO_SOUND | GAME_IMPERFECT_COLORS )
