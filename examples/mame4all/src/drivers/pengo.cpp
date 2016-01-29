#include "../vidhrdw/pengo.cpp"

/***************************************************************************

Pengo memory map (preliminary)

driver by Nicola Salmoria

0000-7fff ROM
8000-83ff Video RAM
8400-87ff Color RAM
8800-8fff RAM

memory mapped ports:

read:
9000      DSW1
9040      DSW0
9080      IN1
90c0      IN0

write:
8ff2-8ffd 6 pairs of two bytes:
          the first byte contains the sprite image number (bits 2-7), Y flip (bit 0),
		  X flip (bit 1); the second byte the color
9005      sound voice 1 waveform (nibble)
9011-9013 sound voice 1 frequency (nibble)
9015      sound voice 1 volume (nibble)
900a      sound voice 2 waveform (nibble)
9016-9018 sound voice 2 frequency (nibble)
901a      sound voice 2 volume (nibble)
900f      sound voice 3 waveform (nibble)
901b-901d sound voice 3 frequency (nibble)
901f      sound voice 3 volume (nibble)
9022-902d Sprite coordinates, x/y pairs for 6 sprites
9040      interrupt enable
9041      sound enable
9042      palette bank selector
9043      flip screen
9044-9045 coin counters
9046      color lookup table bank selector
9047      character/sprite bank selector
9070      watchdog reset

Main clock: XTAL = 18.432 MHz
Z80 Clock: XTAL/6 = 3.072 MHz
Horizontal video frequency: HSYNC = XTAL/3/192/2 = 16 kHz
Video frequency: VSYNC = HSYNC/132/2 = 60.606060 Hz
VBlank duration: 1/VSYNC * (20/132) = 2500 us

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



void pengo_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( pengo_gfxbank_w );
int pengo_vh_start(void);
WRITE_HANDLER( pengo_flipscreen_w );
void pengo_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

extern unsigned char *pengo_soundregs;
WRITE_HANDLER( pengo_sound_enable_w );
WRITE_HANDLER( pengo_sound_w );

/* in machine/segacrpt.c */
void pengo_decode(void);



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_RAM },	/* video and color RAM, scratchpad RAM, sprite codes */
	{ 0x9000, 0x903f, input_port_3_r },	/* DSW1 */
	{ 0x9040, 0x907f, input_port_2_r },	/* DSW0 */
	{ 0x9080, 0x90bf, input_port_1_r },	/* IN1 */
	{ 0x90c0, 0x90ff, input_port_0_r },	/* IN0 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x83ff, videoram_w, &videoram, &videoram_size },
	{ 0x8400, 0x87ff, colorram_w, &colorram },
	{ 0x8800, 0x8fef, MWA_RAMROM },
	{ 0x8ff0, 0x8fff, MWA_RAM, &spriteram, &spriteram_size},
	{ 0x9000, 0x901f, pengo_sound_w, &pengo_soundregs },
	{ 0x9020, 0x902f, MWA_RAM, &spriteram_2 },
	{ 0x9040, 0x9040, interrupt_enable_w },
	{ 0x9041, 0x9041, pengo_sound_enable_w },
	{ 0x9042, 0x9042, MWA_NOP },
	{ 0x9043, 0x9043, pengo_flipscreen_w },
	{ 0x9044, 0x9046, MWA_NOP },
	{ 0x9047, 0x9047, pengo_gfxbank_w },
	{ 0x9070, 0x9070, MWA_NOP },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( pengo )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	/* the coin input must stay low for no less than 2 frames and no more */
	/* than 9 frames to pass the self test check. */
	/* Moreover, this way we avoid the game freezing until the user releases */
	/* the "coin" key. */
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	/* Coin Aux doesn't need IMPULSE to pass the test, but it still needs it */
	/* to avoid the freeze. */
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN3, 2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "30000" )
	PORT_DIPSETTING(    0x01, "50000" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x18, 0x10, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x18, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_BITX(    0x20, 0x20, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Rack Test", KEYCODE_F1, IP_JOY_NONE )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xc0, "Easy" )
	PORT_DIPSETTING(    0x80, "Medium" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x0c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x09, "2 Coins/1 Credit 5/3" )
	PORT_DIPSETTING(    0x05, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0d, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(    0x03, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0x0b, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x07, "1 Coin/2 Credits 5/11" )
	PORT_DIPSETTING(    0x0f, "1 Coin/2 Credits 4/9" )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0xf0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x90, "2 Coins/1 Credit 5/3" )
	PORT_DIPSETTING(    0x50, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xd0, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(    0x30, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0xb0, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x70, "1 Coin/2 Credits 5/11" )
	PORT_DIPSETTING(    0xf0, "1 Coin/2 Credits 4/9" )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_6C ) )
INPUT_PORTS_END



static struct GfxLayout tilelayout =
{
	8,8,	/* 8*8 characters */
    256,    /* 256 characters */
    2,  /* 2 bits per pixel */
    { 0, 4 },   /* the two bitplanes for 4 pixels are packed into one byte */
    { 8*8+0, 8*8+1, 8*8+2, 8*8+3, 0, 1, 2, 3 }, /* bits are packed in groups of four */
    { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
    16*8    /* every char takes 16 bytes */
};


static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 8*8, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 64 bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &tilelayout,      0, 32 },  /* first bank */
    { REGION_GFX1, 0x1000, &spritelayout,    0, 32 },
    { REGION_GFX2, 0x0000, &tilelayout,   4*32, 32 },  /* second bank */
    { REGION_GFX2, 0x1000, &spritelayout, 4*32, 32 },
    { -1 } /* end of array */
};


