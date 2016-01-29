#include "../machine/subs.cpp"
#include "../vidhrdw/subs.cpp"

/***************************************************************************

Subs Driver

If you have any questions about how this driver works, don't hesitate to
ask.  - Mike Balfour (mab22@po.cwru.edu)
***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* vidhrdw/subs.c */
extern void subs_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( subs_invert1_w );
WRITE_HANDLER( subs_invert2_w );

/* machine/subs.c */
extern void subs_init_machine(void);
extern int subs_interrupt(void);
WRITE_HANDLER( subs_steer_reset_w );
READ_HANDLER( subs_control_r );
READ_HANDLER( subs_coin_r );
READ_HANDLER( subs_options_r );
WRITE_HANDLER( subs_lamp1_w );
WRITE_HANDLER( subs_lamp2_w );
WRITE_HANDLER( subs_noise_reset_w );
WRITE_HANDLER( subs_sonar2_w );
WRITE_HANDLER( subs_sonar1_w );
WRITE_HANDLER( subs_crash_w );
WRITE_HANDLER( subs_explode_w );

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x0007, subs_control_r },
	{ 0x0020, 0x0027, subs_coin_r },
	{ 0x0060, 0x0063, subs_options_r },
	{ 0x0000, 0x01ff, MRA_RAM },
	{ 0x0800, 0x0bff, MRA_RAM },
	{ 0x2000, 0x3fff, MRA_ROM },
	{ 0xf000, 0xffff, MRA_ROM }, /* A14/A15 unused, so mirror ROM */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x0000, subs_noise_reset_w },
	{ 0x0020, 0x0020, subs_steer_reset_w },
//	{ 0x0040, 0x0040, subs_timer_reset_w },
	{ 0x0060, 0x0061, subs_lamp1_w },
	{ 0x0062, 0x0063, subs_lamp2_w },
	{ 0x0064, 0x0065, subs_sonar2_w },
	{ 0x0066, 0x0067, subs_sonar1_w },
	{ 0x0068, 0x0069, subs_crash_w },
	{ 0x006a, 0x006b, subs_explode_w },
	{ 0x006c, 0x006d, subs_invert1_w },
	{ 0x006e, 0x006f, subs_invert2_w },
	{ 0x0090, 0x009f, spriteram_w, &spriteram },
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x0800, 0x0bff, videoram_w, &videoram, &videoram_size },
	{ 0x2000, 0x3fff, MWA_ROM },
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( subs )
	PORT_START /* OPTIONS */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Credit/Time" )
	PORT_DIPSETTING(    0x00, "Each Coin Buys Time" )
	PORT_DIPSETTING(    0x02, "Fixed Time" )
	PORT_DIPNAME( 0x0c, 0x00, "Game Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x04, "Spanish" )
	PORT_DIPSETTING(    0x08, "French" )
	PORT_DIPSETTING(    0x0c, "German" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0x40, "Game Length" )
	PORT_DIPSETTING(    0x00, "0:30 Minutes" )
	PORT_DIPSETTING(    0x20, "1:00 Minutes" )
	PORT_DIPSETTING(    0x40, "1:30 Minutes" )
	PORT_DIPSETTING(    0x60, "2:00 Minutes" )
	PORT_DIPSETTING(    0x80, "2:30 Minutes" )
	PORT_DIPSETTING(    0xa0, "3:00 Minutes" )
	PORT_DIPSETTING(    0xc0, "3:30 Minutes" )
	PORT_DIPSETTING(    0xe0, "4:00 Minutes" )

PORT_START /* IN1 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* Diag Step */
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* Diag Hold */
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_TILT )    /* Slam */
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )  /* Spare */
	PORT_BIT ( 0xf0, IP_ACTIVE_HIGH, IPT_UNUSED ) /* Filled in with steering information */

	PORT_START /* IN2 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BITX( 0x40, IP_ACTIVE_LOW, IPT_SERVICE | IPF_TOGGLE, "Self Test", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 )

PORT_START      /* IN3 */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL, 100, 10, 0, 0 )

PORT_START      /* IN4 */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_PLAYER2, 100, 10, 0, 0 )

