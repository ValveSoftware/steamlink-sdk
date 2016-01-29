#include "../vidhrdw/lasso.cpp"

/*	Lasso
**	(C)1982 SNK
**
**	input port issues:
**	- fire button auto-repeats on high score entry screen (real behavior?)
**
**	unknown CPU speeds (effects game timing)
*/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"

WRITE_HANDLER( lasso_videoram_w );
WRITE_HANDLER( lasso_backcolor_w );
WRITE_HANDLER( lasso_cocktail_w );
void lasso_vh_convert_color_prom( UINT8 *palette, UINT16 *colortable, const UINT8 *color_prom );
void lasso_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh );
int lasso_vh_start( void );

extern unsigned char *lasso_vram; /* 0x2000 bytes for a 256x256x1 bitmap */



int lasso_interrupt( void )
{
	if (cpu_getiloops() == 0)
		return interrupt();
	else
		return nmi_interrupt(); // coin input
}


static UINT8 *shareram;

static READ_HANDLER( shareram_r )
{
	return shareram[offset];
}

static WRITE_HANDLER( shareram_w )
{
	shareram[offset] = data;
}

static WRITE_HANDLER( lasso_sound_command_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt( 2, Z80_IRQ_INT ); /* ? */
}

static READ_HANDLER( sound_status_r )
{
	/*	0x01: chip#0 ready; 0x02: chip#1 ready */
	return 0x03;
}

static int lasso_chip_data;

static WRITE_HANDLER( sound_data_w )
{
	lasso_chip_data =
			((data & 0x80) >> 7) |
			((data & 0x40) >> 5) |
			((data & 0x20) >> 3) |
			((data & 0x10) >> 1) |
			((data & 0x08) << 1) |
			((data & 0x04) << 3) |
			((data & 0x02) << 5) |
			((data & 0x01) << 7);
}

static WRITE_HANDLER( sound_select_w )
{
	if (~data & 0x01)	/* chip #0 */
		SN76496_0_w(0,lasso_chip_data);
	if (~data & 0x02)	/* chip #1 */
		SN76496_1_w(0,lasso_chip_data);
}



INPUT_PORTS_START( lasso )
	PORT_START /* 1804 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 ) /* lasso */
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 ) /* shoot */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START /* 1805 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_COCKTAIL | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_COCKTAIL | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_COCKTAIL | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_COCKTAIL | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2	| IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START /* 1806 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x0e, 0x0e, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x0e, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x0c, DEF_STR( 1C_6C ) )
//	PORT_DIPSETTING(	0x06, DEF_STR( 1C_1C ) )
//	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
//	PORT_DIPSETTING(	0x0a, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
//	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Show Instructions" )
	PORT_DIPSETTING(	0x00, DEF_STR( No ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Yes ) )

	PORT_START /* 1807 */
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START1 )
INPUT_PORTS_END




/* 17f0 on CPU1 maps to 07f0 on CPU2 */
static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM }, /* work ram */
	{ 0x0400, 0x0bff, MRA_RAM }, /* videoram */
	{ 0x0c00, 0x0c7f, MRA_RAM }, /* spriteram */
	{ 0x1000, 0x17ff, shareram_r },
	{ 0x1804, 0x1804, input_port_0_r },
	{ 0x1805, 0x1805, input_port_1_r },
	{ 0x1806, 0x1806, input_port_2_r },
	{ 0x1807, 0x1807, input_port_3_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0400, 0x0bff, lasso_videoram_w, &videoram },
	{ 0x0c00, 0x0c7f, MWA_RAM, &spriteram },
	{ 0x1000, 0x17ff, shareram_w },
	{ 0x1800, 0x1800, lasso_sound_command_w },
	{ 0x1801, 0x1801, lasso_backcolor_w },
	{ 0x1802, 0x1802, lasso_cocktail_w },
	{ 0x1806, 0x1806, MWA_NOP }, /* spurious write */
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }
};

static struct MemoryReadAddress readmem_coprocessor[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },	/* shared RAM */
	{ 0x2000, 0x3fff, MRA_RAM },
	{ 0x8000, 0x8fff, MRA_ROM },
	{ 0xf000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress writemem_coprocessor[] =
{
	{ 0x0000, 0x07ff, MWA_RAM, &shareram },	/* code is executed from here */
	{ 0x2000, 0x3fff, MWA_RAM, &lasso_vram },
	{ 0x8000, 0x8fff, MWA_ROM },
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 }
};

