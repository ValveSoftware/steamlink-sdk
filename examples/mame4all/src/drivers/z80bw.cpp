#include "../sndhrdw/z80bw.cpp"

/***************************************************************************

Midway?? Z80 board memory map (preliminary)


0000-1bff ROM
2000-23ff RAM
2400-3fff Video RAM  (also writes between 4000 and 4fff, but only to simplify
                      code, ie. doesn't do clipping when the spaceship scrolls in)

Games which are referenced by this driver:
------------------------------------------
Astro Invader (astinvad)
Kamikaze (kamikaze)
Space Intruder (spaceint)

I/O ports:
read:
08        IN0
09        IN1
0a        IN2

write:
04        sound
05        sound
07		  ???
0b		  ???

TODO:

- How many sets of controls are there on an upright machine?

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

int  astinvad_interrupt(void);

int invaders_vh_start(void);
void invaders_vh_stop(void);

void invadpt2_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( invaders_videoram_w );
void invaders_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( astinvad_sh_port_4_w );
WRITE_HANDLER( astinvad_sh_port_5_w );
void astinvad_sh_update(void);

void init_astinvad(void);
void init_spaceint(void);

extern struct Samplesinterface astinvad_samples_interface;


static struct MemoryReadAddress astinvad_readmem[] =
{
	{ 0x0000, 0x1bff, MRA_ROM },
	{ 0x1c00, 0x3fff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress astinvad_writemem[] =
{
	{ 0x0000, 0x1bff, MWA_ROM },
	{ 0x1c00, 0x23ff, MWA_RAM },
	{ 0x2400, 0x3fff, invaders_videoram_w, &videoram, &videoram_size },
	{ -1 }  /* end of table */
};


