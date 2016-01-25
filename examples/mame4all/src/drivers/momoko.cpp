#include "../vidhrdw/momoko.cpp"
/*****************************************************************************

Momoko 120% (c) 1986 Jaleco

	Driver by Uki

	02/Mar/2001 -

******************************************************************************

Notes

Real machine has some bugs.(escalator bug, sprite garbage)
It is not emulation bug.
Flipped screen looks wrong, but it is correct.

*****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern data8_t *momoko_bg_scrollx;
extern data8_t *momoko_bg_scrolly;

void momoko_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( momoko_fg_scrollx_w );
WRITE_HANDLER( momoko_fg_scrolly_w );
WRITE_HANDLER( momoko_text_scrolly_w );
WRITE_HANDLER( momoko_text_mode_w );
WRITE_HANDLER( momoko_bg_scrollx_w );
WRITE_HANDLER( momoko_bg_scrolly_w );
WRITE_HANDLER( momoko_flipscreen_w );
WRITE_HANDLER( momoko_fg_select_w);
WRITE_HANDLER( momoko_bg_select_w);
WRITE_HANDLER( momoko_bg_priority_w);

WRITE_HANDLER( momoko_bg_read_bank_w )
{
	data8_t *BG_MAP = memory_region(REGION_USER1);
	int bank_address = (data & 0x1f) * 0x1000;
	cpu_setbank(1, &BG_MAP[bank_address]);
}

/****************************************************************************/

static struct MemoryReadAddress readmem[] = {
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xcfff, MRA_RAM },

	{ 0xd064, 0xd0ff, MRA_RAM }, /* sprite ram */

	{ 0xd400, 0xd400, input_port_0_r },
	{ 0xd402, 0xd402, input_port_1_r },
	{ 0xd406, 0xd406, input_port_2_r },
	{ 0xd407, 0xd407, input_port_3_r },

	{ 0xd800, 0xdbff, paletteram_r },
	{ 0xe000, 0xe3ff, MRA_RAM }, /* text */

	{ 0xf000, 0xffff, MRA_BANK1 },
        { -1 } /* end of table */ };

static struct MemoryWriteAddress writemem[] = {
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM },

	{ 0xd064, 0xd0ff, MWA_RAM, &spriteram, &spriteram_size },

	{ 0xd400, 0xd400, MWA_NOP }, /* interrupt ack? */
	{ 0xd402, 0xd402, momoko_flipscreen_w },
	{ 0xd404, 0xd404, watchdog_reset_w },
	{ 0xd406, 0xd406, soundlatch_w },

	{ 0xd800, 0xdbff, paletteram_xxxxRRRRGGGGBBBB_swap_w, &paletteram },

	{ 0xdc00, 0xdc00, momoko_fg_scrolly_w },
	{ 0xdc01, 0xdc01, momoko_fg_scrollx_w },
	{ 0xdc02, 0xdc02, momoko_fg_select_w },

	{ 0xe000, 0xe3ff, videoram_w, &videoram, &videoram_size },

	{ 0xe800, 0xe800, momoko_text_scrolly_w },
	{ 0xe801, 0xe801, momoko_text_mode_w },

	{ 0xf000, 0xf001, momoko_bg_scrolly_w, &momoko_bg_scrolly },
	{ 0xf002, 0xf003, momoko_bg_scrollx_w, &momoko_bg_scrollx },
	{ 0xf004, 0xf004, momoko_bg_read_bank_w },
	{ 0xf006, 0xf006, momoko_bg_select_w },
	{ 0xf007, 0xf007, momoko_bg_priority_w },
        { -1 } /* end of table */ };

static struct MemoryReadAddress readmem_sound[] = {
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xa000, 0xa000, YM2203_status_port_0_r },
	{ 0xa001, 0xa001, YM2203_read_port_0_r },
	{ 0xc000, 0xc000, YM2203_status_port_1_r },
	{ 0xc001, 0xc001, YM2203_read_port_1_r },
        { -1 } /* end of table */ };

