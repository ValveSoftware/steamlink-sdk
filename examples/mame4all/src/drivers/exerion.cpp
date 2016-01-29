#include "../vidhrdw/exerion.cpp"

/***************************************************************************

Exerion by Jaleco

Exerion is a unique driver in that it has idiosyncracies that are straight
out of Bizarro World. I submit for your approval:

* The mystery reads from $d802 - timer-based protection?
* The freakish graphics encoding scheme, which no other MAME-supported game uses
* The sprite-ram, and all the funky parameters that go along with it
* The unusual parallaxed background. Is it controlled by the 2nd CPU?

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


void exerion_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable, const unsigned char *color_prom);
int exerion_vh_start(void);
void exerion_vh_stop(void);
void exerion_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);

WRITE_HANDLER( exerion_videoreg_w );
WRITE_HANDLER( exerion_video_latch_w );
READ_HANDLER( exerion_video_timing_r );

extern UINT8 exerion_cocktail_flip;


/*********************************************************************
 * Interrupts & inputs
 *********************************************************************/

static READ_HANDLER( exerion_port01_r )
{
	/* the cocktail flip bit muxes between ports 0 and 1 */
	return exerion_cocktail_flip ? input_port_1_r(offset) : input_port_0_r(offset);
}


static READ_HANDLER( exerion_port3_r )
{
	/* bit 0 is VBLANK, which we simulate manually */
	int result = input_port_3_r(offset);
	int ybeam = cpu_getscanline();
	if (ybeam > Machine->visible_area.max_y)
		result |= 1;
	return result;
}


static int exerion_interrupt(void)
{
	/* Exerion triggers NMIs on coin insertion */
	if (readinputport(4) & 1)
		return nmi_interrupt();
	else return ignore_interrupt();
}



/*********************************************************************
 * Protection??
 *********************************************************************/

/* This is the first of many Exerion "features." No clue if it's */
/* protection or some sort of timer. */
static UINT8 porta;
static UINT8 portb;

static READ_HANDLER( exerion_porta_r )
{
	porta ^= 0x40;
	return porta;
}

static WRITE_HANDLER( exerion_portb_w )
{
	/* pull the expected value from the ROM */
	porta = memory_region(REGION_CPU1)[0x5f76];
	portb = data;

	//logerror("Port B = %02X\n", data);
}

static READ_HANDLER( exerion_protection_r )
{
	UINT8 *RAM = memory_region(REGION_CPU1);

	if (cpu_get_pc() == 0x4143)
		return RAM[0x33c0 + (RAM[0x600d] << 2) + offset];
	else
		return RAM[0x6008 + offset];
}



/*********************************************************************
 * CPU memory structures
 *********************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6008, 0x600b, exerion_protection_r },
	{ 0x6000, 0x67ff, MRA_RAM },
	{ 0x8000, 0x8bff, MRA_RAM },
	{ 0xa000, 0xa000, exerion_port01_r },
	{ 0xa800, 0xa800, input_port_2_r },
	{ 0xb000, 0xb000, exerion_port3_r },
	{ 0xd802, 0xd802, AY8910_read_port_1_r },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x67ff, MWA_RAM },
	{ 0x8000, 0x87ff, videoram_w, &videoram, &videoram_size },
	{ 0x8800, 0x887f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x8800, 0x8bff, MWA_RAM },
	{ 0xc000, 0xc000, exerion_videoreg_w },
	{ 0xc800, 0xc800, soundlatch_w },
	{ 0xd000, 0xd000, AY8910_control_port_0_w },
	{ 0xd001, 0xd001, AY8910_write_port_0_w },
	{ 0xd800, 0xd800, AY8910_control_port_1_w },
	{ 0xd801, 0xd801, AY8910_write_port_1_w },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress cpu2_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x6000, 0x6000, soundlatch_r },
	{ 0xa000, 0xa000, exerion_video_timing_r },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress cpu2_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x4000, 0x47ff, MWA_RAM },
	{ 0x8000, 0x800c, exerion_video_latch_w },
	{ -1 }  /* end of table */
};



/*********************************************************************
 * Input port definitions
 *********************************************************************/

