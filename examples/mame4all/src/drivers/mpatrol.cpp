#include "../vidhrdw/mpatrol.cpp"

/***************************************************************************

Moon Patrol memory map (preliminary)

driver by Nicola Salmoria

0000-3fff ROM
8000-83ff Video RAM
8400-87ff Color RAM
e000-e7ff RAM


read:
8800      protection
d000      IN0
d001      IN1
d002      IN2
d003      DSW1
d004      DSW2

write:
c820-c87f sprites
c8a0-c8ff sprites
d000      sound command
d001      flip screen

I/O ports
write:
10-1f     scroll registers
40        background #1 x position
60        background #1 y position
80        background #2 x position
a0        background #2 y position
c0        background control?

***************************************************************************/

#include "driver.h"
#include "sndhrdw/irem.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"



WRITE_HANDLER( mpatrol_scroll_w );
WRITE_HANDLER( mpatrol_bg1xpos_w );
WRITE_HANDLER( mpatrol_bg1ypos_w );
WRITE_HANDLER( mpatrol_bg2xpos_w );
WRITE_HANDLER( mpatrol_bg2ypos_w );
WRITE_HANDLER( mpatrol_bgcontrol_w );
WRITE_HANDLER( mpatrol_flipscreen_w );
int mpatrol_vh_start(void);
void mpatrol_vh_stop(void);
void mpatrol_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void mpatrol_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

READ_HANDLER( mpatrol_input_port_3_r );



/* this looks like some kind of protection. The game does strange things */
/* if a read from this address doesn't return the value it expects. */
READ_HANDLER( mpatrol_protection_r )
{
//logerror("%04x: read protection\n",cpu_get_pc());
	return cpu_get_reg(Z80_DE) & 0xff;
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x8800, 0x8800, mpatrol_protection_r },
	{ 0xd000, 0xd000, input_port_0_r },          /* IN0 */
	{ 0xd001, 0xd001, input_port_1_r },          /* IN1 */
	{ 0xd002, 0xd002, input_port_2_r },          /* IN2 */
	{ 0xd003, 0xd003, mpatrol_input_port_3_r },  /* DSW1 */
	{ 0xd004, 0xd004, input_port_4_r },          /* DSW2 */
	{ 0xe000, 0xe7ff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x83ff, videoram_w, &videoram, &videoram_size },
	{ 0x8400, 0x87ff, colorram_w, &colorram },
	{ 0xc820, 0xc87f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc8a0, 0xc8ff, MWA_RAM, &spriteram_2 },
	{ 0xd000, 0xd000, irem_sound_cmd_w },
	{ 0xd001, 0xd001, mpatrol_flipscreen_w },	/* + coin counters */
	{ 0xe000, 0xe7ff, MWA_RAM },
	{ -1 }  /* end of table */
};