static struct MemoryReadAddress readmem_sound[] =
{
	{ 0x0000, 0x01ff, MRA_RAM },
	{ 0x5000, 0x7fff, MRA_ROM },
	{ 0xb004, 0xb004, sound_status_r },
	{ 0xb005, 0xb005, soundlatch_r },
	{ 0xf000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress writemem_sound[] =
{
	{ 0x0000, 0x01ff, MWA_RAM },
	{ 0x5000, 0x7fff, MWA_ROM },
	{ 0xb000, 0xb000, sound_data_w },
	{ 0xb001, 0xb001, sound_select_w },
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 }
};



static struct GfxLayout tile_layout =
{
	8,8,
	0x100,
	2,
	{ 0, 0x2000*8 },
	{ 0,1,2,3,4,5,6,7 },
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8 },
	8*8
};

static struct GfxLayout sprite_layout =
{
	16,16,
	0x40,
	2,
	{ 0, 0x2000*8 },
	{
		0,1,2,3,4,5,6,7,
		64+0,64+1,64+2,64+3,64+4,64+5,64+6,64+7
	},
	{
		0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8,
		128+0*8,128+1*8,128+2*8,128+3*8,128+4*8,128+5*8,128+6*8,128+7*8
	},
	32*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &tile_layout,   0, 0x10 },
	{ REGION_GFX1, 0x1000, &tile_layout,   0, 0x10 },
	{ REGION_GFX1, 0x0800, &sprite_layout, 0, 0x10 },
	{ REGION_GFX1, 0x1800, &sprite_layout, 0, 0x10 },
	{ -1 }
};



static struct SN76496interface sn76496_interface =
{
	2,	/* 2 chips */
	{ 2000000, 2000000 },	/* ? MHz */
	{ 100, 100 }
};



static struct MachineDriver machine_driver_lasso =
{
	{
		{
			CPU_M6502,
			2000000,	/* 2 MHz (?) */
			readmem,writemem,0,0,
			lasso_interrupt,2,
		},
		{
			CPU_M6502,
			2000000,	/* 2 MHz (?) */
			readmem_coprocessor,writemem_coprocessor,0,0,
			ignore_interrupt,1,
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			600000,	/* ?? (controls music tempo) */
			readmem_sound,writemem_sound,0,0,
			ignore_interrupt,1,
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	100, /* CPU slices */
	0, /* init machine */

	/* video hardware */
	32*8, 32*8, { 0, 255, 16, 255-16 },
	gfxdecodeinfo,
	0x40,0x40,
	lasso_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	lasso_vh_start,
	0,
	lasso_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SN76496,
			&sn76496_interface
		}
	}
};

/*
USES THREE 6502 CPUS

CHIP #  POSITION  TYPE
-----------------------
WMA     IC19      2732   DAUGHTER BD	sound cpu
WMB     IC18       "      "				sound data
WMC     IC17       "      "				sound data
WM5     IC45       "     CONN BD		bitmap coprocessor
82S123  IC69              "
82S123  IC70              "
WM4     IC22      2764   BOTTOM BD		main cpu
WM3     IC21       "      "				main cpu
WM2     IC66       "      "				graphics
WM1     IC65       "      "				graphics
*/
ROM_START( lasso )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 6502 code (main cpu) */
	ROM_LOAD( "wm3",       0x8000, 0x2000, 0xf93addd6 )
	ROM_RELOAD(            0xc000, 0x2000)
	ROM_LOAD( "wm4",       0xe000, 0x2000, 0x77719859 )
	ROM_RELOAD(            0xa000, 0x2000)

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 6502 code (lasso image blitter) */
	ROM_LOAD( "wm5",       0xf000, 0x1000, 0x7dc3ff07 )
	ROM_RELOAD(            0x8000, 0x1000)

	ROM_REGION( 0x10000, REGION_CPU3 ) /* 6502 code (sound) */
	ROM_LOAD( "wmc",       0x5000, 0x1000, 0x8b4eb242 )
	ROM_LOAD( "wmb",       0x6000, 0x1000, 0x4658bcb9 )
	ROM_LOAD( "wma",       0x7000, 0x1000, 0x2e7de3e9 )
	ROM_RELOAD(            0xf000, 0x1000 )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "wm1",       0x0000, 0x2000, 0x7db77256 )
	ROM_LOAD( "wm2",       0x2000, 0x2000, 0x9e7d0b6f )

	ROM_REGION( 0x40, REGION_PROMS )
	ROM_LOAD( "82s123.69", 0x0000, 0x0020, 0x1eabb04d )
	ROM_LOAD( "82s123.70", 0x0020, 0x0020, 0x09060f8c )
ROM_END


GAME( 1982, lasso, 0, lasso, lasso, 0, ROT90, "SNK", "Lasso" )