INPUT_PORTS_START( exerion )
	PORT_START      /* player 1 inputs (muxed on 0xa000) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START      /* player 2 inputs (muxed on 0xa000) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START      /* dip switches (0xa800) */
	PORT_DIPNAME( 0x07, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x03, "4" )
	PORT_DIPSETTING(    0x04, "5" )
	PORT_BITX(0,        0x07, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "10000" )
	PORT_DIPSETTING(    0x08, "20000" )
	PORT_DIPSETTING(    0x10, "30000" )
	PORT_DIPSETTING(    0x18, "40000" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )      /* used */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "Hard" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START      /* dip switches/VBLANK (0xb000) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )		/* VBLANK */
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* FAKE */
	/* The coin slots are not memory mapped. */
	/* This fake input port is used by the interrupt */
	/* handler to be notified of coin insertions. We use IMPULSE to */
	/* trigger exactly one interrupt, without having to check when the */
	/* user releases the key. */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
INPUT_PORTS_END



/*********************************************************************
 * Graphics layouts
 *********************************************************************/

static struct GfxLayout charlayout =
{
	8,8,            /* 8*8 characters */
	512,          /* total number of chars */
	2,              /* 2 bits per pixel (# of planes) */
	{ 0, 4 },       /* start of every bitplane */
	{ 3, 2, 1, 0, 8+3, 8+2, 8+1, 8+0 },
	{ 16*0, 16*1, 16*2, 16*3, 16*4, 16*5, 16*6, 16*7 },
	16*8            /* every char takes 16 consecutive bytes */
};

/* 16 x 16 sprites -- requires reorganizing characters in init_exerion() */
static struct GfxLayout spritelayout =
{
	16,16,          /* 16*16 sprites */
	128*2,          /* total number of sprites in the rom */
	2,              /* 2 bits per pixel (# of planes) */
	{ 0, 4 },       /* start of every bitplane */
	{  3, 2, 1, 0, 8+3, 8+2, 8+1, 8+0,
			16+3, 16+2, 16+1, 16+0, 24+3, 24+2, 24+1, 24+0 },
	{ 32*0, 32*1, 32*2, 32*3, 32*4, 32*5, 32*6, 32*7,
			32*8, 32*9, 32*10, 32*11, 32*12, 32*13, 32*14, 32*15 },
	64*8            /* every sprite takes 64 consecutive bytes */
};

/* Quick and dirty way to emulate pixel-doubled sprites. */
static struct GfxLayout bigspritelayout =
{
	32,32,          /* 32*32 sprites */
	128*2,          /* total number of sprites in the rom */
	2,              /* 2 bits per pixel (# of planes) */
	{ 0, 4 },       /* start of every bitplane */
	{  3, 3, 2, 2, 1, 1, 0, 0,
			8+3, 8+3, 8+2, 8+2, 8+1, 8+1, 8+0, 8+0,
			16+3, 16+3, 16+2, 16+2, 16+1, 16+1, 16+0, 16+0,
			24+3, 24+3, 24+2, 24+2, 24+1, 24+1, 24+0, 24+0 },
	{ 32*0, 32*0, 32*1, 32*1, 32*2, 32*2, 32*3, 32*3,
			32*4, 32*4, 32*5, 32*5, 32*6, 32*6, 32*7, 32*7,
			32*8, 32*8, 32*9, 32*9, 32*10, 32*10, 32*11, 32*11,
			32*12, 32*12, 32*13, 32*13, 32*14, 32*14, 32*15, 32*15 },
	64*8            /* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,         0, 64 },
	{ REGION_GFX2, 0, &spritelayout,     256, 64 },
	{ REGION_GFX2, 0, &bigspritelayout,  256, 64 },
	{ -1 } /* end of array */
};



/*********************************************************************
 * Sound interfaces
 *********************************************************************/

static struct AY8910interface ay8910_interface =
{
	2,  /* 2 chips */
	10000000/6, /* 1.666 MHz */
	{ 30, 30 },
	{ 0, exerion_porta_r },
	{ 0 },
	{ 0 },
	{ 0, exerion_portb_w }
};



/*********************************************************************
 * Machine driver
 *********************************************************************/

static struct MachineDriver machine_driver_exerion =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			10000000/3, /* 3.333 MHz */
			readmem,writemem,0,0,
			exerion_interrupt,1
		},
		{
			CPU_Z80,
			10000000/3, /* 3.333 MHz */
			cpu2_readmem,cpu2_writemem,0,0,
			ignore_interrupt,0
		}
	},
	60, 0,  /* frames per second, vblank duration */
	1,  /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 12*8, 52*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32,256*3,
	exerion_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	exerion_vh_start,
	exerion_vh_stop,
	exerion_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};



/*********************************************************************
 * ROM definitions
 *********************************************************************/