static struct namco_interface namco_interface =
{
	3072000/32,	/* sample rate */
	3,			/* number of voices */
	100,		/* playback volume */
	REGION_SOUND1	/* memory region */
};



static struct MachineDriver machine_driver_pengo =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
/*			18432000/6,	* 3.072 Mhz */
			3020000,	/* The correct speed is 3.072 Mhz, but 3.020 gives a more */
						/* accurate emulation speed (time for two attract mode */
						/* cycles after power up, until the high score list appears */
						/* for the second time: 3'39") */
			readmem,writemem,0,0,
			interrupt,1
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	36*8, 28*8, { 0*8, 36*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	32,4*64,
	pengo_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	pengo_vh_start,
	generic_vh_stop,
	pengo_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_NAMCO,
			&namco_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( pengo )
	ROM_REGION( 2*0x10000, REGION_CPU1 )     /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "ic8",          0x0000, 0x1000, 0xf37066a8 )
	ROM_LOAD( "ic7",          0x1000, 0x1000, 0xbaf48143 )
	ROM_LOAD( "ic15",         0x2000, 0x1000, 0xadf0eba0 )
	ROM_LOAD( "ic14",         0x3000, 0x1000, 0xa086d60f )
	ROM_LOAD( "ic21",         0x4000, 0x1000, 0xb72084ec )
	ROM_LOAD( "ic20",         0x5000, 0x1000, 0x94194a89 )
	ROM_LOAD( "ic32",         0x6000, 0x1000, 0xaf7b12c4 )
	ROM_LOAD( "ic31",         0x7000, 0x1000, 0x933950fe )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic92",         0x0000, 0x2000, 0xd7eec6cd )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic105",        0x0000, 0x2000, 0x5bfd26e9 )

	ROM_REGION( 0x0420, REGION_PROMS )
	ROM_LOAD( "pr1633.078",   0x0000, 0x0020, 0x3a5844ec )
	ROM_LOAD( "pr1634.088",   0x0020, 0x0400, 0x766b139b )

	ROM_REGION( 0x0200, REGION_SOUND1 )	/* sound PROMs */
	ROM_LOAD( "pr1635.051",   0x0000, 0x0100, 0xc29dea27 )
	ROM_LOAD( "pr1636.070",   0x0100, 0x0100, 0x77245b66 )	/* timing - not used */
ROM_END

