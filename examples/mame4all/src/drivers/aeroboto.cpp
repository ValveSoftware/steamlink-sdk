#include "../vidhrdw/aeroboto.cpp"

/****************************************************************************

Formation Z / Aeroboto

Driver by Carlos A. Lozano

****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


extern unsigned char *aeroboto_videoram;
extern unsigned char *aeroboto_fgscroll,*aeroboto_bgscroll;
extern int aeroboto_charbank;

void aeroboto_gfxctrl_w(int ofset,int data);
void aeroboto_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



static int player;

static READ_HANDLER( aeroboto_in0_r )
{
	return readinputport(player);
}

static READ_HANDLER( aeroboto_201_r )
{
	/* if you keep a button pressed during boot, the game will expect this */
	/* serie of values to be returned from 3004, and display "PASS 201" if it is */
	int res[4] = { 0xff,0x9f,0x1b,0x03};
	static int count;
	//logerror("PC %04x: read 3004\n",cpu_get_pc());
	return res[(count++)&3];
}

static WRITE_HANDLER( aeroboto_3000_w )
{
	/* bit 0 selects player1/player2 controls */
	player = data & 1;

	/* not sure about this, could be bit 2 */
	aeroboto_charbank = (data & 0x02) >> 1;

	/* there's probably a flip screen here as well */
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x0800, 0x08ff, MRA_RAM },	/* ? copied to 2000 */
	{ 0x1000, 0x17ff, MRA_RAM },
	{ 0x1800, 0x183f, MRA_RAM },
	{ 0x2800, 0x28ff, MRA_RAM },
	{ 0x3000, 0x3000, aeroboto_in0_r },
	{ 0x3001, 0x3001, input_port_2_r },
	{ 0x3002, 0x3002, input_port_3_r },
	{ 0x3004, 0x3004, aeroboto_201_r },
	{ 0x3800, 0x3800, watchdog_reset_r },	/* or IRQ acknowledge */
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x0800, 0x08ff, MWA_RAM },	/* ? initialized on startup */
	{ 0x0900, 0x09ff, MWA_RAM },	/* ? initialized on startup (same as 0800) */
	{ 0x1000, 0x13ff, MWA_RAM, &aeroboto_videoram },
	{ 0x1400, 0x17ff, videoram_w, &videoram, &videoram_size },
	{ 0x1800, 0x181f, MWA_RAM, &aeroboto_fgscroll },
	{ 0x1820, 0x183f, MWA_RAM, &aeroboto_bgscroll },
	{ 0x2000, 0x20ff, MWA_RAM },	/* scroll? maybe stars? copied from 0800 */
	{ 0x2800, 0x28ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3000, 0x3000, aeroboto_3000_w },
	{ 0x3001, 0x3001, soundlatch_w },	/* ? */
	{ 0x3002, 0x3002, soundlatch2_w },	/* ? */
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress readmem_sound[] =
{
	{ 0x0000, 0x0fff, MRA_RAM },
	{ 0x9002, 0x9002, AY8910_read_port_0_r },
	{ 0xa002, 0xa002, AY8910_read_port_1_r },
	{ 0xf000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem_sound[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x9000, 0x9000, AY8910_control_port_0_w },
	{ 0x9001, 0x9001, AY8910_write_port_0_w },
	{ 0xa000, 0xa000, AY8910_control_port_1_w },
	{ 0xa001, 0xa001, AY8910_write_port_1_w },
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( formatz )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x03, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "30000" )
	PORT_DIPSETTING(    0x08, "40000" )
	PORT_DIPSETTING(    0x04, "70000" )
	PORT_DIPSETTING(    0x00, "100000" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	/* the coin input must stay low for exactly 2 frames to be consistently recognized. */
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 2 )

	PORT_START
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x08, "Medium" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x18, "Hardest" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	/* the manual lists the flip screen in the previous bank (replacing Coin A) */
	PORT_DIPNAME( 0x80, 0x00, "Flip Screen?" )
	PORT_DIPSETTING(    0x00, "Off?" )
	PORT_DIPSETTING(    0x80, "On?" )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 chars */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 4, 0 },
	{ 0, 1, 2, 3, 512*8*8+0, 512*8*8+1, 512*8*8+2, 512*8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	8,16,	/* 8*16 sprites */
	256,	/* 128 sprites */
	3,	/* 3 bits per pixel */
	{ 2*256*16*8, 256*16*8, 0 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
            8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,     0, 64 },	/* chars */
	{ REGION_GFX2, 0, &charlayout,     0, 64 },	/* sky */
	{ REGION_GFX3, 0, &spritelayout,   0, 32 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	2,      /* 2 chips */
	1500000,        /* 1.5 MHz ? (hand tuned) */
	{ 25, 25 },
	{ soundlatch_r, 0 },	/* ? */
	{ soundlatch2_r, 0 },	/* ? */
	{ 0, 0 },
	{ 0, 0 }
};

static struct MachineDriver machine_driver_formatz =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			1250000,        /* 1.25 Mhz ? */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_M6809 | CPU_AUDIO_CPU,
			1250000,        /* 1.25 Mhz ? */
			readmem_sound,writemem_sound,0,0,
			interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,	/* init machine */

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256,256,
	0,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	aeroboto_vh_screenrefresh,

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

ROM_START( formatz )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for main CPU */
	ROM_LOAD( "format_z.8",   0x4000, 0x4000, 0x81a2416c )
	ROM_LOAD( "format_z.7",   0x8000, 0x4000, 0x986e6052 )
	ROM_LOAD( "format_z.6",   0xc000, 0x4000, 0xbaa0d745 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for sound CPU */
	ROM_LOAD( "format_z.9",   0xf000, 0x1000, 0x6b9215ad )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "format_z.5",   0x0000, 0x2000, 0xba50be57 )	/* characters */

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "format_z.4",   0x0000, 0x2000, 0x910375a0 )	/* characters */

	ROM_REGION( 0x3000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "format_z.1",   0x0000, 0x1000, 0x5739afd2 )	/* sprites */
	ROM_LOAD( "format_z.2",   0x1000, 0x1000, 0x3a821391 )	/* sprites */
	ROM_LOAD( "format_z.3",   0x2000, 0x1000, 0x7d1aec79 )	/* sprites */

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "10a",          0x0000, 0x0100, 0x00000000 )
	ROM_LOAD( "10b",          0x0100, 0x0100, 0x00000000 )
	ROM_LOAD( "10c",          0x0200, 0x0100, 0x00000000 )
