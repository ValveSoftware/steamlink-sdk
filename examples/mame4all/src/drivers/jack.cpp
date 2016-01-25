#include "../vidhrdw/jack.cpp"

/***************************************************************************

Jack the Giant Killer memory map (preliminary)

driver by Brad Oliver


Main CPU
--------
0000-3fff  ROM
4000-5fff  RAM
b000-b07f  sprite ram
b400       command for sound CPU
b500-b505  input ports
b506	   screen flip off
b507	   screen flip on
b600-b61f  palette ram
b800-bbff  video ram
bc00-bfff  color ram
c000-ffff  More ROM

Sound CPU (appears to run in interrupt mode 1)
---------
0000-0fff  ROM
1000-1fff  ROM (Zzyzzyxx only)
4000-43ff  RAM
6000-6fff  R/C filter ???

I/O
---
0x40: Read - ay-8910 port 0
      Write - ay-8910 write
0x80: Write - ay-8910 control

The 2 ay-8910 read ports are responsible for reading the sound commands.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"


WRITE_HANDLER( jack_paletteram_w );
void jack_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
READ_HANDLER( jack_flipscreen_r );
WRITE_HANDLER( jack_flipscreen_w );


static int timer_rate;

static READ_HANDLER( timer_r )
{
	/* wrong! there should be no need for timer_rate, the same function */
	/* should work for both games */
	return cpu_gettotalcycles() / timer_rate;
}


static WRITE_HANDLER( jack_sh_command_w )
{
	soundlatch_w(0,data);
	cpu_cause_interrupt(1, Z80_IRQ_INT);
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x5fff, MRA_RAM },
	{ 0xb000, 0xb07f, MRA_RAM },
	{ 0xb500, 0xb500, input_port_0_r },
	{ 0xb501, 0xb501, input_port_1_r },
	{ 0xb502, 0xb502, input_port_2_r },
	{ 0xb503, 0xb503, input_port_3_r },
	{ 0xb504, 0xb504, input_port_4_r },
	{ 0xb505, 0xb505, input_port_5_r },
	{ 0xb506, 0xb507, jack_flipscreen_r },
	{ 0xb800, 0xbfff, MRA_RAM },
	{ 0xc000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x5fff, MWA_RAM },
	{ 0xb000, 0xb07f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xb400, 0xb400, jack_sh_command_w },
	{ 0xb506, 0xb507, jack_flipscreen_w },
	{ 0xb600, 0xb61f, jack_paletteram_w, &paletteram },
	{ 0xb800, 0xbbff, videoram_w, &videoram, &videoram_size },
	{ 0xbc00, 0xbfff, colorram_w, &colorram },
	{ 0xc000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x4000, 0x43ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x4000, 0x43ff, MWA_RAM },
	{ 0x6000, 0x6fff, MWA_NOP },  /* R/C filter ??? */
	{ -1 }	/* end of table */
};