INPUT_PORTS_END


static unsigned char palette[] =
{
	0x00,0x00,0x00, /* BLACK - modified on video invert */
	0xff,0xff,0xff, /* WHITE - modified on video invert */
	0x00,0x00,0x00, /* BLACK - modified on video invert */
	0xff,0xff,0xff, /* WHITE - modified on video invert*/
};
static unsigned short colortable[] =
{
	0x00, 0x01,		/* Right screen */
	0x02, 0x03		/* Left screen */
};
static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
	memcpy(game_colortable,colortable,sizeof(colortable));
}


static struct GfxLayout playfield_layout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	1,	/* 1 bits per pixel */
	{ 0 }, /* No info needed for bit offsets */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8 /* every char takes 8 consecutive bytes */
};

static struct GfxLayout motion_layout =
{
	16,16,	/* 16*16 characters */
	64,		/* 64 characters */
	1,	/* 1 bits per pixel */
	{ 0 }, /* No info needed for bit offsets */
	{ 3 + 0x400*8, 2 + 0x400*8, 1 + 0x400*8, 0 + 0x400*8,
	  7 + 0x400*8, 6 + 0x400*8, 5 + 0x400*8, 4 + 0x400*8,
	  3, 2, 1, 0, 7, 6, 5, 4 },
	{ 0, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*8 /* every char takes 16 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &playfield_layout, 0, 2 }, 	/* playfield graphics */
	{ REGION_GFX2, 0, &motion_layout,    0, 2 }, 	/* motion graphics */
	{ -1 } /* end of array */
};

static struct MachineDriver machine_driver_subs =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			12096000/16, 	   /* clock input is the "4H" signal */
			readmem,writemem,0,0,
			subs_interrupt,4	/* NMI interrupt on the 32V signal if not in self-TEST */
		}
	},
	57, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	subs_init_machine,

	/* video hardware */
	64*8, 32*8, { 0*8, 64*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo,
	sizeof(palette) / sizeof(palette[0]) / 3, sizeof(colortable) / sizeof(colortable[0]),
	init_palette,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_DUAL_MONITOR,
	0,
	generic_vh_start,
	generic_vh_stop,
	subs_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
};


/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( subs )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "34190.p1",     0x2800, 0x0800, 0xa88aef21 )
	ROM_LOAD( "34191.p2",     0x3000, 0x0800, 0x2c652e72 )
	ROM_LOAD( "34192.n2",     0x3800, 0x0800, 0x3ce63d33 )
	ROM_RELOAD(               0xf800, 0x0800 )
	/* Note: These are being loaded into a bogus location, */
	/*		 They are nibble wide rom images which will be */
	/*		 merged and loaded into the proper place by    */
	/*		 subs_rom_init()							   */
	ROM_LOAD( "34196.e2",     0x8000, 0x0100, 0x7c7a04c3 )	/* ROM 0 D4-D7 */
	ROM_LOAD( "34194.e1",     0x9000, 0x0100, 0x6b1c4acc )	/* ROM 0 D0-D3 */

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "34211.m4",     0x0000, 0x0800, 0xfa8d4409 )	/* Playfield */

	ROM_REGION( 0x0800, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "34216.d7",     0x0000, 0x0200, 0x941d28b4 )	/* Motion */
	ROM_LOAD( "34218.e7",     0x0200, 0x0200, 0xf4f4d874 )	/* Motion */
	ROM_LOAD( "34217.d8",     0x0400, 0x0200, 0xa7a60da3 )	/* Motion */
	ROM_LOAD( "34219.e8",     0x0600, 0x0200, 0x99a5a49b )	/* Motion */

ROM_END


static void init_subs(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int i;

	/* Merge nibble-wide roms together,
	   and load them into 0x2000-0x20ff */

	for(i=0;i<0x100;i++)
	{
		rom[0x2000+i] = (rom[0x8000+i]<<4)+rom[0x9000+i];
	}
}


GAMEX( 1977, subs, 0, subs, subs, subs, ROT0, "Atari", "Subs", GAME_NO_SOUND )
