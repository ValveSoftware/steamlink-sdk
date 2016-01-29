#include "../vidhrdw/cop01.cpp"

/***************************************************************************
Cops 01
Nichibutsu - 1985

calb@gsyc.inf.uc3m.es

TODO:
----
- Fix colors. (it isn't using the lookup proms)
- Fix sprites bank. (ahhhghg!)
- Fix sprites clip.


MEMORY MAP
----------
0000-BFFF  ROM
C000-C7FF  RAM
D000-DFFF  VRAM (Background)
E000-E0FF  VRAM (Sprites)
F000-F3FF  VRAM (Foreground)

AUDIO MEMORY MAP (Advise: Real audio chip used, UNKNOWN)
----------------
0000-7FFF  ROM
8000-8000  UNKNOWN
C000-C700  RAM

***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"

void cop01_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void cop01_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( cop01_scrollx_w );
WRITE_HANDLER( cop01_gfxbank_w );
int cop01_vh_start(void);
void cop01_vh_stop(void);

extern unsigned char *cop01_videoram;
extern size_t cop01_videoram_size;



WRITE_HANDLER( cop01_sound_command_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt(1,0xff);
}

READ_HANDLER( cop01_sound_command_r )
{
	int res;
	static int pulse;
#define TIMER_RATE 12000	/* total guess */


	res = (soundlatch_r(offset) & 0x7f) << 1;

	/* bit 0 seems to be a timer */
	if ((cpu_gettotalcycles() / TIMER_RATE) & 1)
	{
		if (pulse == 0) res |= 1;
		pulse = 1;
	}
	else pulse = 0;

	return res;
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xd000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xe0ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xd000, 0xd7ff, videoram_w, &videoram, &videoram_size },
	{ 0xd800, 0xdfff, colorram_w, &colorram },
	{ 0xe000, 0xe0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xf000, 0xf3ff, MWA_RAM, &cop01_videoram, &cop01_videoram_size },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r },
	{ 0x01, 0x01, input_port_1_r },
	{ 0x02, 0x02, input_port_2_r },
	{ 0x03, 0x03, input_port_3_r },
	{ 0x04, 0x04, input_port_4_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x40, 0x40, cop01_gfxbank_w },
	{ 0x41, 0x42, cop01_scrollx_w },
	{ 0x44, 0x44, cop01_sound_command_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ -1 }
};

static struct IOReadPort sound_readport[] =
{
	{ 0x06, 0x06, cop01_sound_command_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ 0x02, 0x02, AY8910_control_port_1_w },
	{ 0x03, 0x03, AY8910_write_port_1_w },
	{ 0x04, 0x04, AY8910_control_port_2_w },
	{ 0x05, 0x05, AY8910_write_port_2_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( cop01 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL  )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL  )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL  )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL  )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL  )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START  /* TEST, COIN, START */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_SERVICE( 0x20, IP_ACTIVE_LOW )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "Easy" )
	PORT_DIPSETTING(    0x01, "Medium" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x08, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x10, 0x10, "1st Bonus Life" )
	PORT_DIPSETTING(    0x10, "20000" )
	PORT_DIPSETTING(    0x00, "30000" )
	PORT_DIPNAME( 0x60, 0x60, "2nd Bonus Life" )
	PORT_DIPSETTING(    0x60, "30000" )
	PORT_DIPSETTING(    0x20, "50000" )
	PORT_DIPSETTING(    0x40, "100000" )
	PORT_DIPSETTING(    0x00, "150000" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 4, 0, 12, 8, 20, 16, 28, 24 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8	/* every char takes 32 consecutive bytes */
};

static struct GfxLayout tilelayout =
{
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 4, 0, 12, 8, 20, 16, 28, 24 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8	/* every char takes 32 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	512,	/* 512 sprites */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 }, /* plane offset */
	{ 4, 0, 4+512*64*8, 0+512*64*8, 12, 8, 12+512*64*8, 8+512*64*8,
			20, 16, 20+512*64*8, 16+512*64*8, 28, 24, 28+512*64*8, 24+512*64*8 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	64*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,            0, 16 },	/* ?? */
	{ REGION_GFX2, 0, &tilelayout,        16*16,  4 },
	{ REGION_GFX3, 0, &spritelayout, 16*16+4*16, 16 },
	{ -1 }
};