ROM_END

ROM_START( aeroboto )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for main CPU */
	ROM_LOAD( "aeroboto.8",   0x4000, 0x4000, 0x4d3fc049 )
	ROM_LOAD( "aeroboto.7",   0x8000, 0x4000, 0x522f51c1 )
	ROM_LOAD( "aeroboto.6",   0xc000, 0x4000, 0x1a295ffb )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for sound CPU */
	ROM_LOAD( "format_z.9",   0xf000, 0x1000, 0x6b9215ad )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "aeroboto.5",   0x0000, 0x2000, 0x32fc00f9 )	/* characters */

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "format_z.4",   0x0000, 0x2000, 0x910375a0 )	/* characters */

	ROM_REGION( 0x3000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "aeroboto.1",   0x0000, 0x1000, 0x7820eeaf )	/* sprites */
	ROM_LOAD( "aeroboto.2",   0x1000, 0x1000, 0xc7f81a3c )	/* sprites */
	ROM_LOAD( "aeroboto.3",   0x2000, 0x1000, 0x5203ad04 )	/* sprites */

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "10a",          0x0000, 0x0100, 0x00000000 )
	ROM_LOAD( "10b",          0x0100, 0x0100, 0x00000000 )
	ROM_LOAD( "10c",          0x0200, 0x0100, 0x00000000 )
ROM_END



GAMEX( 1984, formatz,  0,       formatz, formatz, 0, ROT0, "Jaleco", "Formation Z", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
GAMEX( 1984, aeroboto, formatz, formatz, formatz, 0, ROT0, "[Jaleco] (Williams license)", "Aeroboto", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
