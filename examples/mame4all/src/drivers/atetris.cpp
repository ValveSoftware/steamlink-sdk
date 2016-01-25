#include "../vidhrdw/atetris.cpp"

/***************************************************************************

Atari Tetris Memory Map (preliminary)

driver by Zsolt Vasvari

0000-0fff   RAM
1000-1fff   Video RAM
2000-20ff   Palette RAM
2400-25ff   EEPROM
4000-7fff   Paged ROM (Slapstic controlled)
8000-ffff   ROM

I/O

Read

2800-280f Pokey #1
2800-281f Pokey #2

Write

2800-280f Pokey #1
2810-281f Pokey #2
3000      Watchdog
3400      EEPROM enable
3800      ???
3c00      ???

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


void atetris_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
READ_HANDLER( atetris_slapstic_r );



static unsigned char *nvram;
static size_t nvram_size;

static void nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file,nvram,nvram_size);
	else
	{
		if (file)
			osd_fread(file,nvram,nvram_size);
		else
			memset(nvram,0xff,nvram_size);
	}
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x20ff, MRA_RAM },
	{ 0x2400, 0x25ff, MRA_RAM },
	{ 0x2800, 0x280f, pokey1_r },
	{ 0x2810, 0x281f, pokey2_r },
	{ 0x4000, 0x7fff, atetris_slapstic_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x1000, 0x1fff, videoram_w, &videoram, &videoram_size },
	{ 0x2000, 0x20ff, paletteram_RRRGGGBB_w, &paletteram },
	{ 0x2400, 0x25ff, MWA_RAM, &nvram, &nvram_size },
	{ 0x2800, 0x280f, pokey1_w },
	{ 0x2810, 0x281f, pokey2_w },
	{ 0x3000, 0x3000, watchdog_reset_w },
	{ 0x3400, 0x3400, MWA_NOP },  // EEPROM enable
	{ 0x3800, 0x3800, MWA_NOP },  // ???
	{ 0x3c00, 0x3c00, MWA_NOP },  // ???
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( atetris )
	// These ports are read via the Pokeys
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_DIPNAME( 0x04, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, IPT_SERVICE, "Freeze Step", KEYCODE_F1, IP_JOY_NONE )
	PORT_BIT( 0x30, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
INPUT_PORTS_END


// Same as the regular one except they added a Flip Controls switch
INPUT_PORTS_START( atetcktl )
	// These ports are read via the Pokeys
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_DIPNAME( 0x04, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_BITX(0x08, IP_ACTIVE_HIGH, IPT_SERVICE, "Freeze Step", KEYCODE_F1, IP_JOY_NONE )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_DIPNAME( 0x20, 0x00, "Flip Controls" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
        8,8,    /* 8*8 chars */
        2048,   /* 2048 characters */
        4,      /* 4 bits per pixel */
        { 0,1,2,3 },  // The 4 planes are packed together
        { 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4},
        { 0*4*8, 1*4*8, 2*4*8, 3*4*8, 4*4*8, 5*4*8, 6*4*8, 7*4*8},
        8*8*4     /* every char takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout, 0, 16 },
	{ -1 } /* end of array */
};



static struct POKEYinterface pokey_interface =
{
	2,      /* 2 chips */
	1789790,	/* ? */
	{ 50, 50 },
	/* The 8 pot handlers */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	/* The allpot handler */
	{ input_port_0_r, input_port_1_r }
};

static struct MachineDriver machine_driver_atetris =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1750000,        /* 1.75 MHz??? */
			readmem,writemem,0,0,
			interrupt,4
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,  /* frames per second, vblank duration */
	1,      /* single CPU, no need for interleaving */
	0,

	/* video hardware */
	42*8, 32*8, { 0*8, 42*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	256, 256,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	generic_vh_start,
	generic_vh_stop,
	atetris_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		}
	},

	nvram_handler
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( atetris )
	ROM_REGION( 0x14000, REGION_CPU1 )     /* 80k for code */
	ROM_LOAD( "1100.45f",     0x0000, 0x10000, 0x2acbdb09 )

	ROM_REGION( 0x10000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1101.35a",     0x0000, 0x10000, 0x84a1939f )
ROM_END

ROM_START( atetrisa )
	ROM_REGION( 0x14000, REGION_CPU1 )     /* 80k for code */
	ROM_LOAD( "d1",           0x0000, 0x10000, 0x2bcab107 )

	ROM_REGION( 0x10000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1101.35a",     0x0000, 0x10000, 0x84a1939f )
ROM_END

ROM_START( atetrisb )
	ROM_REGION( 0x14000, REGION_CPU1 )     /* 80k for code */
	ROM_LOAD( "tetris.01",    0x0000, 0x10000, 0x944d15f6 )

	ROM_REGION( 0x10000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tetris.02",    0x0000, 0x10000, 0x5c4e7258 )

	/* there's an extra EEPROM, maybe used for protection crack, which */
	/* however doesn't seem to be required to run the game in this driver. */
	ROM_REGION( 0x0800, REGION_USER1 )
	ROM_LOAD( "tetris.03",    0x0000, 0x0800, 0x26618c0b )
ROM_END

ROM_START( atetcktl )
	ROM_REGION( 0x14000, REGION_CPU1 )     /* 80k for code */
	ROM_LOAD( "tetcktl1.rom", 0x0000, 0x10000, 0x9afd1f4a )

	ROM_REGION( 0x10000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1103.35a",     0x0000, 0x10000, 0xec2a7f93 )
ROM_END

ROM_START( atetckt2 )
	ROM_REGION( 0x14000, REGION_CPU1 )     /* 80k for code */
	ROM_LOAD( "1102.45f",     0x0000, 0x10000, 0x1bd28902 )

	ROM_REGION( 0x10000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1103.35a",     0x0000, 0x10000, 0xec2a7f93 )
ROM_END



static void init_atetris(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);


    // Move the lower 16k to 0x10000
    memcpy(&RAM[0x10000], &RAM[0x00000], 0x4000);
    memset(&RAM[0x00000], 0, 0x4000);
}



GAME( 1988, atetris,  0,       atetris, atetris,  atetris, ROT0,   "Atari Games", "Tetris (set 1)" )
GAME( 1988, atetrisa, atetris, atetris, atetris,  atetris, ROT0,   "Atari Games", "Tetris (set 2)" )
GAME( 1988, atetrisb, atetris, atetris, atetris,  atetris, ROT0,   "bootleg",     "Tetris (bootleg)" )
GAME( 1989, atetcktl, atetris, atetris, atetcktl, atetris, ROT270, "Atari Games", "Tetris (Cocktail set 1)" )
GAME( 1989, atetckt2, atetris, atetris, atetcktl, atetris, ROT270, "Atari Games", "Tetris (Cocktail set 2)" )