ROM_START( exerion )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "exerion.07",   0x0000, 0x2000, 0x4c78d57d )
	ROM_LOAD( "exerion.08",   0x2000, 0x2000, 0xdcadc1df )
	ROM_LOAD( "exerion.09",   0x4000, 0x2000, 0x34cc4d14 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the second CPU */
	ROM_LOAD( "exerion.05",   0x0000, 0x2000, 0x32f6bff5 )

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "exerion.06",   0x00000, 0x2000, 0x435a85a4 ) /* fg chars */

	ROM_REGION( 0x04000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "exerion.11",   0x00000, 0x2000, 0xf0633a09 ) /* sprites */
	ROM_LOAD( "exerion.10",   0x02000, 0x2000, 0x80312de0 )

	ROM_REGION( 0x08000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "exerion.03",   0x00000, 0x2000, 0x790595b8 ) /* bg data */
	ROM_LOAD( "exerion.04",   0x02000, 0x2000, 0xd7abd0b9 )
	ROM_LOAD( "exerion.01",   0x04000, 0x2000, 0x5bb755cb )
	ROM_LOAD( "exerion.02",   0x06000, 0x2000, 0xa7ecbb70 )

	ROM_REGION( 0x0420, REGION_PROMS )
	ROM_LOAD( "exerion.e1",   0x0000, 0x0020, 0x2befcc20 ) /* palette */
	ROM_LOAD( "exerion.i8",   0x0020, 0x0100, 0x31db0e08 ) /* fg char lookup table */
	ROM_LOAD( "exerion.h10",  0x0120, 0x0100, 0xcdd23f3e ) /* sprite lookup table */
	ROM_LOAD( "exerion.i3",   0x0220, 0x0100, 0xfe72ab79 ) /* bg char lookup table */
	ROM_LOAD( "exerion.k4",   0x0320, 0x0100, 0xffc2ba43 ) /* bg char mixer */
ROM_END

ROM_START( exeriont )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "prom5.4p",     0x0000, 0x4000, 0x58b4dc1b )
	ROM_LOAD( "prom6.4s",     0x4000, 0x2000, 0xfca18c2d )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the second CPU */
	ROM_LOAD( "exerion.05",   0x0000, 0x2000, 0x32f6bff5 )

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "exerion.06",   0x00000, 0x2000, 0x435a85a4 ) /* fg chars */

	ROM_REGION( 0x04000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "exerion.11",   0x00000, 0x2000, 0xf0633a09 ) /* sprites */
	ROM_LOAD( "exerion.10",   0x02000, 0x2000, 0x80312de0 )

	ROM_REGION( 0x08000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "exerion.03",   0x00000, 0x2000, 0x790595b8 ) /* bg data */
	ROM_LOAD( "exerion.04",   0x02000, 0x2000, 0xd7abd0b9 )
	ROM_LOAD( "exerion.01",   0x04000, 0x2000, 0x5bb755cb )
	ROM_LOAD( "exerion.02",   0x06000, 0x2000, 0xa7ecbb70 )

	ROM_REGION( 0x0420, REGION_PROMS )
	ROM_LOAD( "exerion.e1",   0x0000, 0x0020, 0x2befcc20 ) /* palette */
	ROM_LOAD( "exerion.i8",   0x0020, 0x0100, 0x31db0e08 ) /* fg char lookup table */
	ROM_LOAD( "exerion.h10",  0x0120, 0x0100, 0xcdd23f3e ) /* sprite lookup table */
	ROM_LOAD( "exerion.i3",   0x0220, 0x0100, 0xfe72ab79 ) /* bg char lookup table */
	ROM_LOAD( "exerion.k4",   0x0320, 0x0100, 0xffc2ba43 ) /* bg char mixer */
ROM_END

