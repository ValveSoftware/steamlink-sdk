/***************************************************************************

Eggs

Very similar to Burger Time hardware (and uses its video driver)

driver by Nicola Salmoria

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


/* from vidhrdw/btime.c */
void btime_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int  btime_vh_start (void);
void eggs_vh_screenrefresh    (struct osd_bitmap *bitmap,int full_refresh);

READ_HANDLER( btime_mirrorvideoram_r );
WRITE_HANDLER( btime_mirrorvideoram_w );
READ_HANDLER( btime_mirrorcolorram_r );
WRITE_HANDLER( btime_mirrorcolorram_w );
WRITE_HANDLER( btime_video_control_w );



static struct MemoryReadAddress eggs_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x1000, 0x17ff, MRA_RAM },
	{ 0x1800, 0x1bff, btime_mirrorvideoram_r },
	{ 0x1c00, 0x1fff, btime_mirrorcolorram_r },
	{ 0x2000, 0x2000, input_port_2_r },     /* DSW1 */
	{ 0x2001, 0x2001, input_port_3_r },     /* DSW2 */
	{ 0x2002, 0x2002, input_port_0_r },     /* IN0 */
	{ 0x2003, 0x2003, input_port_1_r },     /* IN1 */
	{ 0x3000, 0x7fff, MRA_ROM },
	{ 0xf000, 0xffff, MRA_ROM },    /* reset/interrupt vectors */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress eggs_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x1000, 0x13ff, videoram_w, &videoram, &videoram_size },
	{ 0x1400, 0x17ff, colorram_w, &colorram },
	{ 0x1800, 0x1bff, btime_mirrorvideoram_w },
	{ 0x1c00, 0x1fff, btime_mirrorcolorram_w },
	{ 0x2000, 0x2000, btime_video_control_w },
	{ 0x2001, 0x2001, MWA_NOP },
	{ 0x2004, 0x2004, AY8910_control_port_0_w },
	{ 0x2005, 0x2005, AY8910_write_port_0_w },
	{ 0x2006, 0x2006, AY8910_control_port_1_w },
	{ 0x2007, 0x2007, AY8910_write_port_1_w },
	{ 0x3000, 0x7fff, MWA_ROM },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( scregg )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_BIT( 0x30, 0x30, IPT_UNKNOWN )     /* almost certainly unused */
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK  )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x06, 0x04, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x04, "30000" )
	PORT_DIPSETTING(    0x02, "50000" )
	PORT_DIPSETTING(    0x00, "70000"  )
	PORT_DIPSETTING(    0x06, "Never"  )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )   /* almost certainly unused */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )   /* almost certainly unused */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )   /* almost certainly unused */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )   /* almost certainly unused */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x80, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
INPUT_PORTS_END




static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	1024,   /* 1024 characters */
	3,      /* 3 bits per pixel */
	{ 2*1024*8*8, 1024*8*8, 0 },    /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	256,    /* 256 sprites */
	3,      /* 3 bits per pixel */
	{ 2*256*16*16, 256*16*16, 0 },  /* the bitplanes are separated */
	{ 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
	  0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8    /* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,          0, 1 },     /* char set #1 */
	{ REGION_GFX1, 0, &spritelayout,        0, 1 },     /* sprites */
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	2,      /* 2 chips */
	1500000,        /* 1.5 MHz ? (hand tuned) */
	{ 23, 23 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_scregg =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1500000,
			eggs_readmem,eggs_writemem,0,0,
			interrupt,1
		}
	},
	57, 3072,        /* frames per second, vblank duration taken from Burger Time */
	1,      /* single CPU, no need from interleaving  */
	0,

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo,
	8, 8,
	btime_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_MODIFIES_PALETTE,
	0,
	btime_vh_start,
	generic_vh_stop,
	eggs_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};



ROM_START( scregg )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "scregg.e14",   0x3000, 0x1000, 0x29226d77 )
	ROM_LOAD( "scregg.d14",   0x4000, 0x1000, 0xeb143880 )
	ROM_LOAD( "scregg.c14",   0x5000, 0x1000, 0x4455f262 )
	ROM_LOAD( "scregg.b14",   0x6000, 0x1000, 0x044ac5d2 )
	ROM_LOAD( "scregg.a14",   0x7000, 0x1000, 0xb5a0814a )
	ROM_RELOAD(               0xf000, 0x1000 )        /* for reset/interrupt vectors */

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "scregg.j12",   0x0000, 0x1000, 0xa485c10c )
	ROM_LOAD( "scregg.j10",   0x1000, 0x1000, 0x1fd4e539 )
	ROM_LOAD( "scregg.h12",   0x2000, 0x1000, 0x8454f4b2 )
	ROM_LOAD( "scregg.h10",   0x3000, 0x1000, 0x72bd89ee )
	ROM_LOAD( "scregg.g12",   0x4000, 0x1000, 0xff3c2894 )
	ROM_LOAD( "scregg.g10",   0x5000, 0x1000, 0x9c20214a )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "screggco.c6",  0x0000, 0x0020, 0xff23bdd6 )	/* palette */
	ROM_LOAD( "screggco.b4",  0x0020, 0x0020, 0x7cc4824b )	/* unknown */
ROM_END

ROM_START( eggs )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "e14.bin",      0x3000, 0x1000, 0x4e216f9d )
	ROM_LOAD( "d14.bin",      0x4000, 0x1000, 0x4edb267f )
	ROM_LOAD( "c14.bin",      0x5000, 0x1000, 0x15a5c48c )
	ROM_LOAD( "b14.bin",      0x6000, 0x1000, 0x5c11c00e )
	ROM_LOAD( "a14.bin",      0x7000, 0x1000, 0x953faf07 )
	ROM_RELOAD(               0xf000, 0x1000 )   /* for reset/interrupt vectors */

	ROM_REGION( 0x6000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "j12.bin",      0x0000, 0x1000, 0xce4a2e46 )
	ROM_LOAD( "j10.bin",      0x1000, 0x1000, 0xa1bcaffc )
	ROM_LOAD( "h12.bin",      0x2000, 0x1000, 0x9562836d )
	ROM_LOAD( "h10.bin",      0x3000, 0x1000, 0x3cfb3a8e )
	ROM_LOAD( "g12.bin",      0x4000, 0x1000, 0x679f8af7 )
	ROM_LOAD( "g10.bin",      0x5000, 0x1000, 0x5b58d3b5 )

	ROM_REGION( 0x0040, REGION_PROMS )
	ROM_LOAD( "eggs.c6",      0x0000, 0x0020, 0xe8408c81 )	/* palette */
	ROM_LOAD( "screggco.b4",  0x0020, 0x0020, 0x7cc4824b )	/* unknown */
ROM_END



GAME( 1983, scregg, 0,      scregg, scregg, 0, ROT270, "Technos", "Scrambled Egg" )
GAME( 1983, eggs,   scregg, scregg, scregg, 0, ROT270, "[Technos] Universal USA", "Eggs" )