static struct IOReadPort sound_readport[] =
{
	{ 0x40, 0x40, AY8910_read_port_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x80, 0x80, AY8910_control_port_0_w },
	{ 0x40, 0x40, AY8910_write_port_0_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( jack )
	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "Every 10000" )
	PORT_DIPSETTING(    0x20, "10000 Only" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Start on Level 1" )
	PORT_DIPSETTING(    0x40, "Start on Level 5" )
	PORT_DIPNAME( 0x80, 0x00, "Bullets per Bean Collected" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x80, "2" )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )  // Most likely unused
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )  // Most likely unused
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )  // Most likely unused
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )  // Most likely unused
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_SERVICE( 0x20, IP_ACTIVE_HIGH )
	PORT_BITX (   0x40, 0x00, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_BITX (   0x80, 0x00, IPT_DIPSWITCH_NAME | IPF_CHEAT, "255 Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x1c, IP_ACTIVE_HIGH, IPT_UNKNOWN )  // Most likely unused
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )  // Most likely unused

	PORT_START      /* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNKNOWN ) // Most likely unused

	PORT_START      /* IN5 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNKNOWN ) // Most likely unused
INPUT_PORTS_END

INPUT_PORTS_START( zzyzzyxx )
	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0x08, 0x00, "Start with 2 Credits" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x02, "Never" )
	PORT_DIPSETTING(    0x00, "10k/50k" )
	PORT_DIPSETTING(    0x01, "25k/100k" )
	PORT_DIPSETTING(    0x03, "100k/300k" )
	PORT_DIPNAME( 0x04, 0x04, "2nd Bonus Given" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Starting Laps" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPNAME( 0x10, 0x00, "Difficulty of Pleasing Lola" )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPNAME( 0x20, 0x00, "Show Intermissions" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Yes ) )
	PORT_DIPNAME( 0xc0, 0x00, "Extra Lives" )
	PORT_DIPSETTING(    0x00, "None" )
  //PORT_DIPSETTING(    0x40, "None" )
	PORT_DIPSETTING(    0x80, "4 under 4000 pts" )
	PORT_DIPSETTING(    0xc0, "6 under 4000 pts" )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x1c, IP_ACTIVE_HIGH, IPT_UNKNOWN )  // Most likely unused
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNKNOWN )  // Most likely unused

	PORT_START      /* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_2WAY )
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0xfe, IP_ACTIVE_HIGH, IPT_UNKNOWN ) // Most likely unused

	PORT_START      /* IN5 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0xfe, IP_ACTIVE_HIGH, IPT_UNKNOWN ) // Most likely unused
INPUT_PORTS_END

INPUT_PORTS_START( freeze )
	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_SERVICE( 0x02, IP_ACTIVE_HIGH )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x08, "5" )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "10000" )
	PORT_DIPSETTING(    0x10, "10000 40000" )
	PORT_DIPSETTING(    0x20, "10000 60000" )
	PORT_DIPSETTING(    0x30, "20000 100000" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( Free_Play ) )

	PORT_START      /* DSW2 */
	/* probably unused */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN5 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	2,	/* 2 bits per pixel */
	{ 0, 1024*8*8 },	/* the two bitplanes are seperated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 16 bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout, 0, 8 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	1,	/* 1 chip */
	18000000/12,	/* 1.5 MHz */
	{ 100 },
	{ soundlatch_r },
	{ timer_r },
	{ 0 },
	{ 0 }
};