ROM_START( exerionb )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "eb5.bin",      0x0000, 0x4000, 0xda175855 )
	ROM_LOAD( "eb6.bin",      0x4000, 0x2000, 0x0dbe2eff )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the second CPU */
	ROM_LOAD( "exerion.05",   0x0000, 0x2000, 0x32f6bff5 )

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "exerion.06",   0x00000, 0x2000, 0x435a85a4 ) /* fg chars */

	ROM_REGION( 0x04000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "exerion.11",   0x00000, 0x2000, 0xf0633a09 ) /* sprites */
	ROM_LOAD( "exerion.10",   0x02000, 0x2000, 0x80312de0 )

	ROM_REGION( 0x08000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "exerion.03",   0x00000, 0x2000, 0x790595b8 ) /* bg data */
	ROM_LOAD( "exerion.04",   0x02000, 0x2000, 0xd7abd0b9 )
	ROM_LOAD( "exerion.01",   0x04000, 0x2000, 0x5bb755cb )
	ROM_LOAD( "exerion.02",   0x06000, 0x2000, 0xa7ecbb70 )

	ROM_REGION( 0x0420, REGION_PROMS )
	ROM_LOAD( "exerion.e1",   0x0000, 0x0020, 0x2befcc20 ) /* palette */
	ROM_LOAD( "exerion.i8",   0x0020, 0x0100, 0x31db0e08 ) /* fg char lookup table */
	ROM_LOAD( "exerion.h10",  0x0120, 0x0100, 0xcdd23f3e ) /* sprite lookup table */
	ROM_LOAD( "exerion.i3",   0x0220, 0x0100, 0xfe72ab79 ) /* bg char lookup table */
	ROM_LOAD( "exerion.k4",   0x0320, 0x0100, 0xffc2ba43 ) /* bg char mixer */
ROM_END



/*********************************************************************
 * Initialization routines
 *********************************************************************/

static void init_exerion(void)
{
	UINT32 oldaddr, newaddr, length;
	UINT8 *src, *dst, *temp;

	/* allocate some temporary space */
	temp = (UINT8*)malloc(0x8000);
	if (!temp)
		return;

	/* make a temporary copy of the character data */
	src = temp;
	dst = memory_region(REGION_GFX1);
	length = memory_region_length(REGION_GFX1);
	memcpy(src, dst, length);

	/* decode the characters */
	/* the bits in the ROM are ordered: n8-n7 n6 n5 n4-v2 v1 v0 n3-n2 n1 n0 h2 */
	/* we want them ordered like this:  n8-n7 n6 n5 n4-n3 n2 n1 n0-v2 v1 v0 h2 */
	for (oldaddr = 0; oldaddr < length; oldaddr++)
	{
		newaddr = ((oldaddr     ) & 0x1f00) |       /* keep n8-n4 */
		          ((oldaddr << 3) & 0x00f0) |       /* move n3-n0 */
		          ((oldaddr >> 4) & 0x000e) |       /* move v2-v0 */
		          ((oldaddr     ) & 0x0001);        /* keep h2 */
		dst[newaddr] = src[oldaddr];
	}

	/* make a temporary copy of the sprite data */
	src = temp;
	dst = memory_region(REGION_GFX2);
	length = memory_region_length(REGION_GFX2);
	memcpy(src, dst, length);

	/* decode the sprites */
	/* the bits in the ROMs are ordered: n3 n7-n6 n5 n4 v3-v2 v1 v0 n2-n1 n0 h3 h2 */
	/* we want them ordered like this:   n7 n6-n5 n4 n3 n2-n1 n0 v3 v2-v1 v0 h3 h2 */
	for (oldaddr = 0; oldaddr < length; oldaddr++)
	{
		newaddr = ((oldaddr << 1) & 0x3c00) |       /* move n7-n4 */
		          ((oldaddr >> 4) & 0x0200) |       /* move n3 */
		          ((oldaddr << 4) & 0x01c0) |       /* move n2-n0 */
		          ((oldaddr >> 3) & 0x003c) |       /* move v3-v0 */
		          ((oldaddr     ) & 0x0003);        /* keep h3-h2 */
		dst[newaddr] = src[oldaddr];
	}

	free(temp);
}


static void init_exerionb(void)
{
	UINT8 *ram = memory_region(REGION_CPU1);
	int addr;

	/* the program ROMs have data lines D1 and D2 swapped. Decode them. */
	for (addr = 0; addr < 0x6000; addr++)
		ram[addr] = (ram[addr] & 0xf9) | ((ram[addr] & 2) << 1) | ((ram[addr] & 4) >> 1);

	/* also convert the gfx as in Exerion */
	init_exerion();
}



/*********************************************************************
 * Game drivers
 *********************************************************************/

GAMEX( 1983, exerion,  0,       exerion, exerion, exerion,  ROT90, "Jaleco", "Exerion", GAME_IMPERFECT_COLORS )
GAMEX( 1983, exeriont, exerion, exerion, exerion, exerion,  ROT90, "Jaleco (Taito America license)", "Exerion (Taito)", GAME_IMPERFECT_COLORS )
GAMEX( 1983, exerionb, exerion, exerion, exerion, exerionb, ROT90, "Jaleco", "Exerion (bootleg)", GAME_IMPERFECT_COLORS )
