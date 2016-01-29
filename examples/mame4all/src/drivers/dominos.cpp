#include "../machine/dominos.cpp"
#include "../vidhrdw/dominos.cpp"

/***************************************************************************

Atari Dominos Driver

Memory Map:
		0000-03FF		RAM
		0400-07FF		DISPLAY RAM
		0800-0BFF	R	SWITCH
		0C00-0FFF	R	SYNC
		0C00-0C0F	W	ATTRACT
		0C10-0C1F	W	TUMBLE
		0C30-0C3F	W	LAMP2
		0C40-0C4F	W	LAMP1
		3000-37FF		Program ROM1
		3800-3FFF		Program ROM2
	   (F800-FFFF)		Program ROM2 - only needed for the 6502 vectors

If you have any questions about how this driver works, don't hesitate to
ask.  - Mike Balfour (mab22@po.cwru.edu)
***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* machine/dominos.c */
READ_HANDLER( dominos_port_r );
READ_HANDLER( dominos_sync_r );
WRITE_HANDLER( dominos_attract_w );
WRITE_HANDLER( dominos_tumble_w );
WRITE_HANDLER( dominos_lamp2_w );
WRITE_HANDLER( dominos_lamp1_w );

/* vidhrdw/dominos.c */
extern void dominos_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM }, /* RAM */
	{ 0x0400, 0x07ff, MRA_RAM }, /* RAM */
	{ 0x0800, 0x083f, dominos_port_r }, /* SWITCH */
	{ 0x0840, 0x087f, input_port_3_r }, /* SWITCH */
	{ 0x0900, 0x093f, dominos_port_r }, /* SWITCH */
	{ 0x0940, 0x097f, input_port_3_r }, /* SWITCH */
	{ 0x0a00, 0x0a3f, dominos_port_r }, /* SWITCH */
	{ 0x0a40, 0x0a7f, input_port_3_r }, /* SWITCH */
	{ 0x0b00, 0x0b3f, dominos_port_r }, /* SWITCH */
	{ 0x0b40, 0x0b7f, input_port_3_r }, /* SWITCH */
	{ 0x0c00, 0x0fff, dominos_sync_r }, /* SYNC */
	{ 0x3000, 0x3fff, MRA_ROM }, /* ROM1-ROM2 */
	{ 0xfff0, 0xffff, MRA_ROM }, /* ROM2 for 6502 vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0400, 0x07ff, videoram_w, &videoram, &videoram_size }, /* DISPLAY */
	{ 0x0c00, 0x0c0f, dominos_attract_w }, /* ATTRACT */
	{ 0x0c10, 0x0c1f, dominos_tumble_w }, /* TUMBLE */
	{ 0x0c30, 0x0c3f, dominos_lamp2_w }, /* LAMP2 */
	{ 0x0c40, 0x0c4f, dominos_lamp1_w }, /* LAMP1 */
	{ 0x0c80, 0x0cff, MWA_NOP }, /* TIMER RESET */
	{ 0x3000, 0x3fff, MWA_ROM }, /* ROM1-ROM2 */
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( dominos )
		PORT_START		/* DSW - fake port, gets mapped to Dominos ports */
		PORT_DIPNAME( 0x03, 0x01, "Points To Win" )
		PORT_DIPSETTING(	0x03, "6" )
		PORT_DIPSETTING(	0x02, "5" )
		PORT_DIPSETTING(	0x01, "4" )
		PORT_DIPSETTING(	0x00, "3" )
		PORT_DIPNAME( 0x0C, 0x08, "Cost" )
		PORT_DIPSETTING(	0x0C, "2 players/coin" )
		PORT_DIPSETTING(	0x08, "1 coin/player" )
		PORT_DIPSETTING(	0x04, "2 coins/player" )
		PORT_DIPSETTING(	0x00, "2 coins/player" ) /* not a typo */

		PORT_START		/* IN0 - fake port, gets mapped to Dominos ports */
		PORT_BIT (0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_PLAYER2)
		PORT_BIT (0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2)
		PORT_BIT (0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_PLAYER2)
		PORT_BIT (0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_PLAYER2)
		PORT_BIT (0xF0, IP_ACTIVE_HIGH, IPT_UNKNOWN )

		PORT_START		/* IN1 - fake port, gets mapped to Dominos ports */
		PORT_BIT (0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
		PORT_BIT (0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
		PORT_BIT (0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
		PORT_BIT (0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
		PORT_BIT (0x10, IP_ACTIVE_LOW, IPT_START1 )
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_START2 )
		PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_SERVICE | IPF_TOGGLE, "Self Test", KEYCODE_F2, IP_JOY_NONE )

		PORT_START		/* IN2 */
		PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
		PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

		PORT_START		/* IN3 */
		PORT_BIT ( 0x0F, IP_ACTIVE_HIGH, IPT_UNKNOWN )
		PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* ATTRACT */
		PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_VBLANK )	/* VRESET */
		PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_VBLANK )	/* VBLANK* */
		PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* Alternating signal? */

INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	64, 	/* 64 characters */
	1,		/* 1 bit per pixel */
	{ 0 },		  /* no separation in 1 bpp */
	{ 4, 5, 6, 7, 0x200*8 + 4, 0x200*8 + 5, 0x200*8 + 6, 0x200*8 + 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8 /* every char takes 8 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout, 0x00, 0x02 }, /* offset into colors, # of colors */
	{ -1 } /* end of array */
};

static unsigned char palette[] =
{
	0x80,0x80,0x80, /* LT GREY */
	0x00,0x00,0x00, /* BLACK */
	0xff,0xff,0xff, /* WHITE */
	0x55,0x55,0x55, /* DK GREY */
};
static unsigned short colortable[] =
{
	0x00, 0x01,
	0x00, 0x02
};
static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
	memcpy(game_colortable,colortable,sizeof(colortable));
}


static struct MachineDriver machine_driver_dominos =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			750000, 	   /* 750 Khz ???? */
			readmem,writemem,0,0,
			interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 28*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	sizeof(palette) / sizeof(palette[0]) / 3, sizeof(colortable) / sizeof(colortable[0]),
	init_palette,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	dominos_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0

};





/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( dominos )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "7352-02.d1",   0x3000, 0x0800, 0x738b4413 )
	ROM_LOAD( "7438-02.e1",   0x3800, 0x0800, 0xc84e54e2 )
	ROM_RELOAD( 			  0xf800, 0x0800 )

	ROM_REGION( 0x800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "7439-01.p4",   0x0000, 0x0200, 0x4f42fdd6 )
	ROM_LOAD( "7440-01.r4",   0x0200, 0x0200, 0x957dd8df )
ROM_END



GAMEX( 1977, dominos, 0, dominos, dominos, 0, ROT0, "Atari", "Dominos", GAME_NO_SOUND )