static struct MachineDriver machine_driver_jack =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18000000/6,	/* 3 MHz */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			18000000/12,	/* 1.5 MHz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,0	/* IRQs are caused by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32, 32,
	0,

	VIDEO_TYPE_RASTER|VIDEO_MODIFIES_PALETTE,
	0,
	generic_vh_start,
	generic_vh_stop,
	jack_vh_screenrefresh,

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

ROM_START( jack )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "j8",           0x0000, 0x1000, 0xc8e73998 )
	ROM_LOAD( "jgk.j6",       0x1000, 0x1000, 0x36d7810e )
	ROM_LOAD( "jgk.j7",       0x2000, 0x1000, 0xb15ff3ee )
	ROM_LOAD( "jgk.j5",       0x3000, 0x1000, 0x4a63d242 )
	ROM_LOAD( "jgk.j3",       0xc000, 0x1000, 0x605514a8 )
	ROM_LOAD( "jgk.j4",       0xd000, 0x1000, 0xbce489b7 )
	ROM_LOAD( "jgk.j2",       0xe000, 0x1000, 0xdb21bd55 )
	ROM_LOAD( "jgk.j1",       0xf000, 0x1000, 0x49fffe31 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "jgk.j9",       0x0000, 0x1000, 0xc2dc1e00 )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "jgk.j12",      0x0000, 0x1000, 0xce726df0 )
	ROM_LOAD( "jgk.j13",      0x1000, 0x1000, 0x6aec2c8d )
	ROM_LOAD( "jgk.j11",      0x2000, 0x1000, 0xfd14c525 )
	ROM_LOAD( "jgk.j10",      0x3000, 0x1000, 0xeab890b2 )
ROM_END

ROM_START( jack2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "jgk.j8",       0x0000, 0x1000, 0xfe229e20 )
	ROM_LOAD( "jgk.j6",       0x1000, 0x1000, 0x36d7810e )
	ROM_LOAD( "jgk.j7",       0x2000, 0x1000, 0xb15ff3ee )
	ROM_LOAD( "jgk.j5",       0x3000, 0x1000, 0x4a63d242 )
	ROM_LOAD( "jgk.j3",       0xc000, 0x1000, 0x605514a8 )
	ROM_LOAD( "jgk.j4",       0xd000, 0x1000, 0xbce489b7 )
	ROM_LOAD( "jgk.j2",       0xe000, 0x1000, 0xdb21bd55 )
	ROM_LOAD( "jgk.j1",       0xf000, 0x1000, 0x49fffe31 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "jgk.j9",       0x0000, 0x1000, 0xc2dc1e00 )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "jgk.j12",      0x0000, 0x1000, 0xce726df0 )
	ROM_LOAD( "jgk.j13",      0x1000, 0x1000, 0x6aec2c8d )
	ROM_LOAD( "jgk.j11",      0x2000, 0x1000, 0xfd14c525 )
	ROM_LOAD( "jgk.j10",      0x3000, 0x1000, 0xeab890b2 )
ROM_END

ROM_START( jack3 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "jack8",        0x0000, 0x1000, 0x632151d2 )
	ROM_LOAD( "jack6",        0x1000, 0x1000, 0xf94f80d9 )
	ROM_LOAD( "jack7",        0x2000, 0x1000, 0xc830ff1e )
	ROM_LOAD( "jack5",        0x3000, 0x1000, 0x8dea17e7 )
	ROM_LOAD( "jgk.j3",       0xc000, 0x1000, 0x605514a8 )
	ROM_LOAD( "jgk.j4",       0xd000, 0x1000, 0xbce489b7 )
	ROM_LOAD( "jgk.j2",       0xe000, 0x1000, 0xdb21bd55 )
	ROM_LOAD( "jack1",        0xf000, 0x1000, 0x7e75ea3d )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "jgk.j9",       0x0000, 0x1000, 0xc2dc1e00 )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "jack12",       0x0000, 0x1000, 0x80320647 )
	ROM_LOAD( "jgk.j13",      0x1000, 0x1000, 0x6aec2c8d )
	ROM_LOAD( "jgk.j11",      0x2000, 0x1000, 0xfd14c525 )
	ROM_LOAD( "jgk.j10",      0x3000, 0x1000, 0xeab890b2 )
ROM_END

ROM_START( treahunt )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "thunt-1.f2",   0x0000, 0x1000, 0x0b35858c )
	ROM_LOAD( "thunt-2.f3",   0x1000, 0x1000, 0x67305a51 )
	ROM_LOAD( "thunt-3.4f",   0x2000, 0x1000, 0xd7a969c3 )
	ROM_LOAD( "thunt-4.6f",   0x3000, 0x1000, 0x2483f14d )
	ROM_LOAD( "thunt-5.7f",   0xc000, 0x1000, 0xc69d5e21 )
	ROM_LOAD( "thunt-6.7e",   0xd000, 0x1000, 0x11bf3d49 )
	ROM_LOAD( "thunt-7.6e",   0xe000, 0x1000, 0x7c2d6279 )
	ROM_LOAD( "thunt-8.4e",   0xf000, 0x1000, 0xf73b86fb )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "jgk.j9",       0x0000, 0x1000, 0xc2dc1e00 )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "thunt-13.a4",  0x0000, 0x1000, 0xe03f1f09 )
	ROM_LOAD( "thunt-12.a3",  0x1000, 0x1000, 0xda4ee9eb )
	ROM_LOAD( "thunt-10.a1",  0x2000, 0x1000, 0x51ec7934 )
	ROM_LOAD( "thunt-11.a2",  0x3000, 0x1000, 0xf9781143 )
ROM_END

ROM_START( zzyzzyxx )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a.2f",         0x0000, 0x1000, 0xa9102e34 )
	ROM_LOAD( "zzyzzyxx.b",   0x1000, 0x1000, 0xefa9d4c6 )
	ROM_LOAD( "zzyzzyxx.c",   0x2000, 0x1000, 0xb0a365b1 )
	ROM_LOAD( "zzyzzyxx.d",   0x3000, 0x1000, 0x5ed6dd9a )
	ROM_LOAD( "zzyzzyxx.e",   0xc000, 0x1000, 0x5966fdbf )
	ROM_LOAD( "f.7e",         0xd000, 0x1000, 0x12f24c68 )
	ROM_LOAD( "g.6e",         0xe000, 0x1000, 0x408f2326 )
	ROM_LOAD( "h.4e",         0xf000, 0x1000, 0xf8bbabe0 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "i.5a",         0x0000, 0x1000, 0xc7742460 )
	ROM_LOAD( "j.6a",         0x1000, 0x1000, 0x72166ccd )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "n.1c",         0x0000, 0x1000, 0x4f64538d )
	ROM_LOAD( "m.1d",         0x1000, 0x1000, 0x217b1402 )
	ROM_LOAD( "k.1b",         0x2000, 0x1000, 0xb8b2b8cc )
	ROM_LOAD( "l.1a",         0x3000, 0x1000, 0xab421a83 )
ROM_END

ROM_START( zzyzzyx2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a.2f",         0x0000, 0x1000, 0xa9102e34 )
	ROM_LOAD( "b.3f",         0x1000, 0x1000, 0x4277beab )
	ROM_LOAD( "c.4f",         0x2000, 0x1000, 0x72ac99e1 )
	ROM_LOAD( "d.6f",         0x3000, 0x1000, 0x7c7eec2b )
	ROM_LOAD( "e.7f",         0xc000, 0x1000, 0xcffc4a68 )
	ROM_LOAD( "f.7e",         0xd000, 0x1000, 0x12f24c68 )
	ROM_LOAD( "g.6e",         0xe000, 0x1000, 0x408f2326 )
	ROM_LOAD( "h.4e",         0xf000, 0x1000, 0xf8bbabe0 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "i.5a",         0x0000, 0x1000, 0xc7742460 )
	ROM_LOAD( "j.6a",         0x1000, 0x1000, 0x72166ccd )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "n.1c",         0x0000, 0x1000, 0x4f64538d )
	ROM_LOAD( "m.1d",         0x1000, 0x1000, 0x217b1402 )
	ROM_LOAD( "k.1b",         0x2000, 0x1000, 0xb8b2b8cc )
	ROM_LOAD( "l.1a",         0x3000, 0x1000, 0xab421a83 )
ROM_END

ROM_START( brix )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a",            0x0000, 0x1000, 0x050e0d70 )
	ROM_LOAD( "b",            0x1000, 0x1000, 0x668118ae )
	ROM_LOAD( "c",            0x2000, 0x1000, 0xff5ed6cf )
	ROM_LOAD( "d",            0x3000, 0x1000, 0xc3ae45a9 )
	ROM_LOAD( "e",            0xc000, 0x1000, 0xdef99fa9 )
	ROM_LOAD( "f",            0xd000, 0x1000, 0xdde717ed )
	ROM_LOAD( "g",            0xe000, 0x1000, 0xadca02d8 )
	ROM_LOAD( "h",            0xf000, 0x1000, 0xbc3b878c )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "i.5a",         0x0000, 0x1000, 0xc7742460 )
	ROM_LOAD( "j.6a",         0x1000, 0x1000, 0x72166ccd )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "n",            0x0000, 0x1000, 0x8064910e )
	ROM_LOAD( "m.1d",         0x1000, 0x1000, 0x217b1402 )
	ROM_LOAD( "k",            0x2000, 0x1000, 0xc7d7e2a0 )
	ROM_LOAD( "l.1a",         0x3000, 0x1000, 0xab421a83 )
ROM_END

ROM_START( freeze )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "freeze.f2",    0x0000, 0x1000, 0x0a431665 )
	ROM_LOAD( "freeze.f3",    0x1000, 0x1000, 0x1189b8ad )
	ROM_LOAD( "freeze.f4",    0x2000, 0x1000, 0x10c4a5ea )
	ROM_LOAD( "freeze.f5",    0x3000, 0x1000, 0x16024c53 )
	ROM_LOAD( "freeze.f7",    0xc000, 0x1000, 0xea0b0765 )
	ROM_LOAD( "freeze.e7",    0xd000, 0x1000, 0x1155c00b )
	ROM_LOAD( "freeze.e5",    0xe000, 0x1000, 0x95c18d75 )
	ROM_LOAD( "freeze.e4",    0xf000, 0x1000, 0x7e8f5afc )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "freeze.a1",    0x0000, 0x1000, 0x7771f5b9 )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "freeze.5a",    0x0000, 0x1000, 0x6c8a98a0 )
	ROM_LOAD( "freeze.3a",    0x1000, 0x1000, 0x6d2125e4 )
	ROM_LOAD( "freeze.1a",    0x2000, 0x1000, 0x3a7f2fa9 )
	ROM_LOAD( "freeze.2a",    0x3000, 0x1000, 0xdd70ddd6 )
ROM_END

ROM_START( sucasino )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "1",       	  0x0000, 0x1000, 0xe116e979 )
	ROM_LOAD( "2",      	  0x1000, 0x1000, 0x2a2635f5 )
	ROM_LOAD( "3",       	  0x2000, 0x1000, 0x69864d90 )
	ROM_LOAD( "4",       	  0x3000, 0x1000, 0x174c9373 )
	ROM_LOAD( "5",       	  0xc000, 0x1000, 0x115bcb1e )
	ROM_LOAD( "6",       	  0xd000, 0x1000, 0x434caa17 )
	ROM_LOAD( "7",       	  0xe000, 0x1000, 0x67c68b82 )
	ROM_LOAD( "8",       	  0xf000, 0x1000, 0xf5b63006 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "9",       	  0x0000, 0x1000, 0x67cf8aec )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11",      	  0x0000, 0x1000, 0xf92c4c5b )
	/* 1000-1fff empty */
	ROM_LOAD( "10",      	  0x2000, 0x1000, 0x3b0783ce )
	/* 3000-3fff empty */