static struct IOReadPort astinvad_readport[] =
{
	{ 0x08, 0x08, input_port_0_r },
	{ 0x09, 0x09, input_port_1_r },
	{ 0x0a, 0x0a, input_port_2_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort astinvad_writeport[] =
{
	{ 0x04, 0x04, astinvad_sh_port_4_w },
	{ 0x05, 0x05, astinvad_sh_port_5_w },
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( astinvad )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x02, "10000" )
	PORT_DIPSETTING(    0x00, "20000" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x88, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x88, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )

	PORT_START      /* IN2 */
	PORT_DIPNAME( 0x01, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_BIT( 0xfe, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* FAKE - select cabinet type */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_BIT( 0xfe, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( kamikaze )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x88, 0x88, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x88, "5000" )
	PORT_DIPSETTING(    0x80, "10000" )
	PORT_DIPSETTING(    0x08, "15000" )
	PORT_DIPSETTING(    0x00, "20000" )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )

	PORT_START      /* IN2 */
	PORT_DIPNAME( 0x01, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_BIT( 0xfe, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* FAKE - select cabinet type */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_BIT( 0xfe, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END

static struct MachineDriver machine_driver_astinvad = /* LT */
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			2000000,        /* 2 Mhz? */
			astinvad_readmem,astinvad_writemem,astinvad_readport,astinvad_writeport,
			interrupt,1    /* two interrupts per frame */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,      /* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	0,      /* no gfxdecodeinfo - bitmapped display */
	8, 0,
	invadpt2_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY | VIDEO_MODIFIES_PALETTE,
	0,
	invaders_vh_start,
	invaders_vh_stop,
	invaders_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0,
	{
		{
			SOUND_SAMPLES,
			&astinvad_samples_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/


/*------------------------------------------------------------------------------
 Shoei Space Intruder
 Added By Lee Taylor (lee@defender.demon.co.uk)
 December 1998
------------------------------------------------------------------------------*/


static int spaceint_interrupt(void)
{
	if (readinputport(2) & 1)	/* Coin */
		return nmi_interrupt();
	else return interrupt();
}


static struct MemoryReadAddress spaceint_readmem[] =
{
	{ 0x0000, 0x17ff, MRA_ROM },
	{ 0x2000, 0x23ff, MRA_RAM },
	{ 0x4000, 0x5fff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress spaceint_writemem[] =
{
	{ 0x0000, 0x17ff, MWA_ROM },
	{ 0x2000, 0x23ff, MWA_RAM },
	{ 0x4000, 0x40ff, MWA_RAM },
	{ 0x4100, 0x5fff, invaders_videoram_w, &videoram, &videoram_size },
	{ -1 }  /* end of table */
};


static struct IOReadPort spaceint_readport[] =
{
	{ 0x00, 0x00, input_port_0_r },
	{ 0x01, 0x01, input_port_1_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort spaceint_writeport[] =
{
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( spaceint )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2  )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY  )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2)

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x04, "4" )
  //PORT_DIPSETTING(    0x06, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
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

	PORT_START	/* FAKE */
	/* The coin slots are not memory mapped. Coin insertion causes a NMI. */
	/* This fake input port is used by the interrupt */
	/* handler to be notified of coin insertions. We use IMPULSE to */
	/* trigger exactly one interrupt, without having to check when the */
	/* user releases the key. */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 1 )

	PORT_START	/* FAKE - select cabinet type */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_BIT( 0xfe, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END


static struct MachineDriver machine_driver_spaceint = /* 20-12-1998 LT */
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			2000000,        /* 2 Mhz? */
			spaceint_readmem,spaceint_writemem,spaceint_readport,spaceint_writeport,
			spaceint_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,      /* single CPU, no need for interleaving */
	0,

	/* video hardware */
	//32*8, 32*8, { 0*8, 32*8-1, 0*8, 32*8-1 },
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 32*8-1 },
	0,      /* no gfxdecodeinfo - bitmapped display */
	8, 0,
	invadpt2_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY | VIDEO_MODIFIES_PALETTE,
	0,
	invaders_vh_start,
	invaders_vh_stop,
	invaders_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0,
	{
		{
			SOUND_SAMPLES,
			&astinvad_samples_interface
		}
	}
};






ROM_START( astinvad )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "ai_cpu_1.rom", 0x0000, 0x0400, 0x20e3ec41 )
	ROM_LOAD( "ai_cpu_2.rom", 0x0400, 0x0400, 0xe8f1ab55 )
	ROM_LOAD( "ai_cpu_3.rom", 0x0800, 0x0400, 0xa0092553 )
	ROM_LOAD( "ai_cpu_4.rom", 0x0c00, 0x0400, 0xbe14185c )
	ROM_LOAD( "ai_cpu_5.rom", 0x1000, 0x0400, 0xfee681ec )
	ROM_LOAD( "ai_cpu_6.rom", 0x1400, 0x0400, 0xeb338863 )
	ROM_LOAD( "ai_cpu_7.rom", 0x1800, 0x0400, 0x16dcfea4 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "ai_vid_c.rom", 0x0000, 0x0400, 0xb45287ff )
ROM_END

ROM_START( kamikaze )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "km01",         0x0000, 0x0800, 0x8aae7414 )
	ROM_LOAD( "km02",         0x0800, 0x0800, 0x6c7a2beb )
	ROM_LOAD( "km03",         0x1000, 0x0800, 0x3e8dedb6 )
	ROM_LOAD( "km04",         0x1800, 0x0800, 0x494e1f6d )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "ai_vid_c.rom", 0x0000, 0x0400, BADCRC(0xb45287ff) )
ROM_END

ROM_START( spaceint )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "1",	 0x0000, 0x0400, 0x184314d2 )
	ROM_LOAD( "2",	 0x0400, 0x0400, 0x55459aa1 )
	ROM_LOAD( "3",	 0x0800, 0x0400, 0x9d6819be )
	ROM_LOAD( "4",	 0x0c00, 0x0400, 0x432052d4 )
	ROM_LOAD( "5",	 0x1000, 0x0400, 0xc6cfa650 )
	ROM_LOAD( "6",	 0x1400, 0x0400, 0xc7ccf40f )

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "clr", 0x0000, 0x0100, 0x13c1803f )
ROM_END



GAME( 1980, astinvad, 0,        astinvad, astinvad, astinvad, ROT270, "Stern", "Astro Invader" )
GAME( 1979, kamikaze, astinvad, astinvad, kamikaze, astinvad, ROT270, "Leijac Corporation", "Kamikaze" )
GAMEX(1980, spaceint, 0,        spaceint, spaceint, spaceint, ROT0,   "Shoei", "Space Intruder", GAME_WRONG_COLORS | GAME_NO_SOUND | GAME_NO_COCKTAIL )
