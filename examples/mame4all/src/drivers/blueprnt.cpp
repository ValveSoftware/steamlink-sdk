#include "../vidhrdw/blueprnt.cpp"

/***************************************************************************

Blue Print memory map (preliminary)

driver by Nicola Salmoria


CPU #1
0000-4fff ROM
8000-87ff RAM
9000-93ff Video RAM
b000-b0ff Sprite RAM
f000-f3ff Color RAM

read:
c000      IN0
c001      IN1
c003      read dip switches from the second CPU

e000      Watchdog reset

write:
c000      bit 0,1 = coin counters
d000      command for the second CPU
e000      bit 1 = flip screen

CPU #2
0000-0fff ROM
2000-2fff ROM
4000-43ff RAM

read:
6002      8910 #0 read
8002      8910 #1 read

write:
6000      8910 #0 control
6001      8910 #0 write
8000      8910 #1 control
8001      8910 #1 write

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"



extern unsigned char *blueprnt_scrollram;

void blueprnt_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( blueprnt_flipscreen_w );
void blueprnt_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



static int dipsw;

static WRITE_HANDLER( dipsw_w )
{
	dipsw = data;
}

READ_HANDLER( blueprnt_sh_dipsw_r )
{
	return dipsw;
}

WRITE_HANDLER( blueprnt_sound_command_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt(1,Z80_NMI_INT);
}

static WRITE_HANDLER( blueprnt_coin_w )
{
	static int lastval;

	if (lastval == data) return;
	coin_counter_w (0, data & 0x01);
	coin_counter_w (1, data & 0x02);
	lastval = data;
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x9000, 0x93ff, MRA_RAM },
	{ 0x9400, 0x97ff, videoram_r },	/* mirror address, I THINK */
	{ 0xa000, 0xa01f, MRA_RAM },
	{ 0xb000, 0xb0ff, MRA_RAM },
	{ 0xc000, 0xc000, input_port_0_r },
	{ 0xc001, 0xc001, input_port_1_r },
	{ 0xc003, 0xc003, blueprnt_sh_dipsw_r },
	{ 0xe000, 0xe000, watchdog_reset_r },
	{ 0xf000, 0xf3ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x9000, 0x93ff, videoram_w, &videoram, &videoram_size },
	{ 0xa000, 0xa01f, MWA_RAM, &blueprnt_scrollram },
	{ 0xb000, 0xb0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc000, 0xc000, blueprnt_coin_w },
	{ 0xd000, 0xd000, blueprnt_sound_command_w },
	{ 0xe000, 0xe000, blueprnt_flipscreen_w },	/* + gfx bank */
	{ 0xf000, 0xf3ff, colorram_w, &colorram },
	{ -1 }	/* end of table */
};