ROM_START( pengo2 )
	ROM_REGION( 2*0x10000, REGION_CPU1 )     /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "ic8.2",        0x0000, 0x1000, 0xe4924b7b )
	ROM_LOAD( "ic7.2",        0x1000, 0x1000, 0x72e7775d )
	ROM_LOAD( "ic15.2",       0x2000, 0x1000, 0x7410ef1e )
	ROM_LOAD( "ic14.2",       0x3000, 0x1000, 0x55b3f379 )
	ROM_LOAD( "ic21",         0x4000, 0x1000, 0xb72084ec )
	ROM_LOAD( "ic20.2",       0x5000, 0x1000, 0x770570cf )
	ROM_LOAD( "ic32",         0x6000, 0x1000, 0xaf7b12c4 )
	ROM_LOAD( "ic31.2",       0x7000, 0x1000, 0x669555c1 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic92",         0x0000, 0x2000, 0xd7eec6cd )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic105",        0x0000, 0x2000, 0x5bfd26e9 )

	ROM_REGION( 0x0420, REGION_PROMS )
	ROM_LOAD( "pr1633.078",   0x0000, 0x0020, 0x3a5844ec )
	ROM_LOAD( "pr1634.088",   0x0020, 0x0400, 0x766b139b )

	ROM_REGION( 0x0200, REGION_SOUND1 )	/* sound PROMs */
	ROM_LOAD( "pr1635.051",   0x0000, 0x0100, 0xc29dea27 )
	ROM_LOAD( "pr1636.070",   0x0100, 0x0100, 0x77245b66 )	/* timing - not used */
ROM_END

ROM_START( pengo2u )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "pengo.u8",     0x0000, 0x1000, 0x3dfeb20e )
	ROM_LOAD( "pengo.u7",     0x1000, 0x1000, 0x1db341bd )
	ROM_LOAD( "pengo.u15",    0x2000, 0x1000, 0x7c2842d5 )
	ROM_LOAD( "pengo.u14",    0x3000, 0x1000, 0x6e3c1f2f )
	ROM_LOAD( "pengo.u21",    0x4000, 0x1000, 0x95f354ff )
	ROM_LOAD( "pengo.u20",    0x5000, 0x1000, 0x0fdb04b8 )
	ROM_LOAD( "pengo.u32",    0x6000, 0x1000, 0xe5920728 )
	ROM_LOAD( "pengo.u31",    0x7000, 0x1000, 0x13de47ed )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic92",         0x0000, 0x2000, 0xd7eec6cd )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic105",        0x0000, 0x2000, 0x5bfd26e9 )

	ROM_REGION( 0x0420, REGION_PROMS )
	ROM_LOAD( "pr1633.078",   0x0000, 0x0020, 0x3a5844ec )
	ROM_LOAD( "pr1634.088",   0x0020, 0x0400, 0x766b139b )

	ROM_REGION( 0x0200, REGION_SOUND1 )	/* sound PROMs */
	ROM_LOAD( "pr1635.051",   0x0000, 0x0100, 0xc29dea27 )
	ROM_LOAD( "pr1636.070",   0x0100, 0x0100, 0x77245b66 )	/* timing - not used */
ROM_END

ROM_START( penta )
	ROM_REGION( 2*0x10000, REGION_CPU1 )     /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "008_pn01.bin", 0x0000, 0x1000, 0x22f328df )
	ROM_LOAD( "007_pn05.bin", 0x1000, 0x1000, 0x15bbc7d3 )
	ROM_LOAD( "015_pn02.bin", 0x2000, 0x1000, 0xde82b74a )
	ROM_LOAD( "014_pn06.bin", 0x3000, 0x1000, 0x160f3836 )
	ROM_LOAD( "021_pn03.bin", 0x4000, 0x1000, 0x7824e3ef )
	ROM_LOAD( "020_pn07.bin", 0x5000, 0x1000, 0x377b9663 )
	ROM_LOAD( "032_pn04.bin", 0x6000, 0x1000, 0xbfde44c1 )
	ROM_LOAD( "031_pn08.bin", 0x7000, 0x1000, 0x64e8c30d )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "092_pn09.bin", 0x0000, 0x2000, 0x6afeba9d )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic105",        0x0000, 0x2000, 0x5bfd26e9 )

	ROM_REGION( 0x0420, REGION_PROMS )
	ROM_LOAD( "pr1633.078",   0x0000, 0x0020, 0x3a5844ec )
	ROM_LOAD( "pr1634.088",   0x0020, 0x0400, 0x766b139b )

	ROM_REGION( 0x0200, REGION_SOUND1 )	/* sound PROMs */
	ROM_LOAD( "pr1635.051",   0x0000, 0x0100, 0xc29dea27 )
	ROM_LOAD( "pr1636.070",   0x0100, 0x0100, 0x77245b66 )	/* timing - not used */