static struct MemoryWriteAddress writemem_sound[] = {
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x9000, 0x9000, MWA_NOP }, /* unknown */
	{ 0xa000, 0xa000, YM2203_control_port_0_w },
	{ 0xa001, 0xa001, YM2203_write_port_0_w },
	{ 0xb000, 0xb000, MWA_NOP }, /* unknown */
	{ 0xc000, 0xc000, YM2203_control_port_1_w },
	{ 0xc001, 0xc001, YM2203_write_port_1_w },
        { -1 } /* end of table */ };

/****************************************************************************/

INPUT_PORTS_START( momoko )
    PORT_START
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )

    PORT_START
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )

    PORT_START  /* dsw0 */
    PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
    PORT_DIPSETTING(    0x03, "3" )
    PORT_DIPSETTING(    0x02, "4" )
    PORT_DIPSETTING(    0x01, "5" )
    PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "256", IP_KEY_NONE, IP_JOY_NONE )
    PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Coinage ) )
    PORT_DIPSETTING(    0x10, DEF_STR( 5C_1C ) )
    PORT_DIPSETTING(    0x14, DEF_STR( 3C_1C ) )
    PORT_DIPSETTING(    0x18, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING(    0x1c, DEF_STR( 1C_1C ) )
    PORT_DIPSETTING(    0x0c, DEF_STR( 1C_2C ) )
    PORT_DIPSETTING(    0x04, DEF_STR( 2C_5C ) )
    PORT_DIPSETTING(    0x08, DEF_STR( 1C_5C ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
    PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
    PORT_DIPSETTING(    0x40, "Easy" )
    PORT_DIPSETTING(    0x60, "Normal" )
    PORT_DIPSETTING(    0x20, "Difficult" )
    PORT_DIPSETTING(    0x00, "Very difficult" )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

    PORT_START  /* dsw1 */
    PORT_DIPNAME( 0x03, 0x03, DEF_STR( Bonus_Life) )
    PORT_DIPSETTING(    0x01, "20000" )
    PORT_DIPSETTING(    0x03, "30000" )
    PORT_DIPSETTING(    0x02, "50000" )
    PORT_DIPSETTING(    0x00, "100000" )
    PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown) )
    PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown) )
    PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x10, 0x00, DEF_STR( Cabinet ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
    PORT_DIPSETTING(    0x10, DEF_STR( Cocktail ) )
    PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x20, DEF_STR( On ) )
    PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )

    PORT_START	/* fake */
    PORT_DIPNAME( 0x01, 0x00, DEF_STR( Flip_Screen ) )
    PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
    PORT_DIPSETTING(	0x01, DEF_STR( On ) )

INPUT_PORTS_END

/****************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	256,    /* 256 characters */
	2,      /* 2 bits per pixel */
	{4, 0},
	{0, 1, 2, 3, 256*8*8+0, 256*8*8+1, 256*8*8+2, 256*8*8+3},
	{8*0, 8*1, 8*2, 8*3, 8*4, 8*5, 8*6, 8*7},
	8*8
};

static struct GfxLayout spritelayout =
{
	8,16,     /* 8*16 characters */
	2048-128, /* 1024 sprites ( ccc 0ccccccc ) */
	4,        /* 4 bits per pixel */
	{12,8,4,0},
	{0, 1, 2, 3, 4096*8+0, 4096*8+1, 4096*8+2, 4096*8+3},
	{0, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
	 8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16},
	8*32
};

static struct GfxLayout tilelayout =
{
	8,8,      /* 8*8 characters */
	8192-256, /* 4096 tiles ( cccc0 cccccccc ) */
	4,        /* 4 bits per pixel */
	{4,0,12,8},
	{0, 1, 2, 3, 4096*8+0, 4096*8+1, 4096*8+2, 4096*8+3},
	{0, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16},
	8*16
};

static struct GfxLayout charlayout1 =
{
	8,1,    /* 8*1 characters */
	256*8,  /* 2048 characters */
	2,      /* 2 bits per pixel */
	{4, 0},
	{0, 1, 2, 3, 256*8*8+0, 256*8*8+1, 256*8*8+2, 256*8*8+3},
	{8*0},
	8*1
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout1,      0,  24 }, /* TEXT */
	{ REGION_GFX2, 0x0000, &tilelayout,     256,  16 }, /* BG */
	{ REGION_GFX3, 0x0000, &charlayout,       0,   1 }, /* FG */
	{ REGION_GFX4, 0x0000, &spritelayout,   128,   8 }, /* sprite */
	{ -1 } /* end of array */
};