ROM_END



static void treahunt_decode(void)
{
	int A;
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;
	int data;


	memory_set_opcode_base(0,rom+diff);

	/* Thanks to Mike Balfour for helping out with the decryption */
	for (A = 0; A < 0x4000; A++)
	{
		data = rom[A];

		if (A & 0x1000)
		{
			/* unencrypted = D0 D2 D5 D1 D3 D6 D4 D7 */
			rom[A+diff] =
				 ((data & 0x01) << 7) |
				 ((data & 0x02) << 3) |
				 ((data & 0x04) << 4) |
				  (data & 0x28) |
				 ((data & 0x10) >> 3) |
				 ((data & 0x40) >> 4) |
				 ((data & 0x80) >> 7);

			if ((A & 0x04) == 0)
			/* unencrypted = !D0 D2 D5 D1 D3 D6 D4 !D7 */
				rom[A+diff] ^= 0x81;
		}
		else
		{
			/* unencrypted = !D7 D2 D5 D1 D3 D6 D4 !D0 */
			rom[A+diff] =
					(~data & 0x81) |
					((data & 0x02) << 3) |
					((data & 0x04) << 4) |
					 (data & 0x28) |
					((data & 0x10) >> 3) |
					((data & 0x40) >> 4);
		}
	}
}

static void init_jack(void)
{
	timer_rate = 128;
}

