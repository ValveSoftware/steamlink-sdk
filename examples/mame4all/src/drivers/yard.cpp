#include "../vidhrdw/yard.cpp"

/****************************************************************************
10 Yard Fight Driver.

L Taylor
J Clegg

Loosely based on the Kung Fu Master driver.

****************************************************************************/

#include "driver.h"
#include "sndhrdw/irem.h"
#include "vidhrdw/generic.h"


extern unsigned char *yard_scroll_x_low;
extern unsigned char *yard_scroll_x_high;
extern unsigned char *yard_scroll_y_low;
extern unsigned char *yard_score_panel_disabled;

void yard_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int yard_vh_start(void);
void yard_vh_stop(void);
WRITE_HANDLER( yard_flipscreen_w );
WRITE_HANDLER( yard_scroll_panel_w );
void yard_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



READ_HANDLER( mpatrol_input_port_3_r );



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_RAM },        /* Video and Color ram */
	{ 0xd000, 0xd000, input_port_0_r },	        /* IN0 */
	{ 0xd001, 0xd001, input_port_1_r },	        /* IN1 */
	{ 0xd002, 0xd002, input_port_2_r },	        /* IN2 */
	{ 0xd003, 0xd003, mpatrol_input_port_3_r },	/* DSW1 */
	{ 0xd004, 0xd004, input_port_4_r },	        /* DSW2 */
	{ 0xe000, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x8000, 0x8fff, videoram_w, &videoram, &videoram_size },
	{ 0x9000, 0x9fff, yard_scroll_panel_w },
	{ 0xc820, 0xc87f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xa000, 0xa000, MWA_RAM, &yard_scroll_x_low },
	{ 0xa200, 0xa200, MWA_RAM, &yard_scroll_x_high },
	{ 0xa400, 0xa400, MWA_RAM, &yard_scroll_y_low },
	{ 0xa800, 0xa800, MWA_RAM, &yard_score_panel_disabled },
	{ 0xd000, 0xd000, irem_sound_cmd_w },
	{ 0xd001, 0xd001, yard_flipscreen_w },	/* + coin counters */
	{ 0xe000, 0xefff, MWA_RAM },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( yard )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	/* coin input must be active for 19 frames to be consistently recognized */
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN3, 19 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "SW1A" )
	PORT_DIPSETTING( 0x01, DEF_STR( Off ) )
	PORT_DIPSETTING( 0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "SW2A" )
	PORT_DIPSETTING( 0x02, DEF_STR( Off ) )
	PORT_DIPSETTING( 0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Time Reduced" )
	PORT_DIPSETTING( 0x0c, "Normal" )
	PORT_DIPSETTING( 0x08, "x 1.3" )
	PORT_DIPSETTING( 0x04, "x 1.5" )
	PORT_DIPSETTING( 0x00, "x 1.8" )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )  /* Gets filled in based on the coin mode */

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
	PORT_BITX(    0x08, 0x08, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Slow Motion", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "SW6B" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	/* Fake port to support the two different coin modes */
	PORT_START
	PORT_DIPNAME( 0x0f, 0x0f, "Coinage Mode 1" ) /* mapped on coin mode 1 */
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
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* settings 0x10, 0x20, 0x80, 0x90 all give 1 Coin/1 Credit */
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

/* exactly the same as yard, only difference is the Allow Continue dip switch */
/* Also, the Cabinet dip switch doesn't seem to work. */
INPUT_PORTS_START( vsyard )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	/* coin input must be active for 19 frames to be consistently recognized */
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN3, 19 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, "Continue (Vs. Mode Only)" )
	PORT_DIPSETTING( 0x01, DEF_STR( No ) )
	PORT_DIPSETTING( 0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x02, 0x02, "SW2A" )
	PORT_DIPSETTING( 0x02, DEF_STR( Off ) )
	PORT_DIPSETTING( 0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Time Reduced" )
	PORT_DIPSETTING( 0x0c, "Normal" )
	PORT_DIPSETTING( 0x08, "x 1.3" )
	PORT_DIPSETTING( 0x04, "x 1.5" )
	PORT_DIPSETTING( 0x00, "x 1.8" )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED )  /* Gets filled in based on the coin mode */

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Cabinet?" )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
/* This activates a different coin mode. Look at the dip switch setting schematic */
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_BITX(    0x08, 0x08, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Slow Motion", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "SW6B" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	/* Fake port to support the two different coin modes */
	PORT_START
	PORT_DIPNAME( 0x0f, 0x0f, "Coinage Mode 1" ) /* mapped on coin mode 1 */
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
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* settings 0x10, 0x20, 0x80, 0x90 all give 1 Coin/1 Credit */
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
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	3,	/* 3 bits per pixel */
	{ 2*1024*8*8, 1024*8*8, 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 8*0, 8*1, 8*2, 8*3, 8*4, 8*5, 8*6, 8*7 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	512,	/* 256 sprites */
	3,	/* 3 bits per pixel */
	{ 2*0x4000*8, 0x4000*8, 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7},
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,       0, 32 },	/* use colors 0-255 */
	{ REGION_GFX2, 0, &spritelayout,  32*8, 32 },	/* use colors 256-271 with lookup table */
	/* bitmapped radar uses colors 272-527 */
	{ -1 } /* end of array */
};