static struct IOWritePort writeport[] =
{
	{ 0x10, 0x1f, mpatrol_scroll_w },
	{ 0x40, 0x40, mpatrol_bg1xpos_w },
	{ 0x60, 0x60, mpatrol_bg1ypos_w },
	{ 0x80, 0x80, mpatrol_bg2xpos_w },
	{ 0xa0, 0xa0, mpatrol_bg2ypos_w },
	{ 0xc0, 0xc0, mpatrol_bgcontrol_w },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( mpatrol )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	/* coin input must be active for ? frames to be consistently recognized */
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN3, 17 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "10000 30000 50000" )
	PORT_DIPSETTING(    0x08, "20000 40000 60000" )
	PORT_DIPSETTING(    0x04, "10000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )  /* Gets filled in based on the coin mode */

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Sector Selection", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	/* Fake port to support the two different coin modes */
	PORT_START
	PORT_DIPNAME( 0x0f, 0x0f, "Coinage Mode 1" )   /* mapped on coin mode 1 */
	PORT_DIPSETTING(    0x09, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_8C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
 	PORT_DIPNAME( 0x30, 0x30, "Coin A  Mode 2" )   /* mapped on coin mode 2 */
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xc0, 0xc0, "Coin B  Mode 2" )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
INPUT_PORTS_END

/* Identical to mpatrol, the only difference is the number of lives */
INPUT_PORTS_START( mpatrolw )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
/* coin input must be active for ? frames to be consistently recognized */
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN3, 17 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "10000 30000 50000" )
	PORT_DIPSETTING(    0x08, "20000 40000 60000" )
	PORT_DIPSETTING(    0x04, "10000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )  /* Gets filled in based on the coin mode */

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Sector Selection", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	/* Fake port to support the two different coin modes */
	PORT_START
	PORT_DIPNAME( 0x0f, 0x0f, "Coinage Mode 1" )   /* mapped on coin mode 1 */
	PORT_DIPSETTING(    0x09, DEF_STR( 7C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_8C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
 	PORT_DIPNAME( 0x30, 0x30, "Coin A  Mode 2" )   /* mapped on coin mode 2 */
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xc0, 0xc0, "Coin B  Mode 2" )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	512,    /* 512 characters */
	2,      /* 2 bits per pixel */
	{ 0, 512*8*8 }, /* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	128,    /* 128 sprites */
	2,      /* 2 bits per pixel */
	{ 0, 128*16*16 },       /* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8    /* every sprite takes 32 consecutive bytes */
};
static struct GfxLayout bgcharlayout =
{
	32,32,  /* 32*32 characters (actually, it is just 1 big 256x64 image) */
	8,      /* 8 characters */
	2,      /* 2 bits per pixel */
	{ 4, 0 },       /* the two bitplanes for 4 pixels are packed into one byte */
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3, 2*8+0, 2*8+1, 2*8+2, 2*8+3, 3*8+0, 3*8+1, 3*8+2, 3*8+3,
			4*8+0, 4*8+1, 4*8+2, 4*8+3, 5*8+0, 5*8+1, 5*8+2, 5*8+3, 6*8+0, 6*8+1, 6*8+2, 6*8+3, 7*8+0, 7*8+1, 7*8+2, 7*8+3 },
	{ 0*512, 1*512, 2*512, 3*512, 4*512, 5*512, 6*512, 7*512, 8*512, 9*512, 10*512, 11*512, 12*512, 13*512, 14*512, 15*512,
			16*512, 17*512, 18*512, 19*512, 20*512, 21*512, 22*512, 23*512, 24*512, 25*512, 26*512, 27*512, 28*512, 29*512, 30*512, 31*512 },
	8*8
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,               0, 64 },
	{ REGION_GFX2, 0x0000, &spritelayout,          64*4, 16 },
	{ REGION_GFX3, 0x0000, &bgcharlayout, 64*4+16*4+0*4,  1 },	/* top half */
	{ REGION_GFX3, 0x0800, &bgcharlayout, 64*4+16*4+0*4,  1 },	/* bottom half */
	{ REGION_GFX4, 0x0000, &bgcharlayout, 64*4+16*4+1*4,  1 },	/* top half */
	{ REGION_GFX4, 0x0800, &bgcharlayout, 64*4+16*4+1*4,  1 },	/* bottom half */
	{ REGION_GFX5, 0x0000, &bgcharlayout, 64*4+16*4+2*4,  1 },	/* top half */
	{ REGION_GFX5, 0x0800, &bgcharlayout, 64*4+16*4+2*4,  1 },	/* bottom half */
	{ -1 } /* end of array */
};