static void init_treahunt(void)
{
	timer_rate = 128;
	treahunt_decode();
}

static void init_zzyzzyxx(void)
{
	timer_rate = 16;
}



GAME( 1982, jack,     0,        jack, jack,     jack,     ROT90, "Cinematronics", "Jack the Giantkiller (set 1)" )
GAME( 1982, jack2,    jack,     jack, jack,     jack,     ROT90, "Cinematronics", "Jack the Giantkiller (set 2)" )
GAME( 1982, jack3,    jack,     jack, jack,     jack,     ROT90, "Cinematronics", "Jack the Giantkiller (set 3)" )
GAME( 1982, treahunt, jack,     jack, jack,     treahunt, ROT90, "Hara Industries", "Treasure Hunt (Japan?)" )
GAME( 1982, zzyzzyxx, 0,        jack, zzyzzyxx, zzyzzyxx, ROT90, "Cinematronics + Advanced Microcomputer Systems", "Zzyzzyxx (set 1)" )
GAME( 1982, zzyzzyx2, zzyzzyxx, jack, zzyzzyxx, zzyzzyxx, ROT90, "Cinematronics + Advanced Microcomputer Systems", "Zzyzzyxx (set 2)" )
GAME( 1982, brix,     zzyzzyxx, jack, zzyzzyxx, zzyzzyxx, ROT90, "Cinematronics + Advanced Microcomputer Systems", "Brix" )
GAME( ????, freeze,   0,        jack, freeze,   jack,     ROT90, "Cinematronics", "Freeze" )
GAME( 1982, sucasino, 0,        jack, jack,     jack,     ROT90, "Data Amusement", "Super Casino" )
