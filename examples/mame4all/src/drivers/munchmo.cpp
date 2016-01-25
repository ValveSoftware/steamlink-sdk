#include "../vidhrdw/munchmo.cpp"

/***************************************************************************
  Munch Mobile
  (C) 1982 SNK

  2 Z80s
  2 AY-8910s
  15 MHz crystal

  Known Issues:
	- sprite priority problems
	- it's unclear if mirroring the videoram chunks is correct behavior
	- several unmapped registers
	- sustained sounds (when there should be silence)

***************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"
#include "vidhrdw/generic.h"


extern UINT8 *mnchmobl_vreg;
extern UINT8 *mnchmobl_status_vram;
extern UINT8 *mnchmobl_sprite_xpos;
extern UINT8 *mnchmobl_sprite_attr;
extern UINT8 *mnchmobl_sprite_tile;

void mnchmobl_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int mnchmobl_vh_start( void );
void mnchmobl_vh_stop( void );
WRITE_HANDLER( mnchmobl_palette_bank_w );
WRITE_HANDLER( mnchmobl_flipscreen_w );
READ_HANDLER( mnchmobl_sprite_xpos_r );
WRITE_HANDLER( mnchmobl_sprite_xpos_w );
READ_HANDLER( mnchmobl_sprite_attr_r );
WRITE_HANDLER( mnchmobl_sprite_attr_w );
READ_HANDLER( mnchmobl_sprite_tile_r );
WRITE_HANDLER( mnchmobl_sprite_tile_w );
READ_HANDLER( mnchmobl_videoram_r );
WRITE_HANDLER( mnchmobl_videoram_w );
void mnchmobl_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


/***************************************************************************/

static int mnchmobl_nmi_enable = 0;

static WRITE_HANDLER( mnchmobl_nmi_enable_w )
{
	mnchmobl_nmi_enable = data;
}

static int mnchmobl_interrupt( void )
{
	static int which;
	which = !which;
	if( which ) return interrupt();
	if( mnchmobl_nmi_enable ) return nmi_interrupt();
	return ignore_interrupt();
}

WRITE_HANDLER( mnchmobl_soundlatch_w )
{
	soundlatch_w( offset, data );
	cpu_cause_interrupt( 1, Z80_IRQ_INT );
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x83ff, MRA_RAM }, /* working RAM */
	{ 0xa000, 0xa3ff, MRA_RAM },
	{ 0xa400, 0xa7ff, mnchmobl_sprite_xpos_r }, /* mirrored */
	{ 0xa800, 0xabff, MRA_RAM },
	{ 0xac00, 0xafff, mnchmobl_sprite_tile_r }, /* mirrored */
	{ 0xb000, 0xb3ff, MRA_RAM },
	{ 0xb400, 0xb7ff, mnchmobl_sprite_attr_r }, /* mirrored */
	{ 0xb800, 0xb8ff, MRA_RAM },
	{ 0xb900, 0xb9ff, mnchmobl_videoram_r },	/* mirrored */
	{ 0xbe02, 0xbe02, input_port_3_r }, /* DSW1 */
	{ 0xbe03, 0xbe03, input_port_4_r }, /* DSW2 */
	{ 0xbf01, 0xbf01, input_port_0_r }, /* coin, start */
	{ 0xbf02, 0xbf02, input_port_1_r }, /* P1 controls */
	{ 0xbf03, 0xbf03, input_port_2_r }, /* P2 controls */
	{ -1 }
};

static struct MemoryWriteAddress writemem[] =
{
 	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x8000, 0x83ff, MWA_RAM }, /* working RAM */
	{ 0xa000, 0xa3ff, MWA_RAM, &mnchmobl_sprite_xpos },
	{ 0xa400, 0xa7ff, mnchmobl_sprite_xpos_w },
	{ 0xa800, 0xabff, MWA_RAM, &mnchmobl_sprite_tile },
	{ 0xac00, 0xafff, mnchmobl_sprite_tile_w },
	{ 0xb000, 0xb3ff, MWA_RAM, &mnchmobl_sprite_attr },
	{ 0xb400, 0xb7ff, mnchmobl_sprite_attr_w },
	{ 0xb800, 0xb9ff, mnchmobl_videoram_w, &videoram },
	{ 0xba00, 0xbbff, MWA_RAM },
	{ 0xbc00, 0xbc7f, MWA_RAM, &mnchmobl_status_vram },
	{ 0xbe00, 0xbe00, mnchmobl_soundlatch_w },
	{ 0xbe01, 0xbe01, mnchmobl_palette_bank_w },
	{ 0xbe11, 0xbe11, MWA_RAM }, /* ? */
	{ 0xbe21, 0xbe21, MWA_RAM }, /* ? */
	{ 0xbe31, 0xbe31, MWA_RAM }, /* ? */
	{ 0xbe41, 0xbe41, mnchmobl_flipscreen_w },
	{ 0xbe61, 0xbe61, mnchmobl_nmi_enable_w }, /* ENI 1-10C */
	{ 0xbf00, 0xbf07, MWA_RAM, &mnchmobl_vreg }, /* MY0 1-8C */
	{ -1 }
};

