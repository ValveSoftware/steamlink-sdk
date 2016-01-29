#include "../vidhrdw/skydiver.cpp"

/***************************************************************************

Atari Sky Diver Driver

Memory Map:
0000-00FF    R/W    PAGE ZERO RAM
0010         R/W    H POS PLANE 1
0011         R/W    H POS PLANE 2
0012         R/W    H POS MAN 1
0013         R/W    H POS MAN 2
0014         R/W    RANGE LOAD
0015         R/W    NOTE LOAD
0016         R/W    NAM LD
0017         R/W    UNUSED
0018         R/W    V POS PLANE 1
0019         R/W    PICTURE PLANE 1
001A         R/W    V POS PLANE 2
001B         R/W    PICTURE PLANE 2
001C         R/W    V POS MAN 1
001D         R/W    PICTURE MAN 1
001E         R/W    V POS MAN 2
001F         R/W    PICTURE MAN 2
0400-077F    R/W    PLAYFIELD
0780-07FF    R/W    MAPS TO 0000-D0
0800-0801     W     S LAMP
0802-0803     W     K LAMP
0804-0805     W     START LITE 1
0806-0807     W     START LITE 2
0808-0809     W     Y LAMP
080A-080B     W     D LAMP
080C-080D     W     SOUND ENABLE
1000-1001     W     JUMP LITE 1
1002-1003     W     COIN LOCK OUT
1006-1007     W     JUMP LITE 2
1008-1009     W     WHISTLE
100A-100B     W     WHISTLE 2
100C-100D     W     NMION
100E-100F     W     WIDTH
1800          R     D6=LEFT 1, D7=RIGHT 1
1801          R     D6=LEFT 2, D7=RIGHT 2
1802          R     D6=JUMP 1, D7=CHUTE 1
1803          R     D6=JUMP 2, D7=CHUTE 2
1804          R     D6=(D) OPT SW: NEXT TEST, D7=(F) OPT SW
1805          R     D6=(E) OPT SW, D7= (H) OPT SW: DIAGNOSTICS
1806          R     D6=START 1, D7=COIN 1
1807          R     D6=START 2, D7=COIN 2
1808          R     D6=MISSES 2, D7=MISSES 1
1809          R     D6=COIN 2, D7=COIN1
180A          R     D6=HARD/EASY, D7=EXTENDED PLAY
180B          R     D6=LANGUAGE 2, D7=LANGUAGE 1
1810          R     D6=TEST, D7=!VBLANK
1811          R     D6=!SLAM, D7=UNUSED
2000          W     TIMER RESET
2002-2003     W     I LAMP
2004-2005     W     V LAMP
2006-2007     W     E LAMP
2008-2009     W     R LAMP
200A-200B     W     OCT 1
200C-200D     W     OCT 2
200E-200F     W     NOISE RESET
2800-2FFF     R     ROM 0
3000-37FF     R     ROM 1
3800-3FFF     R     ROM 2A
7800-7FFF     R     ROM 2B

If you have any questions about how this driver works, don't hesitate to
ask.  - Mike Balfour (mab22@po.cwru.edu)

Notes:

The NMI interrupts are only used to read the coin switches.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* vidhrdw/skydiver.c */
WRITE_HANDLER( skydiver_sk_lamps_w );
WRITE_HANDLER( skydiver_yd_lamps_w );
WRITE_HANDLER( skydiver_iver_lamps_w );
WRITE_HANDLER( skydiver_width_w );
extern void skydiver_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

static int skydiver_nmion;

READ_HANDLER( skydiver_input_0_r )
{
	int data = input_port_0_r(0);

	switch(offset)
	{
		case 0:		return ((data & 0x03) << 6);
		case 1:		return ((data & 0x0C) << 4);
		case 2:		return ((data & 0x30) << 2);
		case 3:		return ((data & 0xC0) << 0);
		default:		return 0;
	}
}

READ_HANDLER( skydiver_input_1_r )
{
	int data = input_port_1_r(0);

	switch(offset)
	{
		case 0:		return ((data & 0x03) << 6);
		case 1:		return ((data & 0x0C) << 4);
		case 2:		return ((data & 0x30) << 2);
		case 3:		return ((data & 0xC0) << 0);
		default:		return 0;
	}
}