/****************************************************************************/

static struct YM2203interface ym2203_interface =
{
	2,          /* 2 chips */
	1250000,    /* 1.25 MHz */
	{ YM2203_VOL(40,15), YM2203_VOL(40,15) },
	{ 0, soundlatch_r },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct MachineDriver machine_driver_momoko =
{
	{
		{
			CPU_Z80,
			5000000,	/* 5.0MHz */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			2500000,	/* 2.5MHz */
			readmem_sound,writemem_sound,0,0,
			ignore_interrupt,0
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	0,

	32*8, 32*8, { 1*8, 31*8-1, 2*8, 29*8-1 },

	gfxdecodeinfo,
	512,512,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE ,
	0,
	generic_vh_start,
	generic_vh_stop,
	momoko_vh_screenrefresh,

	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};

/****************************************************************************/

ROM_START( momoko )
	ROM_REGION( 0x10000, REGION_CPU1) /* main CPU */
	ROM_LOAD( "momoko03.bin", 0x0000,  0x8000, 0x386e26ed )
	ROM_LOAD( "momoko02.bin", 0x8000,  0x4000, 0x4255e351 )

	ROM_REGION( 0x10000, REGION_CPU2) /* sound CPU */
	ROM_LOAD( "momoko01.bin", 0x0000,  0x8000, 0xe8a6673c )

	ROM_REGION( 0x2000, REGION_GFX1| REGIONFLAG_DISPOSE ) /* text */
	ROM_LOAD( "momoko13.bin", 0x0000,  0x2000, 0x2745cf5a )

	ROM_REGION( 0x2000, REGION_GFX3| REGIONFLAG_DISPOSE ) /* FG */
	ROM_LOAD( "momoko14.bin", 0x0000,  0x2000, 0xcfccca05 )

	ROM_REGION( 0x10000, REGION_GFX4| REGIONFLAG_DISPOSE ) /* sprite */
	ROM_LOAD_ODD( "momoko16.bin", 0x0000,  0x8000, 0xfc6876fc )
	ROM_LOAD_EVEN( "momoko17.bin", 0x0001,  0x8000, 0x45dc0247 )

	ROM_REGION( 0x20000, REGION_GFX2) /* BG */
	ROM_LOAD_ODD( "momoko09.bin", 0x00000, 0x8000, 0x9f5847c7 )
	ROM_LOAD_EVEN( "momoko11.bin", 0x00001, 0x8000, 0x9c9fbd43 )
	ROM_LOAD_ODD( "momoko10.bin", 0x10000, 0x8000, 0xae17e74b )
	ROM_LOAD_EVEN( "momoko12.bin", 0x10001, 0x8000, 0x1e29c9c4 )

	ROM_REGION( 0x20000, REGION_USER1) /* BG map */
	ROM_LOAD( "momoko04.bin", 0x0000,  0x8000, 0x3ab3c2c3 )
	ROM_LOAD( "momoko05.bin", 0x8000,  0x8000, 0x757cdd2b )
	ROM_LOAD( "momoko06.bin", 0x10000, 0x8000, 0x20cacf8b )
	ROM_LOAD( "momoko07.bin", 0x18000, 0x8000, 0xb94b38db )

	ROM_REGION( 0x2000, REGION_USER2) /* BG color/priority table */
	ROM_LOAD( "momoko08.bin", 0x0000,  0x2000, 0x69b41702 )

	ROM_REGION( 0x4000, REGION_USER3) /* FG map */
	ROM_LOAD( "momoko15.bin", 0x0000,  0x4000, 0x8028f806 )

	ROM_REGION( 0x0120, REGION_PROMS) /* TEXT color */
	ROM_LOAD( "momoko-c.bin", 0x0000,  0x0100, 0xf35ccae0 )
	ROM_LOAD( "momoko-b.bin", 0x0100,  0x0020, 0x427b0e5c )
ROM_END

GAME( 1986, momoko, 0, momoko, momoko, 0, ROT0, "Jaleco", "Momoko 120%" )