static struct MachineDriver machine_driver_yard =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4 Mhz (?) */
			readmem,writemem,0,0,
			interrupt,1
		},
		IREM_AUDIO_CPU
	},
	57, 1790,	/* accurate frequency, measured on a Moon Patrol board, is 56.75Hz. */
				/* the Lode Runner manual (similar but different hardware) */
				/* talks about 55Hz and 1790ms vblank duration. */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo,
	256+16+256, 32*8+32*8,
	yard_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	yard_vh_start,
	yard_vh_stop,
	yard_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		IREM_AUDIO
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/
ROM_START( yard )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "yf-a-3p",      0x0000, 0x2000, 0x4586114f )
	ROM_LOAD( "yf-a-3n",      0x2000, 0x2000, 0x947fa760 )
	ROM_LOAD( "yf-a-3m",      0x4000, 0x2000, 0xd4975633 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu */
	ROM_LOAD( "yf-s-3b",      0x8000, 0x2000, 0x0392a60c )
	ROM_LOAD( "yf-s-1b",      0xa000, 0x2000, 0x6588f41a )
	ROM_LOAD( "yf-s-3a",      0xc000, 0x2000, 0xbd054e44 )
	ROM_LOAD( "yf-s-1a",      0xe000, 0x2000, 0x2490d4c3 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "yf-a-3e",      0x00000, 0x2000, 0x77e9e9cc )	/* chars */
	ROM_LOAD( "yf-a-3d",      0x02000, 0x2000, 0x854d5ff4 )
	ROM_LOAD( "yf-a-3c",      0x04000, 0x2000, 0x0cd8ffad )

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "yf-b-5b",      0x00000, 0x2000, 0x1299ae30 )	/* sprites */
	ROM_LOAD( "yf-b-5c",      0x02000, 0x2000, 0x8708b888 )
	ROM_LOAD( "yf-b-5f",      0x04000, 0x2000, 0xd9bb8ab8 )
	ROM_LOAD( "yf-b-5e",      0x06000, 0x2000, 0x47077e8d )
	ROM_LOAD( "yf-b-5j",      0x08000, 0x2000, 0x713ef31f )
	ROM_LOAD( "yf-b-5k",      0x0a000, 0x2000, 0xf49651cc )

	ROM_REGION( 0x0520, REGION_PROMS )
	ROM_LOAD( "yard.1c",      0x0000, 0x0100, 0x08fa5103 ) /* chars palette low 4 bits */
	ROM_LOAD( "yard.1d",      0x0100, 0x0100, 0x7c04994c ) /* chars palette high 4 bits */
	ROM_LOAD( "yard.1f",      0x0200, 0x0020, 0xb8554da5 ) /* sprites palette */
	ROM_LOAD( "yard.2h",      0x0220, 0x0100, 0xe1cdfb06 ) /* sprites lookup table */
	ROM_LOAD( "yard.2n",      0x0320, 0x0100, 0xcd85b646 ) /* radar palette low 4 bits */
	ROM_LOAD( "yard.2m",      0x0420, 0x0100, 0x45384397 ) /* radar palette high 4 bits */
ROM_END

