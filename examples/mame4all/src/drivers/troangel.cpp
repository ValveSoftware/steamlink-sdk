#include "../vidhrdw/troangel.cpp"

/****************************************************************************

Tropical Angel

driver by Phil Stroffolino

****************************************************************************/
#include "driver.h"
#include "sndhrdw/irem.h"
#include "vidhrdw/generic.h"

extern unsigned char *troangel_scroll;
WRITE_HANDLER( troangel_flipscreen_w );
void troangel_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void troangel_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



static struct MemoryReadAddress troangel_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_RAM },
	{ 0x9000, 0x90ff, MRA_RAM },
	{ 0xd000, 0xd000, input_port_0_r },
	{ 0xd001, 0xd001, input_port_1_r },
	{ 0xd002, 0xd002, input_port_2_r },
	{ 0xd003, 0xd003, input_port_3_r },
	{ 0xd004, 0xd004, input_port_4_r },
	{ 0xe000, 0xe7ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress troangel_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, videoram_w, &videoram, &videoram_size },
//	{ 0x8800, 0x8fff, MWA_RAM },
	{ 0x9000, 0x91ff, MWA_RAM, &troangel_scroll },
	{ 0xc820, 0xc8ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd000, 0xd000, irem_sound_cmd_w },
	{ 0xd001, 0xd001, troangel_flipscreen_w },	/* + coin counters */
	{ 0xe000, 0xe7ff, MWA_RAM },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( troangel )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	/* coin input must be active for 19 frames to be consistently recognized */
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN3, 19 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Time" )
	PORT_DIPSETTING(    0x03, "180 160 140" )
	PORT_DIPSETTING(    0x02, "160 140 120" )
	PORT_DIPSETTING(    0x01, "140 120 100" )
	PORT_DIPSETTING(    0x00, "120 100 100" )
	PORT_DIPNAME( 0x04, 0x04, "Crash Loss Time" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_DIPSETTING(    0x00, "10" )
	PORT_DIPNAME( 0x08, 0x08, "Background Sound" )
	PORT_DIPSETTING(    0x08, "Boat Motor" )
	PORT_DIPSETTING(    0x00, "Music" )
	/* TODO: support the different settings which happen in Coin Mode 2 */
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coinage ) ) /* mapped on coin mode 1 */
	PORT_DIPSETTING(    0xa0, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* settings 0x10, 0x20, 0x80, 0x90 all give 1 Coin/1 Credit */

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
/* This activates a different coin mode. Look at the dip switch setting schematic */
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
/* TODO: the following enables an analog accelerator input read from 0xd003 */
/* however that is the DSW1 input so it must be multiplexed some way */
	PORT_DIPNAME( 0x08, 0x08, "Analog Accelarator" )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8, /* character size */
	1024, /* number of characters */
	3, /* bits per pixel */
	{ 0, 1024*8*8, 2*1024*8*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* character offset */
};

static struct GfxLayout spritelayout =
{
	16,32, /* sprite size */
	64, /* number of sprites */
	3, /* bits per pixel */
	{ 0, 0x4000*8, 2*0x4000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8,
			256*64+0*8, 256*64+1*8, 256*64+2*8, 256*64+3*8, 256*64+4*8, 256*64+5*8, 256*64+6*8, 256*64+7*8,
			256*64+8*8, 256*64+9*8, 256*64+10*8, 256*64+11*8, 256*64+12*8, 256*64+13*8, 256*64+14*8, 256*64+15*8 },
	32*8	/* character offset */
};

static struct GfxDecodeInfo troangel_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,      0, 32 },
	{ REGION_GFX2, 0x0000, &spritelayout, 32*8, 32 },
	{ REGION_GFX2, 0x1000, &spritelayout, 32*8, 32 },
	{ REGION_GFX2, 0x2000, &spritelayout, 32*8, 32 },
	{ REGION_GFX2, 0x3000, &spritelayout, 32*8, 32 },
	{ -1 } /* end of array */
};



static struct MachineDriver machine_driver_troangel =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3000000,	/* 3 Mhz ??? */
			troangel_readmem,troangel_writemem,
			0,0,
			interrupt,1
		},
		IREM_AUDIO_CPU
	},
	57, 1790,	/* accurate frequency, measured on a Moon Patrol board, is 56.75Hz. */
				/* the Lode Runner manual (similar but different hardware) */
				/* talks about 55Hz and 1790ms vblank duration. */
	1, /* cpu slices */
	0, /* init machine */

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 1*8, 31*8-1 },
	troangel_gfxdecodeinfo,
	32*8+16,32*8+32*8,
	troangel_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	troangel_vh_screenrefresh,


	/* sound hardware */
	0,0,0,0,
	{
		IREM_AUDIO
	}
};




ROM_START( troangel )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* main CPU */
	ROM_LOAD( "ta-a-3k",	0x0000, 0x2000, 0xf21f8196 )
	ROM_LOAD( "ta-a-3m",	0x2000, 0x2000, 0x58801e55 )
	ROM_LOAD( "ta-a-3n",	0x4000, 0x2000, 0xde3dea44 )
	ROM_LOAD( "ta-a-3q",	0x6000, 0x2000, 0xfff0fc2a )

	ROM_REGION(  0x10000 , REGION_CPU2 )	/* sound CPU */
	ROM_LOAD( "ta-s-1a",	0xe000, 0x2000, 0x15a83210 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ta-a-3c",	0x00000, 0x2000, 0x7ff5482f )	/* characters */
	ROM_LOAD( "ta-a-3d",	0x02000, 0x2000, 0x06eef241 )
	ROM_LOAD( "ta-a-3e",	0x04000, 0x2000, 0xe49f7ad8 )

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ta-b-5j",	0x00000, 0x2000, 0x86895c0c )	/* sprites */
	ROM_LOAD( "ta-b-5h",	0x02000, 0x2000, 0xf8cff29d )
	ROM_LOAD( "ta-b-5e",	0x04000, 0x2000, 0x8b21ee9a )
	ROM_LOAD( "ta-b-5d",	0x06000, 0x2000, 0xcd473d47 )
	ROM_LOAD( "ta-b-5c",	0x08000, 0x2000, 0xc19134c9 )
	ROM_LOAD( "ta-b-5a",	0x0a000, 0x2000, 0x0012792a )

	ROM_REGION( 0x0320, REGION_PROMS )
	ROM_LOAD( "ta-a-5a",	0x0000,	0x0100, 0x01de1167 ) /* chars palette low 4 bits */
	ROM_LOAD( "ta-a-5b",	0x0100,	0x0100, 0xefd11d4b ) /* chars palette high 4 bits */
	ROM_LOAD( "ta-b-1b",	0x0200, 0x0020, 0xf94911ea ) /* sprites palette */
	ROM_LOAD( "ta-b-3d",	0x0220,	0x0100, 0xed3e2aa4 ) /* sprites lookup table */
ROM_END



GAME( 1983, troangel, 0, troangel, troangel, 0, ROT0, "Irem", "Tropical Angel" )