ROM_END



static void init_pengo(void)
{
	pengo_decode();
}

static void init_penta(void)
{
/*
	the values vary, but the translation mask is always laid out like this:

	  0 1 2 3 4 5 6 7 8 9 a b c d e f
	0 A A B B A A B B C C D D C C D D
	1 A A B B A A B B C C D D C C D D
	2 E E F F E E F F G G H H G G H H
	3 E E F F E E F F G G H H G G H H
	4 A A B B A A B B C C D D C C D D
	5 A A B B A A B B C C D D C C D D
	6 E E F F E E F F G G H H G G H H
	7 E E F F E E F F G G H H G G H H
	8 H H G G H H G G F F E E F F E E
	9 H H G G H H G G F F E E F F E E
	a D D C C D D C C B B A A B B A A
	b D D C C D D C C B B A A B B A A
	c H H G G H H G G F F E E F F E E
	d H H G G H H G G F F E E F F E E
	e D D C C D D C C B B A A B B A A
	f D D C C D D C C B B A A B B A A

	(e.g. 0xc0 is XORed with H)
	therefore in the following tables we only keep track of A, B, C, D, E, F, G and H.
*/
	static const unsigned char data_xortable[2][8] =
	{
		{ 0xa0,0x82,0x28,0x0a,0x82,0xa0,0x0a,0x28 },	/* ...............0 */
		{ 0x88,0x0a,0x82,0x00,0x88,0x0a,0x82,0x00 }		/* ...............1 */
	};
	static const unsigned char opcode_xortable[8][8] =
	{
		{ 0x02,0x08,0x2a,0x20,0x20,0x2a,0x08,0x02 },	/* ...0...0...0.... */
		{ 0x88,0x88,0x00,0x00,0x88,0x88,0x00,0x00 },	/* ...0...0...1.... */
		{ 0x88,0x0a,0x82,0x00,0xa0,0x22,0xaa,0x28 },	/* ...0...1...0.... */
		{ 0x88,0x0a,0x82,0x00,0xa0,0x22,0xaa,0x28 },	/* ...0...1...1.... */
		{ 0x2a,0x08,0x2a,0x08,0x8a,0xa8,0x8a,0xa8 },	/* ...1...0...0.... */
		{ 0x2a,0x08,0x2a,0x08,0x8a,0xa8,0x8a,0xa8 },	/* ...1...0...1.... */
		{ 0x88,0x0a,0x82,0x00,0xa0,0x22,0xaa,0x28 },	/* ...1...1...0.... */
		{ 0x88,0x0a,0x82,0x00,0xa0,0x22,0xaa,0x28 }		/* ...1...1...1.... */
	};
	int A;
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;


	memory_set_opcode_base(0,rom+diff);

	for (A = 0x0000;A < 0x8000;A++)
	{
		int i,j;
		unsigned char src;


		src = rom[A];

		/* pick the translation table from bit 0 of the address */
		i = A & 1;

		/* pick the offset in the table from bits 1, 3 and 5 of the source data */
		j = ((src >> 1) & 1) + (((src >> 3) & 1) << 1) + (((src >> 5) & 1) << 2);
		/* the bottom half of the translation table is the mirror image of the top */
		if (src & 0x80) j = 7 - j;

		/* decode the ROM data */
		rom[A] = src ^ data_xortable[i][j];

		/* now decode the opcodes */
		/* pick the translation table from bits 4, 8 and 12 of the address */
		i = ((A >> 4) & 1) + (((A >> 8) & 1) << 1) + (((A >> 12) & 1) << 2);
		rom[A + diff] = src ^ opcode_xortable[i][j];
	}
}



GAME( 1982, pengo,   0,     pengo, pengo, pengo, ROT90, "Sega", "Pengo (set 1)" )
GAME( 1982, pengo2,  pengo, pengo, pengo, pengo, ROT90, "Sega", "Pengo (set 2)" )
GAME( 1982, pengo2u, pengo, pengo, pengo, 0,     ROT90, "Sega", "Pengo (set 2 not encrypted)" )
GAME( 1982, penta,   pengo, pengo, pengo, penta, ROT90, "bootleg", "Penta" )