static struct MachineDriver machine_driver_mpatrol =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,        /* 3.072 Mhz ? */
			readmem,writemem,0,writeport,
			interrupt,1
		},
		IREM_AUDIO_CPU
	},
	57, 1790,	/* accurate frequency, measured on a real board, is 56.75Hz. */
				/* the Lode Runner manual (similar but different hardware) */
				/* talks about 55Hz and 1790ms vblank duration. */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 1*8, 32*8-1 },
	gfxdecodeinfo,
	128+32+32,64*4+16*4+3*4,
	mpatrol_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	mpatrol_vh_start,
	mpatrol_vh_stop,
	mpatrol_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		IREM_AUDIO
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( mpatrol )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "mp-a.3m",      0x0000, 0x1000, 0x5873a860 )
	ROM_LOAD( "mp-a.3l",      0x1000, 0x1000, 0xf4b85974 )
	ROM_LOAD( "mp-a.3k",      0x2000, 0x1000, 0x2e1a598c )
	ROM_LOAD( "mp-a.3j",      0x3000, 0x1000, 0xdd05b587 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for code */
	ROM_LOAD( "mp-snd.1a",    0xf000, 0x1000, 0x561d3108 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-e.3e",      0x0000, 0x1000, 0xe3ee7f75 )       /* chars */
	ROM_LOAD( "mp-e.3f",      0x1000, 0x1000, 0xcca6d023 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-b.3m",      0x0000, 0x1000, 0x707ace5e )       /* sprites */
	ROM_LOAD( "mp-b.3n",      0x1000, 0x1000, 0x9b72133a )

	ROM_REGION( 0x1000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-e.3l",      0x0000, 0x1000, 0xc46a7f72 )       /* background graphics */

	ROM_REGION( 0x1000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-e.3k",      0x0000, 0x1000, 0xc7aa1fb0 )

	ROM_REGION( 0x1000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-e.3h",      0x0000, 0x1000, 0xa0919392 )

	ROM_REGION( 0x0240, REGION_PROMS )
	ROM_LOAD( "2a",           0x0000, 0x0100, 0x0f193a50 ) /* character palette */
	ROM_LOAD( "1m",           0x0100, 0x0020, 0x6a57eff2 ) /* background palette */
	ROM_LOAD( "1c1j",         0x0120, 0x0020, 0x26979b13 ) /* sprite palette */
	ROM_LOAD( "2hx",          0x0140, 0x0100, 0x7ae4cd97 ) /* sprite lookup table */
ROM_END

ROM_START( mpatrolw )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "mpw-a.3m",     0x0000, 0x1000, 0xbaa1a1d4 )
	ROM_LOAD( "mpw-a.3l",     0x1000, 0x1000, 0x52459e51 )
	ROM_LOAD( "mpw-a.3k",     0x2000, 0x1000, 0x9b249fe5 )
	ROM_LOAD( "mpw-a.3j",     0x3000, 0x1000, 0xfee76972 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for code */
	ROM_LOAD( "mp-snd.1a",    0xf000, 0x1000, 0x561d3108 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mpw-e.3e",     0x0000, 0x1000, 0xf56e01fe )       /* chars */
	ROM_LOAD( "mpw-e.3f",     0x1000, 0x1000, 0xcaaba2d9 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-b.3m",      0x0000, 0x1000, 0x707ace5e )       /* sprites */
	ROM_LOAD( "mp-b.3n",      0x1000, 0x1000, 0x9b72133a )

	ROM_REGION( 0x1000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-e.3l",      0x0000, 0x1000, 0xc46a7f72 )       /* background graphics */

	ROM_REGION( 0x1000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-e.3k",      0x0000, 0x1000, 0xc7aa1fb0 )

	ROM_REGION( 0x1000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-e.3h",      0x0000, 0x1000, 0xa0919392 )

	ROM_REGION( 0x0240, REGION_PROMS )
	ROM_LOAD( "2a",           0x0000, 0x0100, 0x0f193a50 ) /* character palette */
	ROM_LOAD( "1m",           0x0100, 0x0020, 0x6a57eff2 ) /* background palette */
	ROM_LOAD( "1c1j",         0x0120, 0x0020, 0x26979b13 ) /* sprite palette */
	ROM_LOAD( "2hx",          0x0140, 0x0100, 0x7ae4cd97 ) /* sprite lookup table */
ROM_END

ROM_START( mranger )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "mp-a.3m",      0x0000, 0x1000, 0x5873a860 )
	ROM_LOAD( "mr-a.3l",      0x1000, 0x1000, 0x217dd431 )
	ROM_LOAD( "mr-a.3k",      0x2000, 0x1000, 0x9f0af7b2 )
	ROM_LOAD( "mr-a.3j",      0x3000, 0x1000, 0x7fe8e2cd )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for code */
	ROM_LOAD( "mp-snd.1a",    0xf000, 0x1000, 0x561d3108 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-e.3e",      0x0000, 0x1000, 0xe3ee7f75 )       /* chars */
	ROM_LOAD( "mp-e.3f",      0x1000, 0x1000, 0xcca6d023 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-b.3m",      0x0000, 0x1000, 0x707ace5e )       /* sprites */
	ROM_LOAD( "mp-b.3n",      0x1000, 0x1000, 0x9b72133a )

	ROM_REGION( 0x1000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-e.3l",      0x0000, 0x1000, 0xc46a7f72 )       /* background graphics */

	ROM_REGION( 0x1000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-e.3k",      0x0000, 0x1000, 0xc7aa1fb0 )

	ROM_REGION( 0x1000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mp-e.3h",      0x0000, 0x1000, 0xa0919392 )

	ROM_REGION( 0x0240, REGION_PROMS )
	ROM_LOAD( "2a",           0x0000, 0x0100, 0x0f193a50 ) /* character palette */
	ROM_LOAD( "1m",           0x0100, 0x0020, 0x6a57eff2 ) /* background palette */
	ROM_LOAD( "1c1j",         0x0120, 0x0020, 0x26979b13 ) /* sprite palette */
	ROM_LOAD( "2hx",          0x0140, 0x0100, 0x7ae4cd97 ) /* sprite lookup table */
ROM_END



GAME( 1982, mpatrol,  0,       mpatrol, mpatrol,  0, ROT0, "Irem", "Moon Patrol" )
GAME( 1982, mpatrolw, mpatrol, mpatrol, mpatrolw, 0, ROT0, "Irem (Williams license)", "Moon Patrol (Williams)" )
GAME( 1982, mranger,  mpatrol, mpatrol, mpatrol,  0, ROT0, "bootleg", "Moon Ranger" )