static struct MemoryReadAddress readmem_sound[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x2000, soundlatch_r },
	{ 0xe000, 0xe7ff, MRA_RAM },
	{ -1 }
};
static struct MemoryWriteAddress writemem_sound[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x4000, 0x4000, AY8910_write_port_0_w },
	{ 0x5000, 0x5000, AY8910_control_port_0_w },
	{ 0x6000, 0x6000, AY8910_write_port_1_w },
	{ 0x7000, 0x7000, AY8910_control_port_1_w },
	{ 0x8000, 0x8000, MWA_NOP }, /* ? */
	{ 0xa000, 0xa000, MWA_NOP }, /* ? */
	{ 0xc000, 0xc000, MWA_NOP }, /* ? */
	{ 0xe000, 0xe7ff, MWA_RAM },
	{ -1 }
};

static struct AY8910interface ay8910_interface =
{
	2,	/* 2 chips */
	1500000,	/* 1.5 MHz? */
	{ 50, 50 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

INPUT_PORTS_START( mnchmobl )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 ) /* service */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START /* P1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_2WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_2WAY )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* P2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW1 0xbe02 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x1e, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 3C_1C ) )
//	PORT_DIPSETTING(    0x12, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x16, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
//	PORT_DIPSETTING(    0x1e, DEF_STR( 1C_1C ) )
//	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_1C ) )
//	PORT_DIPSETTING(    0x1a, DEF_STR( 1C_1C ) )
//	PORT_DIPSETTING(    0x18, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_8C ) )
	PORT_DIPNAME( 0xe0, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "No Bonus" )
	PORT_DIPSETTING(    0x20, "70000" )
	PORT_DIPSETTING(    0x40, "60000" )
	PORT_DIPSETTING(    0x60, "50000" )
	PORT_DIPSETTING(    0x80, "40000" )
	PORT_DIPSETTING(    0xa0, "30000" )
	PORT_DIPSETTING(    0xc0, "20000" )
	PORT_DIPSETTING(    0xe0, "10000" )

	PORT_START	/* DSW2 0xbe03 */
	PORT_DIPNAME( 0x03, 0x00, "Second Bonus Life" )
	PORT_DIPSETTING(    0x00, "No Bonus?" )
	PORT_DIPSETTING(    0x01, "100000?" )
	PORT_DIPSETTING(    0x02, "40000?" )
	PORT_DIPSETTING(    0x03, "30000?" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x0c, "5" )
	PORT_DIPNAME( 0x10, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

static struct GfxLayout char_layout =
{
	8,8,
	256,
	4,
	{ 0, 8, 256*128,256*128+8 },
	{ 7,6,5,4,3,2,1,0 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	128
};

static struct GfxLayout tile_layout =
{
	8,8,
	0x100,
	4,
	{ 8,12,0,4 },
	{ 0,0,1,1,2,2,3,3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	128
};

static struct GfxLayout sprite_layout1 =
{
	32,32,
	128,
	3,
	{ 0x4000*8,0x2000*8,0 },
	{
		7,7,6,6,5,5,4,4,3,3,2,2,1,1,0,0,
		0x8000+7,0x8000+7,0x8000+6,0x8000+6,0x8000+5,0x8000+5,0x8000+4,0x8000+4,
		0x8000+3,0x8000+3,0x8000+2,0x8000+2,0x8000+1,0x8000+1,0x8000+0,0x8000+0
	},
	{
		 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		 8*8, 9*8,10*8,11*8,12*8,13*8,14*8,15*8,
		16*8,17*8,18*8,19*8,20*8,21*8,22*8,23*8,
		24*8,25*8,26*8,27*8,28*8,29*8,30*8,31*8
	},
	256
};

static struct GfxLayout sprite_layout2 =
{
	32,32,
	128,
	3,
	{ 0,0,0 },
	{
		7,7,6,6,5,5,4,4,3,3,2,2,1,1,0,0,
		0x8000+7,0x8000+7,0x8000+6,0x8000+6,0x8000+5,0x8000+5,0x8000+4,0x8000+4,
		0x8000+3,0x8000+3,0x8000+2,0x8000+2,0x8000+1,0x8000+1,0x8000+0,0x8000+0
	},
	{
		 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		 8*8, 9*8,10*8,11*8,12*8,13*8,14*8,15*8,
		16*8,17*8,18*8,19*8,20*8,21*8,22*8,23*8,
		24*8,25*8,26*8,27*8,28*8,29*8,30*8,31*8
	},
	256
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0,      &char_layout,      0,  4 },	/* colors   0- 63 */
	{ REGION_GFX2, 0x1000, &tile_layout,     64,  4 },	/* colors  64-127 */
	{ REGION_GFX3, 0,      &sprite_layout1, 128, 16 },	/* colors 128-255 */
	{ REGION_GFX4, 0,      &sprite_layout2, 128, 16 },	/* colors 128-255 */
	{ -1 }
};

static struct MachineDriver machine_driver_munchmo =
{
	{
		{
			CPU_Z80,
			3750000, /* ? */
			readmem,writemem,0,0,
			mnchmobl_interrupt,2
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3750000, /* ? */
			readmem_sound,writemem_sound,0,0,
			nmi_interrupt,1
		}
	},
	57, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	0,

	/* video hardware */
	256+32+32, 256, { 0, 255+32+32,0, 255-16 },
	gfxdecodeinfo,
	256,256,
	mnchmobl_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	mnchmobl_vh_start,
	mnchmobl_vh_stop,
	mnchmobl_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};


ROM_START( joyfulr )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for CPUA */
	ROM_LOAD( "m1j.10e", 0x0000, 0x2000, 0x1fe86e25 )
	ROM_LOAD( "m2j.10d", 0x2000, 0x2000, 0xb144b9a6 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for sound CPU */
	ROM_LOAD( "mu.2j",	 0x0000, 0x2000, 0x420adbd4 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "s1.10a",	 0x0000, 0x1000, 0xc0bcc301 )	/* characters */
	ROM_LOAD( "s2.10b",	 0x1000, 0x1000, 0x96aa11ca )

	ROM_REGION( 0x2000, REGION_GFX2 )
	ROM_LOAD( "b1.2c",	 0x0000, 0x1000, 0x8ce3a403 )	/* tile layout */
	ROM_LOAD( "b2.2b",	 0x1000, 0x1000, 0x0df28913 )	/* 4x8 tiles */

	ROM_REGION( 0x6000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "f1j.1g",	 0x0000, 0x2000, 0x93c3c17e )	/* sprites */
	ROM_LOAD( "f2j.3g",	 0x2000, 0x2000, 0xb3fb5bd2 )
	ROM_LOAD( "f3j.5g",	 0x4000, 0x2000, 0x772a7527 )

	ROM_REGION( 0x2000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "h",		 0x0000, 0x2000, 0x332584de )	/* monochrome sprites */

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "a2001.clr", 0x0000, 0x0100, 0x1b16b907 ) /* color prom */
ROM_END

ROM_START( mnchmobl )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for CPUA */
	ROM_LOAD( "m1.10e",	 0x0000, 0x2000, 0xa4bebc6a )
	ROM_LOAD( "m2.10d",	 0x2000, 0x2000, 0xf502d466 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for sound CPU */
	ROM_LOAD( "mu.2j",	 0x0000, 0x2000, 0x420adbd4 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "s1.10a",	 0x0000, 0x1000, 0xc0bcc301 )	/* characters */
	ROM_LOAD( "s2.10b",	 0x1000, 0x1000, 0x96aa11ca )

	ROM_REGION( 0x2000, REGION_GFX2 )
	ROM_LOAD( "b1.2c",	 0x0000, 0x1000, 0x8ce3a403 )	/* tile layout */
	ROM_LOAD( "b2.2b",	 0x1000, 0x1000, 0x0df28913 )	/* 4x8 tiles */

	ROM_REGION( 0x6000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "f1.1g",	 0x0000, 0x2000, 0xb75411d4 )	/* sprites */
	ROM_LOAD( "f2.3g",	 0x2000, 0x2000, 0x539a43ba )
	ROM_LOAD( "f3.5g",	 0x4000, 0x2000, 0xec996706 )

	ROM_REGION( 0x2000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "h",		 0x0000, 0x2000, 0x332584de )	/* monochrome sprites */

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "a2001.clr", 0x0000, 0x0100, 0x1b16b907 ) /* color prom */
ROM_END


GAME( 1983, joyfulr,  0,       munchmo, mnchmobl, 0, ROT270, "SNK", "Joyful Road (US)" )
GAME( 1983, mnchmobl, joyfulr, munchmo, mnchmobl, 0, ROT270, "SNK (Centuri license)", "Munch Mobile (Japan)" )