static struct AY8910interface ay8910_interface =
{
	3,	/* 3 chips */
	1500000,	/* 1.5 MHz?????? */
	{ 25, 25, 25 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_cop01 =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3500000,        /* 3.5 Mhz (?) */
			readmem,writemem,readport,writeport,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,        /* 3.0 Mhz (?) */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,0	/* IRQs are caused by the main CPU */
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0, /* init machine */

	/* video hardware */
	32*8, 32*8, { 0, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256, 16*16+4*16+16*16,
	cop01_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	cop01_vh_start,
	cop01_vh_stop,
	cop01_vh_screenrefresh,

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


ROM_START( cop01 )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "cop01.2b",     0x0000, 0x4000, 0x5c2734ab )
	ROM_LOAD( "cop02.4b",     0x4000, 0x4000, 0x9c7336ef )
	ROM_LOAD( "cop03.5b",     0x8000, 0x4000, 0x2566c8bf )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for code */
	ROM_LOAD( "cop15.17b",    0x0000, 0x4000, 0x6a5f08fa )
	ROM_LOAD( "cop16.18b",    0x4000, 0x4000, 0x56bf6946 )

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cop14.15g",    0x00000, 0x2000, 0x066d1c55 )	/* chars */

	ROM_REGION( 0x08000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cop04.15c",    0x00000, 0x4000, 0x622d32e6 )	/* tiles */
	ROM_LOAD( "cop05.16c",    0x04000, 0x4000, 0xc6ac5a35 )

	ROM_REGION( 0x10000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cop10.3e",     0x00000, 0x2000, 0x444cb19d )	/* sprites */
	ROM_LOAD( "cop11.5e",     0x02000, 0x2000, 0x9078bc04 )
	ROM_LOAD( "cop12.6e",     0x04000, 0x2000, 0x257a6706 )
	ROM_LOAD( "cop13.8e",     0x06000, 0x2000, 0x07c4ea66 )
	ROM_LOAD( "cop06.3g",     0x08000, 0x2000, 0xf1c1f4a5 )
	ROM_LOAD( "cop07.5g",     0x0a000, 0x2000, 0x11db7b72 )
	ROM_LOAD( "cop08.6g",     0x0c000, 0x2000, 0xa63ddda6 )
	ROM_LOAD( "cop09.8g",     0x0e000, 0x2000, 0x855a2ec3 )

	ROM_REGION( 0x0500, REGION_PROMS )
	ROM_LOAD( "copproma.13d", 0x0000, 0x0100, 0x97f68a7a )	/* red */
	ROM_LOAD( "coppromb.14d", 0x0100, 0x0100, 0x39a40b4c )	/* green */
	ROM_LOAD( "coppromc.15d", 0x0200, 0x0100, 0x8181748b )	/* blue */
	ROM_LOAD( "coppromd.19d", 0x0300, 0x0100, 0x6a63dbb8 )	/* lookup table? (not implemented) */
	ROM_LOAD( "copprome.2e",  0x0400, 0x0100, 0x214392fa )	/* sprite lookup table */
ROM_END

ROM_START( cop01a )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "cop01alt.001", 0x0000, 0x4000, 0xa13ee0d3 )
	ROM_LOAD( "cop01alt.002", 0x4000, 0x4000, 0x20bad28e )
	ROM_LOAD( "cop01alt.003", 0x8000, 0x4000, 0xa7e10b79 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for code */
	ROM_LOAD( "cop01alt.015", 0x0000, 0x4000, 0x95be9270 )
	ROM_LOAD( "cop01alt.016", 0x4000, 0x4000, 0xc20bf649 )

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cop01alt.014", 0x00000, 0x2000, 0xedd8a474 )	/* chars */

	ROM_REGION( 0x08000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cop04.15c",    0x00000, 0x4000, 0x622d32e6 )	/* tiles */
	ROM_LOAD( "cop05.16c",    0x04000, 0x4000, 0xc6ac5a35 )

	ROM_REGION( 0x10000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cop01alt.010", 0x00000, 0x2000, 0x94aee9d6 )	/* sprites */
	ROM_LOAD( "cop11.5e",     0x02000, 0x2000, 0x9078bc04 )
	ROM_LOAD( "cop12.6e",     0x04000, 0x2000, 0x257a6706 )
	ROM_LOAD( "cop13.8e",     0x06000, 0x2000, 0x07c4ea66 )
	ROM_LOAD( "cop01alt.006", 0x08000, 0x2000, 0xcac7dac8 )
	ROM_LOAD( "cop07.5g",     0x0a000, 0x2000, 0x11db7b72 )
	ROM_LOAD( "cop08.6g",     0x0c000, 0x2000, 0xa63ddda6 )
	ROM_LOAD( "cop09.8g",     0x0e000, 0x2000, 0x855a2ec3 )

	ROM_REGION( 0x0500, REGION_PROMS )
	ROM_LOAD( "copproma.13d", 0x0000, 0x0100, 0x97f68a7a )	/* red */
	ROM_LOAD( "coppromb.14d", 0x0100, 0x0100, 0x39a40b4c )	/* green */
	ROM_LOAD( "coppromc.15d", 0x0200, 0x0100, 0x8181748b )	/* blue */
	ROM_LOAD( "coppromd.19d", 0x0300, 0x0100, 0x6a63dbb8 )	/* lookup table? (not implemented) */
	ROM_LOAD( "copprome.2e",  0x0400, 0x0100, 0x214392fa )	/* sprite lookup table */
ROM_END



GAMEX( 1985, cop01,  0,     cop01, cop01, 0, ROT0, "Nichibutsu", "Cop 01 (set 1)", GAME_IMPERFECT_COLORS )
GAMEX( 1985, cop01a, cop01, cop01, cop01, 0, ROT0, "Nichibutsu", "Cop 01 (set 2)", GAME_IMPERFECT_COLORS )
