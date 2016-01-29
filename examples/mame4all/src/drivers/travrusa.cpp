#include "../vidhrdw/travrusa.cpp"

/****************************************************************************
Traverse USA Driver

Lee Taylor
John Clegg

Loosely based on the our previous 10 Yard Fight driver.


- I haven't understood how char/sprite priority works. This is used for
  tunnels. I hacked it just by making the two needed colors opaque. They
  don't seem to be used anywhere else.

****************************************************************************/

#include "driver.h"
#include "sndhrdw/irem.h"

extern unsigned char *spriteram;
extern size_t spriteram_size;

extern unsigned char *travrusa_videoram;

void travrusa_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int travrusa_vh_start(void);
WRITE_HANDLER( travrusa_videoram_w );
WRITE_HANDLER( travrusa_scroll_x_low_w );
WRITE_HANDLER( travrusa_scroll_x_high_w );
WRITE_HANDLER( travrusa_flipscreen_w );
void travrusa_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_RAM },        /* Video and Color ram */
	{ 0xd000, 0xd000, input_port_0_r },	/* IN0 */
	{ 0xd001, 0xd001, input_port_1_r },	/* IN1 */
	{ 0xd002, 0xd002, input_port_2_r },	/* IN2 */
	{ 0xd003, 0xd003, input_port_3_r },	/* DSW1 */
	{ 0xd004, 0xd004, input_port_4_r },	/* DSW2 */
	{ 0xe000, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8fff, travrusa_videoram_w, &travrusa_videoram },
	{ 0x9000, 0x9000, travrusa_scroll_x_low_w },
	{ 0xa000, 0xa000, travrusa_scroll_x_high_w },
	{ 0xc800, 0xc9ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xd000, 0xd000, irem_sound_cmd_w },
	{ 0xd001, 0xd001, travrusa_flipscreen_w },	/* + coin counters */
	{ 0xe000, 0xefff, MWA_RAM },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( travrusa )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	/* coin input must be active for 19 frames to be consistently recognized */
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN3, 19 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Fuel Reduced on Collision" )
	PORT_DIPSETTING(    0x03, "Low" )
	PORT_DIPSETTING(    0x02, "Med" )
	PORT_DIPSETTING(    0x01, "Hi" )
	PORT_DIPSETTING(    0x00, "Max" )
	PORT_DIPNAME( 0x04, 0x04, "Fuel Consumption" )
	PORT_DIPSETTING(    0x04, "Low" )
	PORT_DIPSETTING(    0x00, "Hi" )
	PORT_DIPNAME( 0x08, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_7C ) )

	/* PORT_DIPSETTING( 0x10, "INVALID" ) */
	/* PORT_DIPSETTING( 0x00, "INVALID" ) */

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, "Speed Type" )
	PORT_DIPSETTING(    0x08, "M/H" )
	PORT_DIPSETTING(    0x00, "Km/H" )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Title" )
	PORT_DIPSETTING(    0x20, "Traverse USA" )
	PORT_DIPSETTING(    0x00, "Zippy Race" )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

/* same as travrusa, no "Title" switch */
INPUT_PORTS_START( motorace )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	/* coin input must be active for 19 frames to be consistently recognized */
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN3, 19 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Fuel Reduced on Collision" )
	PORT_DIPSETTING(    0x03, "Low" )
	PORT_DIPSETTING(    0x02, "Med" )
	PORT_DIPSETTING(    0x01, "Hi" )
	PORT_DIPSETTING(    0x00, "Max" )
	PORT_DIPNAME( 0x04, 0x04, "Fuel Consumption" )
	PORT_DIPSETTING(    0x04, "Low" )
	PORT_DIPSETTING(    0x00, "Hi" )
	PORT_DIPNAME( 0x08, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x08, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_7C ) )

	/* PORT_DIPSETTING( 0x10, "INVALID" ) */
	/* PORT_DIPSETTING( 0x00, "INVALID" ) */

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Coin Mode" )
	PORT_DIPSETTING(    0x04, "Mode 1" )
	PORT_DIPSETTING(    0x00, "Mode 2" )
	PORT_DIPNAME( 0x08, 0x08, "Speed Type" )
	PORT_DIPSETTING(    0x08, "M/H" )
	PORT_DIPSETTING(    0x00, "Km/H" )
	/* In stop mode, press 2 to stop and 1 to restart */
	PORT_BITX   ( 0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Stop Mode", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	3,	/* 3 bits per pixel */
	{ 2*1024*8*8, 1024*8*8, 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 8*0, 8*1, 8*2, 8*3, 8*4, 8*5, 8*6, 8*7 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	256,	/* 256 sprites */
	3,	/* 3 bits per pixel */
	{ 2*256*16*16, 256*16*16, 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,      0, 16 },
	{ REGION_GFX2, 0, &spritelayout, 16*8, 16 },
	{ -1 } /* end of array */
};



static struct MachineDriver machine_driver_travrusa =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4 Mhz (?) */
			readmem,writemem,0,0,
			interrupt,1
		},
		IREM_AUDIO_CPU
	},
	57, 1790,	/* accurate frequency, measured on a Moon Patrol board, is 56.75Hz. */
				/* the Lode Runner manual (similar but different hardware) */
				/* talks about 55Hz and 1790ms vblank duration. */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo,
	128+32, 16*8+16*8,
	travrusa_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	travrusa_vh_start,
	0,
	travrusa_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		IREM_AUDIO
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/


