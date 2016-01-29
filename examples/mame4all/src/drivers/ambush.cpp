#include "../vidhrdw/ambush.cpp"

/***************************************************************************

Ambush Memory Map (preliminary)

driver by Zsolt Vasvari


Memory Mapped:

0000-7fff   R	ROM
8000-87ff	R/W	RAM
a000		R	Watchdog Reset
c080-c09f	W	Scroll RAM (1 byte for each column)
c100-c1ff	W   Color RAM (1 line corresponds to 4 in the video ram)
c200-c3ff   W   Sprite RAM
c400-c7ff   W   Video RAM
c800		R   DIP Switches
cc00-cc03   W   ??? (Maybe analog sound triggers?)
cc04		W   Flip Screen
cc05		W   Color Bank Select
cc07		W   Coin Counter


I/O Ports:

00-01		R/W AY8910 #0 (Port A = Input Port #0)
80-81		R/W AY8910 #1 (Port A = Input Port #1)


TODO:

- Verify Z80 and AY8910 clock speeds

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


extern unsigned char *ambush_scrollram;
extern unsigned char *ambush_colorbank;

void ambush_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void ambush_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static WRITE_HANDLER( ambush_coin_counter_w )
{
	coin_counter_w(0, data & 0x01);
	coin_counter_w(1, data & 0x02);
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xa000, 0xa000, watchdog_reset_r },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc800, 0xc800, input_port_2_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xc080, 0xc09f, MWA_RAM, &ambush_scrollram },
	{ 0xc100, 0xc1ff, MWA_RAM, &colorram },
	{ 0xc200, 0xc3ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc400, 0xc7ff, MWA_RAM, &videoram, &videoram_size },
	{ 0xcc00, 0xcc03, MWA_NOP },
	{ 0xcc04, 0xcc04, flip_screen_w },
	{ 0xcc05, 0xcc05, MWA_RAM, &ambush_colorbank },
	{ 0xcc07, 0xcc07, ambush_coin_counter_w },
	{ -1 }  /* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, AY8910_read_port_0_r },
	{ 0x80, 0x80, AY8910_read_port_1_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ 0x80, 0x80, AY8910_control_port_1_w },
	{ 0x81, 0x81, AY8910_write_port_1_w },
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( ambush )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )

	PORT_START      /* DSW */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x1c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x1c, "Service Mode/Free Play" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )  /* used */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x40, "80000" )
	PORT_DIPSETTING(    0x00, "120000" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 chars */
	1024,   /* 2048 characters */
	2,      /* 2 bits per pixel */
	{ 0, 0x2000*8 },  /* The bitplanes are seperate */
	{ 0, 1, 2, 3, 4, 5, 6, 7},
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8},
	8*8     /* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 8*8 chars */
	256,    /* 2048 characters */
	2,      /* 2 bits per pixel */
	{ 0, 0x2000*8 },  /* The bitplanes are seperate */
	{     0,     1,     2,     3,     4,     5,     6,     7,
	  8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{  0*8,  1*8,  2*8,  3*8,  4*8,  5*8,  6*8,  7*8,
	  16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8     /* every char takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,   0, 128 },	/* I'm only using the first 64 colors */
	{ REGION_GFX1, 0x0000, &spritelayout, 0, 128 },
	{ -1 } /* end of array */
};


static struct AY8910interface ay8910_interface =
{
	2,	/* 2 chips */
	1500000,	/* 1.5 MHz ? */
	{ 25, 25 },
	{ input_port_0_r, input_port_1_r },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 }
};


static struct MachineDriver machine_driver_ambush =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,        /* 4.00 MHz??? */
			readmem,writemem,readport,writeport,
			interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,  /* frames per second, vblank duration */
	1,      /* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-3 },  /* The -3 makes the cocktail mode perfect */
	gfxdecodeinfo,
	512, 512,
	ambush_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	ambush_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( ambush )
	ROM_REGION( 0x10000, REGION_CPU1 )       /* 64k for code */
	ROM_LOAD( "ambush.h7",    0x0000, 0x2000, 0xce306563 )
	ROM_LOAD( "ambush.g7",    0x2000, 0x2000, 0x90291409 )
	ROM_LOAD( "ambush.f7",    0x4000, 0x2000, 0xd023ca29 )
	ROM_LOAD( "ambush.e7",    0x6000, 0x2000, 0x6cc2d3ee )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ambush.n4",    0x0000, 0x2000, 0xecc0dc85 )
	ROM_LOAD( "ambush.m4",    0x2000, 0x2000, 0xe86ca98a )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "a.bpr",        0x0000, 0x0100, 0x5f27f511 )  /* color PROMs */
	ROM_LOAD( "b.bpr",        0x0100, 0x0100, 0x1b03fd3b )	/* How is this selected, */
															/* or is it even a color PROM? */
	ROM_LOAD( "13.bpr",		  0x0200, 0x0100, 0x547e970f )  /* I'm not sure what these do. */
	ROM_LOAD( "14.bpr",		  0x0300, 0x0100, 0x622a8ce7 )  /* They don't look like color PROMs */
ROM_END



GAME( 1983, ambush, 0, ambush, ambush, 0, ROT0, "Nippon Amuse Co-Ltd", "Ambush" )