READ_HANDLER( skydiver_input_2_r )
{
	int data = input_port_2_r(0);

	switch(offset)
	{
		case 0:		return ((data & 0x03) << 6);
		case 1:		return ((data & 0x0C) << 4);
		case 2:		return ((data & 0x30) << 2);
		case 3:		return ((data & 0xC0) << 0);
		default:		return 0;
	}
}

READ_HANDLER( skydiver_input_3_r )
{
	int data = input_port_3_r(0);

	switch(offset)
	{
		case 0:		return ((data & 0x03) << 6);
		case 1:		return ((data & 0x0C) << 4);
		case 2:		return ((data & 0x30) << 2);
		case 3:		return ((data & 0xC0) << 0);
		default:		return 0;
	}
}

WRITE_HANDLER( skydiver_nmion_w )
{
//	logerror("nmi_on: %02x:%02x\n", offset, data);
	skydiver_nmion = offset;
}

int skydiver_interrupt(void)
{
	if (skydiver_nmion)
		return nmi_interrupt();
	else
	   	return ignore_interrupt();
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x00ff, MRA_RAM },
	{ 0x0400, 0x077f, MRA_RAM },
//  { 0x780, 0x7ff, MRA_RAM },
	{ 0x1800, 0x1803, skydiver_input_0_r },
	{ 0x1804, 0x1807, skydiver_input_1_r },
	{ 0x1808, 0x180b, skydiver_input_2_r },
	{ 0x1810, 0x1811, skydiver_input_3_r },
	{ 0x2000, 0x2000, watchdog_reset_r },
	{ 0x2800, 0x3fff, MRA_ROM },
	{ 0x7800, 0x7fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x00ff, MWA_RAM },
	{ 0x0010, 0x001f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x0400, 0x077f, videoram_w, &videoram, &videoram_size },
	// { 0x0780, 0x07ff, MWA_RAM },
	{ 0x0800, 0x0803, skydiver_sk_lamps_w },
	// { 0x0804, 0x0807, skydiver_start_lamps_w },
	{ 0x0808, 0x080b, skydiver_yd_lamps_w },
	// { 0x080c, 0x080d, skydiver_sound_enable_w },
	// { 0x1000, 0x1001, skydiver_jump1_lamps_w },
	// { 0x1002, 0x1003, skydiver_coin_lockout_w },
	// { 0x1006, 0x1007, skydiver_jump2_lamps_w },
	// { 0x1008, 0x100b, skydiver_whistle_w },
	{ 0x100c, 0x100d, skydiver_nmion_w },
	{ 0x100e, 0x100f, skydiver_width_w },
	{ 0x2002, 0x2009, skydiver_iver_lamps_w },
	// { 0x200a, 0x200d, skydiver_oct_w },
	// { 0x200e, 0x200f, skydiver_noise_reset_w },
	{ 0x2800, 0x3fff, MWA_ROM },
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( skydiver )
	PORT_START /* fake port, gets mapped to Sky Diver ports */
	PORT_BIT (0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT (0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT (0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 )
	PORT_BIT (0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT (0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )	/* Jump 1 */
	PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )	/* Chute 1 */
	PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )	/* Jump 2 */
	PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )	/* Chute 2 */

	PORT_START		/* fake port, gets mapped to Sky Diver ports */
	PORT_BITX(0x01, IP_ACTIVE_LOW, IPT_UNKNOWN, "(D) OPT SW NEXT TEST", KEYCODE_D, IP_JOY_NONE )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_UNKNOWN, "(F) OPT SW", KEYCODE_F, IP_JOY_NONE )
	PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_UNKNOWN, "(E) OPT SW", KEYCODE_E, IP_JOY_NONE )
	PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_UNKNOWN, "(H) OPT SW DIAGNOSTICS", KEYCODE_H, IP_JOY_NONE )
	PORT_BIT (0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN1, 1 )
	PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN2, 1 )

	PORT_START		/* fake port, gets mapped to Sky Diver ports */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x08, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x20, 0x00, "Extended Play" )
	PORT_DIPSETTING(    0x20, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0xc0, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x40, "French" )
	PORT_DIPSETTING(    0x80, "Spanish" )
	PORT_DIPSETTING(    0xc0, "German" )

	PORT_START		/* fake port, gets mapped to Sky Diver ports */
	PORT_BITX(0x01, IP_ACTIVE_LOW, IPT_SERVICE | IPF_TOGGLE, "Self Test", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT (0x02, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT (0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT (0xF8, IP_ACTIVE_HIGH, IPT_UNUSED )

INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	64, 	/* 64 characters */
	1,		/* 1 bit per pixel */
	{ 0 },		  /* no separation in 1 bpp */
	{ 7, 6, 5, 4, 15, 14, 13, 12 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	8*16 /* every char takes 16 consecutive bytes */
};

static struct GfxLayout motion_layout =
{
	16,16,	/* 16*16 characters */
	32, 	/* 32 characters */
	1,		/* 1 bit per pixel */
	{ 0 },		  /* no separation in 1 bpp */
	{ 4, 5, 6, 7, 4 + 0x400*8, 5 + 0x400*8, 6 + 0x400*8, 7 + 0x400*8,
	  12, 13, 14, 15, 12 + 0x400*8, 13 + 0x400*8, 14 + 0x400*8, 15 + 0x400*8 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
	  8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	8*32 /* every char takes 32 consecutive bytes */
};

static struct GfxLayout wide_motion_layout =
{
	32,16,	/* 32*16 characters */
	32, 	/* 32 characters */
	1,		/* 1 bit per pixel */
	{ 0 },		  /* no separation in 1 bpp */
	{ 4, 4, 5, 5, 6, 6, 7, 7,
	  4 + 0x400*8, 4 + 0x400*8, 5 + 0x400*8, 5 + 0x400*8,
	  6 + 0x400*8, 6 + 0x400*8, 7 + 0x400*8, 7 + 0x400*8,
	  12, 12, 13, 13, 14, 14, 15, 15,
	  12 + 0x400*8, 12 + 0x400*8, 13 + 0x400*8, 13 + 0x400*8,
	  14 + 0x400*8, 14 + 0x400*8, 15 + 0x400*8, 15 + 0x400*8 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
	  8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	8*32 /* every char takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,         0, 6 },
	{ REGION_GFX2, 0, &motion_layout,      0, 6 },
	{ REGION_GFX2, 0, &wide_motion_layout, 0, 6 },
	{ -1 } /* end of array */
};

#if 0
static unsigned char palette[] =
{
	0x00,0x00,0x00, /* BLACK */
	0xff,0xff,0xff, /* WHITE */
	0x80,0x80,0x80, /* GREY */
};
#else
static unsigned char palette[] =
{
	0x00,0x00,0x00, /* BLACK */
	0xbf,0xbf,0xff, /* LT BLUE */
	0x7f,0x7f,0xff, /* BLUE */
};
#endif

static unsigned short colortable[] =
{
	0x02, 0x01,
	0x02, 0x00,
	0x01, 0x02,
	0x00, 0x02,
	0x00, 0x00, /* used only to draw the SKYDIVER LEDs */
	0x00, 0x01, /* used only to draw the SKYDIVER LEDs */
};
static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
	memcpy(game_colortable,colortable,sizeof(colortable));
}


static struct MachineDriver machine_driver_skydiver =
{
	/* basic machine hardware */
	{
		{
			CPU_M6800,
			3000000/4,	   /* ???? */
			readmem,writemem,0,0,
			skydiver_interrupt,8
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 0*8, 29*8-1 },
	gfxdecodeinfo,
	sizeof(palette) / sizeof(palette[0]) / 3, sizeof(colortable) / sizeof(colortable[0]),
	init_palette,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	skydiver_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0

};





/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( skydiver )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "33167-02.f1", 0x2800, 0x0800, 0x25a5c976 )
	ROM_LOAD( "33164-02.e1", 0x3000, 0x0800, 0xa348ac39 )
	ROM_LOAD( "33165-02.d1", 0x3800, 0x0800, 0xa1fc5504 )
	ROM_LOAD( "33166-02.c1", 0x7800, 0x0800, 0x3d26da2b )
	ROM_RELOAD(              0xF800, 0x0800 )

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "33163-01.h5", 0x0000, 0x0400, 0x5b9bb7c2 )

	ROM_REGION( 0x0800, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "33176-01.l5", 0x0000, 0x0400, 0x6b082a01 )
	ROM_LOAD( "33177-01.k5", 0x0400, 0x0400, 0xf5541af0 )
ROM_END



GAMEX( 1978, skydiver, 0, skydiver, skydiver, 0, ROT0, "Atari", "Sky Diver", GAME_NO_SOUND )