ROM_START( vsyard )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a-3p",         0x0000, 0x2000, 0x1edac08f )
	ROM_LOAD( "vyf-a-3m",     0x2000, 0x2000, 0x3b9330f8 )
	ROM_LOAD( "a-3m",         0x4000, 0x2000, 0xcf783dad )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu */
	ROM_LOAD( "yf-s-3b",      0x8000, 0x2000, 0x0392a60c )
	ROM_LOAD( "yf-s-1b",      0xa000, 0x2000, 0x6588f41a )
	ROM_LOAD( "yf-s-3a",      0xc000, 0x2000, 0xbd054e44 )
	ROM_LOAD( "yf-s-1a",      0xe000, 0x2000, 0x2490d4c3 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vyf-a-3a",     0x00000, 0x2000, 0x354d7330 )	/* chars */
	ROM_LOAD( "vyf-a-3c",     0x02000, 0x2000, 0xf48eedca )
	ROM_LOAD( "vyf-a-3d",     0x04000, 0x2000, 0x7d1b4d93 )

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "yf-b-5b",      0x00000, 0x2000, 0x1299ae30 )	/* sprites */
	ROM_LOAD( "yf-b-5c",      0x02000, 0x2000, 0x8708b888 )
	ROM_LOAD( "yf-b-5f",      0x04000, 0x2000, 0xd9bb8ab8 )
	ROM_LOAD( "yf-b-5e",      0x06000, 0x2000, 0x47077e8d )
	ROM_LOAD( "yf-b-5j",      0x08000, 0x2000, 0x713ef31f )
	ROM_LOAD( "yf-b-5k",      0x0a000, 0x2000, 0xf49651cc )

	ROM_REGION( 0x0520, REGION_PROMS )
	ROM_LOAD( "yard.1c",      0x0000, 0x0100, 0x08fa5103 ) /* chars palette low 4 bits */
	ROM_LOAD( "yard.1d",      0x0100, 0x0100, 0x7c04994c ) /* chars palette high 4 bits */
	ROM_LOAD( "yard.1f",      0x0200, 0x0020, 0xb8554da5 ) /* sprites palette */
	ROM_LOAD( "yard.2h",      0x0220, 0x0100, 0xe1cdfb06 ) /* sprites lookup table */
	ROM_LOAD( "yard.2n",      0x0320, 0x0100, 0xcd85b646 ) /* radar palette low 4 bits */
	ROM_LOAD( "yard.2m",      0x0420, 0x0100, 0x45384397 ) /* radar palette high 4 bits */
ROM_END

ROM_START( vsyard2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "vyf-a-3n",     0x0000, 0x2000, 0x418e01fc )
	ROM_LOAD( "vyf-a-3m",     0x2000, 0x2000, 0x3b9330f8 )
	ROM_LOAD( "vyf-a-3k",     0x4000, 0x2000, 0xa0ec15bb )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu */
	ROM_LOAD( "yf-s-3b",      0x8000, 0x2000, 0x0392a60c )
	ROM_LOAD( "yf-s-1b",      0xa000, 0x2000, 0x6588f41a )
	ROM_LOAD( "yf-s-3a",      0xc000, 0x2000, 0xbd054e44 )
	ROM_LOAD( "yf-s-1a",      0xe000, 0x2000, 0x2490d4c3 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vyf-a-3a",     0x00000, 0x2000, 0x354d7330 )	/* chars */
	ROM_LOAD( "vyf-a-3c",     0x02000, 0x2000, 0xf48eedca )
	ROM_LOAD( "vyf-a-3d",     0x04000, 0x2000, 0x7d1b4d93 )

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "yf-b-5b",      0x00000, 0x2000, 0x1299ae30 )	/* sprites */
	ROM_LOAD( "yf-b-5c",      0x02000, 0x2000, 0x8708b888 )
	ROM_LOAD( "yf-b-5f",      0x04000, 0x2000, 0xd9bb8ab8 )
	ROM_LOAD( "yf-b-5e",      0x06000, 0x2000, 0x47077e8d )
	ROM_LOAD( "yf-b-5j",      0x08000, 0x2000, 0x713ef31f )
	ROM_LOAD( "yf-b-5k",      0x0a000, 0x2000, 0xf49651cc )

	ROM_REGION( 0x0520, REGION_PROMS )
	ROM_LOAD( "yard.1c",      0x0000, 0x0100, 0x08fa5103 ) /* chars palette low 4 bits */
	ROM_LOAD( "yard.1d",      0x0100, 0x0100, 0x7c04994c ) /* chars palette high 4 bits */
	ROM_LOAD( "yard.1f",      0x0200, 0x0020, 0xb8554da5 ) /* sprites palette */
	ROM_LOAD( "yard.2h",      0x0220, 0x0100, 0xe1cdfb06 ) /* sprites lookup table */
	ROM_LOAD( "yard.2n",      0x0320, 0x0100, 0xcd85b646 ) /* radar palette low 4 bits */
	ROM_LOAD( "yard.2m",      0x0420, 0x0100, 0x45384397 ) /* radar palette high 4 bits */
ROM_END



GAME( 1983, yard,    0,    yard, yard,   0, ROT0, "Irem", "10 Yard Fight" )
GAME( 1984, vsyard,  yard, yard, vsyard, 0, ROT0, "Irem", "10 Yard Fight (Vs. version 11/05/84)" )
GAME( 1984, vsyard2, yard, yard, vsyard, 0, ROT0, "Irem", "10 Yard Fight (Vs. version, set 2)" )