static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x0fff, MRA_ROM },
	{ 0x2000, 0x2fff, MRA_ROM },
	{ 0x4000, 0x43ff, MRA_RAM },
	{ 0x6002, 0x6002, AY8910_read_port_0_r },
	{ 0x8002, 0x8002, AY8910_read_port_1_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_ROM },
	{ 0x2000, 0x2fff, MWA_ROM },
	{ 0x4000, 0x43ff, MWA_RAM },
	{ 0x6000, 0x6000, AY8910_control_port_0_w },
	{ 0x6001, 0x6001, AY8910_write_port_0_w },
	{ 0x8000, 0x8000, AY8910_control_port_1_w },
	{ 0x8001, 0x8001, AY8910_write_port_1_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( blueprnt )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_SERVICE( 0x04, IP_ACTIVE_HIGH )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20000" )
	PORT_DIPSETTING(    0x02, "30000" )
	PORT_DIPSETTING(    0x04, "40000" )
	PORT_DIPSETTING(    0x06, "50000" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Maze Monster" )
	PORT_DIPSETTING(    0x00, "2nd Maze" )
	PORT_DIPSETTING(    0x10, "3rd Maze" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x10, "Medium" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x30, "Hardest" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( saturn )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x80, "5" )
	PORT_DIPSETTING(    0xc0, "6" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, "A 2/1 B 1/3" )
	PORT_DIPSETTING(    0x00, "A 1/1 B 1/6" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 512*8*8, 0 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	8,16,	/* 8*16 sprites */
	256,	/* 256 sprites */
	3,	/* 3 bits per pixel */
	{ 2*128*16*16, 128*16*16, 0 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,       0, 128 },
	{ REGION_GFX2, 0, &spritelayout, 128*4,   1 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	2,	/* 2 chips */
	10000000/8,	/* 1.25 MHz (4H) */
	{ 25, 25 },
	{            0, input_port_2_r },
	{ soundlatch_r, input_port_3_r },
	{ dipsw_w },
	{ 0 }
};



static struct MachineDriver machine_driver_blueprnt =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			10000000/4,	/* 2.5 MHz (2H) */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80,	/* can't use CPU_AUDIO_CPU because this CPU reads the dip switches */
			10000000/4,	/* 2.5 MHz (2H) */
			sound_readmem,sound_writemem,0,0,
			interrupt,4	/* IRQs connected to 32V */
						/* NMIs are caused by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	16,128*4+8,
	blueprnt_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	blueprnt_vh_screenrefresh,

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

ROM_START( blueprnt )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "1m",           0x0000, 0x1000, 0xb20069a6 )
	ROM_LOAD( "1n",           0x1000, 0x1000, 0x4a30302e )
	ROM_LOAD( "1p",           0x2000, 0x1000, 0x6866ca07 )
	ROM_LOAD( "1r",           0x3000, 0x1000, 0x5d3cfac3 )
	ROM_LOAD( "1s",           0x4000, 0x1000, 0xa556cac4 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "3u",           0x0000, 0x1000, 0xfd38777a )
	ROM_LOAD( "3v",           0x2000, 0x1000, 0x33d5bf5b )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "c3",           0x0000, 0x1000, 0xac2a61bc )
	ROM_LOAD( "d3",           0x1000, 0x1000, 0x81fe85d7 )

	ROM_REGION( 0x3000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "d17",          0x0000, 0x1000, 0xa73b6483 )
	ROM_LOAD( "d18",          0x1000, 0x1000, 0x7d622550 )
	ROM_LOAD( "d20",          0x2000, 0x1000, 0x2fcb4f26 )
ROM_END

ROM_START( blueprnj )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "bp_01.bin",    0x0000, 0x1000, 0x2e746693 )
	ROM_LOAD( "bp_02.bin",    0x1000, 0x1000, 0xa0eb0b8e )
	ROM_LOAD( "bp_03.bin",    0x2000, 0x1000, 0xc34981bb )
	ROM_LOAD( "bp_04.bin",    0x3000, 0x1000, 0x525e77b5 )
	ROM_LOAD( "bp_05.bin",    0x4000, 0x1000, 0x431a015f )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "3u",           0x0000, 0x1000, 0xfd38777a )
	ROM_LOAD( "3v",           0x2000, 0x1000, 0x33d5bf5b )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bp_09.bin",    0x0000, 0x0800, 0x43718c34 )
	ROM_LOAD( "bp_08.bin",    0x1000, 0x0800, 0xd3ce077d )

	ROM_REGION( 0x3000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bp_10.bin",    0x0000, 0x1000, 0x83da108f )
	ROM_LOAD( "bp_11.bin",    0x1000, 0x1000, 0xb440f32f )
	ROM_LOAD( "bp_12.bin",    0x2000, 0x1000, 0x23026765 )
ROM_END

ROM_START( saturn )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "r1",           0x0000, 0x1000, 0x18a6d68e )
	ROM_LOAD( "r2",           0x1000, 0x1000, 0xa7dd2665 )
	ROM_LOAD( "r3",           0x2000, 0x1000, 0xb9cfa791 )
	ROM_LOAD( "r4",           0x3000, 0x1000, 0xc5a997e7 )
	ROM_LOAD( "r5",           0x4000, 0x1000, 0x43444d00 )
	ROM_LOAD( "r6",           0x5000, 0x1000, 0x4d4821f6 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "r7",           0x0000, 0x1000, 0xdd43e02f )
	ROM_LOAD( "r8",           0x2000, 0x1000, 0x7f9d0877 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "r10",          0x0000, 0x1000, 0x35987d61 )
	ROM_LOAD( "r9",           0x1000, 0x1000, 0xca6a7fda )

	ROM_REGION( 0x3000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "r11",          0x0000, 0x1000, 0x6e4e6e5d )
	ROM_LOAD( "r12",          0x1000, 0x1000, 0x46fc049e )
	ROM_LOAD( "r13",          0x2000, 0x1000, 0x8b3e8c32 )
ROM_END



GAME( 1982, blueprnt, 0,        blueprnt, blueprnt, 0, ROT270, "[Zilec] Bally Midway", "Blue Print (Midway)" )
GAME( 1982, blueprnj, blueprnt, blueprnt, blueprnt, 0, ROT270, "[Zilec] Jaleco", "Blue Print (Jaleco)" )
GAME( 1983, saturn,   0,        blueprnt, saturn,   0, ROT270, "[Zilec] Jaleco", "Saturn" )