ROM_START( travrusa )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "zippyrac.000", 0x0000, 0x2000, 0xbe066c0a )
	ROM_LOAD( "zippyrac.005", 0x2000, 0x2000, 0x145d6b34 )
	ROM_LOAD( "zippyrac.006", 0x4000, 0x2000, 0xe1b51383 )
	ROM_LOAD( "zippyrac.007", 0x6000, 0x2000, 0x85cd1a51 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu */
	ROM_LOAD( "mr10.1a",      0xf000, 0x1000, 0xa02ad8a0 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zippyrac.001", 0x00000, 0x2000, 0xaa8994dd )
	ROM_LOAD( "mr8.3c",       0x02000, 0x2000, 0x3a046dd1 )
	ROM_LOAD( "mr9.3a",       0x04000, 0x2000, 0x1cc3d3f4 )

	ROM_REGION( 0x06000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zippyrac.008", 0x00000, 0x2000, 0x3e2c7a6b )
	ROM_LOAD( "zippyrac.009", 0x02000, 0x2000, 0x13be6a14 )
	ROM_LOAD( "zippyrac.010", 0x04000, 0x2000, 0x6fcc9fdb )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "mmi6349.ij",   0x0000, 0x0200, 0xc9724350 ) /* character palette - last $100 are unused */
	ROM_LOAD( "tbp18s.2",     0x0100, 0x0020, 0xa1130007 ) /* sprite palette */
	ROM_LOAD( "tbp24s10.3",   0x0120, 0x0100, 0x76062638 ) /* sprite lookup table */
ROM_END

ROM_START( motorace )
	ROM_REGION( 0x12000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "mr.cpu",       0x10000, 0x2000, 0x89030b0c )	/* we load the ROM at 10000-11fff, */
														/* it will be decrypted at 0000 */
	ROM_LOAD( "mr1.3l",       0x2000, 0x2000, 0x0904ed58 )
	ROM_LOAD( "mr2.3k",       0x4000, 0x2000, 0x8a2374ec )
	ROM_LOAD( "mr3.3j",       0x6000, 0x2000, 0x2f04c341 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu */
	ROM_LOAD( "mr10.1a",      0xf000, 0x1000, 0xa02ad8a0 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mr7.3e",       0x00000, 0x2000, 0x492a60be )
	ROM_LOAD( "mr8.3c",       0x02000, 0x2000, 0x3a046dd1 )
	ROM_LOAD( "mr9.3a",       0x04000, 0x2000, 0x1cc3d3f4 )

	ROM_REGION( 0x06000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mr4.3n",       0x00000, 0x2000, 0x5cf1a0d6 )
	ROM_LOAD( "mr5.3m",       0x02000, 0x2000, 0xf75f2aad )
	ROM_LOAD( "mr6.3k",       0x04000, 0x2000, 0x518889a0 )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "mmi6349.ij",   0x0000, 0x0200, 0xc9724350 ) /* character palette - last $100 are unused */
	ROM_LOAD( "tbp18s.2",     0x0100, 0x0020, 0xa1130007 ) /* sprite palette */
	ROM_LOAD( "tbp24s10.3",   0x0120, 0x0100, 0x76062638 ) /* sprite lookup table */
ROM_END



void init_motorace(void)
{
	int A,i,j;
	unsigned char *RAM = memory_region(REGION_CPU1);


	/* The first CPU ROM has the address and data lines scrambled */
	for (A = 0;A < 0x2000;A++)
	{
		int bit[13];


		for (i = 0;i < 13;i++)
			bit[i] = (A >> i) & 1;

		j =
			(bit[11] <<  0) +
			(bit[ 0] <<  1) +
			(bit[ 2] <<  2) +
			(bit[ 4] <<  3) +
			(bit[ 6] <<  4) +
			(bit[ 8] <<  5) +
			(bit[10] <<  6) +
			(bit[12] <<  7) +
			(bit[ 1] <<  8) +
			(bit[ 3] <<  9) +
			(bit[ 5] << 10) +
			(bit[ 7] << 11) +
			(bit[ 9] << 12);

		for (i = 0;i < 8;i++)
			bit[i] = (RAM[A + 0x10000] >> i) & 1;

		RAM[j] =
			(bit[5] << 0) +
			(bit[0] << 1) +
			(bit[3] << 2) +
			(bit[6] << 3) +
			(bit[1] << 4) +
			(bit[4] << 5) +
			(bit[7] << 6) +
			(bit[2] << 7);
	}
}



GAME( 1983, travrusa, 0,        travrusa, travrusa, 0,        ROT270, "Irem", "Traverse USA / Zippy Race" )
GAME( 1983, motorace, travrusa, travrusa, motorace, motorace, ROT270, "Irem (Williams license)", "MotoRace USA" )
