#include "../machine/turbo.cpp"
#include "../vidhrdw/turbo.cpp"

/*************************************************************************

	Turbo - Sega - 1981

driver by Alex Pasadyn, Howie Cohen, Frank Palazzolo, Aaron Giles, Ernesto Corvi


	Memory Map:  ( * not complete * )

	Address Range:	R/W:	 Function:
	--------------------------------------------------------------------------
	0000 - 5fff		R		 Program ROM
	a000 - a0ff		W		 Sprite RAM
	a800 - a803		W		 Lamps / Coin Meters
	b000 - b1ff		R/W		 Collision RAM
	e000 - e7ff		R/W		 character RAM
	f000 - f7ff		R/W		 RAM
	f202					 coinage 2
	f205					 coinage 1
	f800 - f803		R/W		 road drawing
	f900 - f903		R/W		 road drawing
	fa00 - fa03		R/W		 sound
	fb00 - fb03		R/W		 x,DS2,x,x
	fc00 - fc01		R		 DS1,x
	fc00 - fc01		W		 score
	fd00			R		 Coin Inputs, etc.
	fe00			R		 DS3,x

 Switch settings:
 Notes:
		1) Facing the CPU board, with the two large IDC connectors at
		   the top of the board, and the large and small IDC
		   connectors at the bottom, DIP switch #1 is upper right DIP
		   switch, DIP switch #2 is the DIP switch to the right of it.

		2) Facing the Sound board, with the IDC connector at the
		   bottom of the board, DIP switch #3 (4 bank) can be seen.
 ----------------------------------------------------------------------------

 Option	   (DIP Swtich #1) | SW1 | SW2 | SW3 | SW4 | SW5 | SW6 | SW7 | SW8 |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 1 Car On Extended Play	   | ON	 | ON  |	 |	   |	 |	   |	 |	   |
 2 Car On Extended Play	   | OFF | ON  |	 |	   |	 |	   |	 |	   |
 3 Car On Extended Play	   | ON	 | OFF |	 |	   |	 |	   |	 |	   |
 4 Car On Extended Play	   | OFF | OFF |	 |	   |	 |	   |	 |	   |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Game Time Adjustable	   |	 |	   | ON	 |	   |	 |	   |	 |	   |
 Game Time Fixed (55 Sec.) |	 |	   | OFF |	   |	 |	   |	 |	   |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Hard Game Difficulty	   |	 |	   |	 | ON  |	 |	   |	 |	   |
 Easy Game Difficulty	   |	 |	   |	 | OFF |	 |	   |	 |	   |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Normal Game Mode		   |	 |	   |	 |	   | ON	 |	   |	 |	   |
 No Collisions (cheat)	   |	 |	   |	 |	   | OFF |	   |	 |	   |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Initial Entry Off (?)	   |	 |	   |	 |	   |	 | ON  |	 |	   |
 Initial Entry On  (?)	   |	 |	   |	 |	   |	 | OFF |	 |	   |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Not Used				   |	 |	   |	 |	   |	 |	   |  X	 |	X  |
 ---------------------------------------------------------------------------

 Option	   (DIP Swtich #2) | SW1 | SW2 | SW3 | SW4 | SW5 | SW6 | SW7 | SW8 |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 60 Seconds Game Time	   | ON	 | ON  |	 |	   |	 |	   |	 |	   |
 70 Seconds Game Time	   | OFF | ON  |	 |	   |	 |	   |	 |	   |
 80 Seconds Game Time	   | ON	 | OFF |	 |	   |	 |	   |	 |	   |
 90 Seconds Game Time	   | OFF | OFF |	 |	   |	 |	   |	 |	   |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Slot 1	 1 Coin	 1 Credit  |	 |	   | ON	 | ON  | ON	 |	   |	 |	   |
 Slot 1	 1 Coin	 2 Credits |	 |	   | OFF | ON  | ON	 |	   |	 |	   |
 Slot 1	 1 Coin	 3 Credits |	 |	   | ON	 | OFF | ON	 |	   |	 |	   |
 Slot 1	 1 Coin	 6 Credits |	 |	   | OFF | OFF | ON	 |	   |	 |	   |
 Slot 1	 2 Coins 1 Credit  |	 |	   | ON	 | ON  | OFF |	   |	 |	   |
 Slot 1	 3 Coins 1 Credit  |	 |	   | OFF | ON  | OFF |	   |	 |	   |
 Slot 1	 4 Coins 1 Credit  |	 |	   | ON	 | OFF | OFF |	   |	 |	   |
 Slot 1	 1 Coin	 1 Credit  |	 |	   | OFF | OFF | OFF |	   |	 |	   |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Slot 2	 1 Coin	 1 Credit  |	 |	   |	 |	   |	 | ON  | ON	 | ON  |
 Slot 2	 1 Coin	 2 Credits |	 |	   |	 |	   |	 | OFF | ON	 | ON  |
 Slot 2	 1 Coin	 3 Credits |	 |	   |	 |	   |	 | ON  | OFF | ON  |
 Slot 2	 1 Coin	 6 Credits |	 |	   |	 |	   |	 | OFF | OFF | ON  |
 Slot 2	 2 Coins 1 Credit  |	 |	   |	 |	   |	 | ON  | ON	 | OFF |
 Slot 2	 3 Coins 1 Credit  |	 |	   |	 |	   |	 | OFF | ON	 | OFF |
 Slot 2	 4 Coins 1 Credit  |	 |	   |	 |	   |	 | ON  | OFF | OFF |
 Slot 2	 1 Coins 1 Credit  |	 |	   |	 |	   |	 | OFF | OFF | OFF |
 ---------------------------------------------------------------------------

 Option	   (DIP Swtich #3) | SW1 | SW2 | SW3 | SW4 |
 --------------------------|-----|-----|-----|-----|
 Not Used				   |  X	 |	X  |	 |	   |
 --------------------------|-----|-----|-----|-----|
 Digital (LED) Tachometer  |	 |	   | ON	 |	   |
 Analog (Meter) Tachometer |	 |	   | OFF |	   |
 --------------------------|-----|-----|-----|-----|
 Cockpit Sound System	   |	 |	   |	 | ON  |
 Upright Sound System	   |	 |	   |	 | OFF |
---------------------------------------------------

Here is a complete list of the ROMs:

	Turbo ROMLIST - Frank Palazzolo
	Name	 Loc			 Function
	-----------------------------------------------------------------------------
	Images Acquired:
	EPR1262,3,4	 IC76, IC89, IC103
	EPR1363,4,5
	EPR15xx				Program ROMS
	EPR1244				Character Data 1
	EPR1245				Character Data 2
	EPR-1125			Road ROMS
	EPR-1126
	EPR-1127
	EPR-1238
	EPR-1239
	EPR-1240
	EPR-1241
	EPR-1242
	EPR-1243
	EPR1246-1258		Sprite ROMS
	EPR1288-1300

	PR-1114		IC13	Color 1 (road, etc.)
	PR-1115		IC18	Road gfx
	PR-1116		IC20	Crash (collision detection?)
	PR-1117		IC21	Color 2 (road, etc.)
	PR-1118		IC99	256x4 Character Color PROM
	PR-1119		IC50	512x8 Vertical Timing PROM
	PR-1120		IC62	Horizontal Timing PROM
	PR-1121		IC29	Color PROM
	PR-1122		IC11	Pattern 1
	PR-1123		IC21	Pattern 2

	PA-06R		IC22	Mathbox Timing PAL
	PA-06L		IC90	Address Decode PAL

**************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/8255ppi.h"

/* from machine */
void turbo_init_machine(void);
READ_HANDLER( turbo_8279_r );
READ_HANDLER( turbo_collision_r );
WRITE_HANDLER( turbo_8279_w );
WRITE_HANDLER( turbo_coin_and_lamp_w );

/* from vidhrdw */
extern UINT8 *turbo_sprite_position;

int turbo_vh_start(void);
void turbo_vh_stop(void);
void turbo_vh_convert_color_prom(UINT8 *palette, UINT16 *colortable, const UINT8 *color_prom);
void turbo_vh_eof(void);
void turbo_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
WRITE_HANDLER( turbo_collision_clear_w );


/*********************************************************************
 * CPU memory structures
 *********************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0xb000, 0xb1ff, MRA_RAM },
	{ 0xe000, 0xe7ff, MRA_RAM },
	{ 0xf000, 0xf7ff, MRA_RAM },
	{ 0xf800, 0xf803, ppi8255_0_r },
	{ 0xf900, 0xf903, ppi8255_1_r },
	{ 0xfa00, 0xfa03, ppi8255_2_r },
	{ 0xfb00, 0xfb03, ppi8255_3_r },
	{ 0xfc00, 0xfcff, turbo_8279_r },
	{ 0xfd00, 0xfdff, input_port_0_r },
	{ 0xfe00, 0xfeff, turbo_collision_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0xa000, 0xa0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xa800, 0xa807, turbo_coin_and_lamp_w },
	{ 0xb000, 0xb1ff, MWA_RAM, &turbo_sprite_position },
	{ 0xb800, 0xb800, MWA_NOP },	/* resets the analog wheel value */
	{ 0xe000, 0xe7ff, MWA_RAM, &videoram, &videoram_size },
	{ 0xe800, 0xe800, turbo_collision_clear_w },
	{ 0xf000, 0xf7ff, MWA_RAM },
	{ 0xf800, 0xf803, ppi8255_0_w },
	{ 0xf900, 0xf903, ppi8255_1_w },
	{ 0xfa00, 0xfa03, ppi8255_2_w },
	{ 0xfb00, 0xfb03, ppi8255_3_w },
	{ 0xfc00, 0xfcff, turbo_8279_w },
	{ -1 }	/* end of table */
};


/*********************************************************************
 * Input port definitions
 *********************************************************************/

INPUT_PORTS_START( turbo )
	PORT_START		/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )				/* ACCEL B */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )				/* ACCEL A */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_TOGGLE )	/* SHIFT */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN4 )					/* SERVICE */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* DSW 1 */
	PORT_DIPNAME( 0x03, 0x03, "Car On Extended Play" )
	PORT_DIPSETTING( 0x03, "1" )
	PORT_DIPSETTING( 0x02, "2" )
	PORT_DIPSETTING( 0x01, "3" )
	PORT_DIPSETTING( 0x00, "4" )
	PORT_DIPNAME( 0x04, 0x04, "Game Time" )
	PORT_DIPSETTING( 0x00, "Fixed (55 sec)")
	PORT_DIPSETTING( 0x04, "Adjustable" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING( 0x00, "Easy")
	PORT_DIPSETTING( 0x08, "Hard")
	PORT_DIPNAME( 0x10, 0x00, "Game Mode" )
	PORT_DIPSETTING( 0x10, "No Collisions (cheat)")
	PORT_DIPSETTING( 0x00, "Normal")
	PORT_DIPNAME( 0x20, 0x00, "Initial Entry" )
	PORT_DIPSETTING( 0x20, DEF_STR( Off ))
	PORT_DIPSETTING( 0x00, DEF_STR( On ))
	PORT_BIT( 0xc0, 0xc0, IPT_UNUSED )

	PORT_START	/* DSW 2 */
	PORT_DIPNAME( 0x03, 0x03, "Game Time" )
	PORT_DIPSETTING( 0x00, "60 seconds" )
	PORT_DIPSETTING( 0x01, "70 seconds" )
	PORT_DIPSETTING( 0x02, "80 seconds" )
	PORT_DIPSETTING( 0x03, "90 seconds" )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Coin_B ))
	PORT_DIPSETTING(	0x18, DEF_STR( 4C_1C ))
	PORT_DIPSETTING(	0x14, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(	0x10, DEF_STR( 2C_1C ))
/*	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ))*/
	PORT_DIPSETTING(	0x1c, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(	0x08, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(	0x0c, DEF_STR( 1C_6C ))
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coin_A ))
	PORT_DIPSETTING(	0xc0, DEF_STR( 4C_1C ))
	PORT_DIPSETTING(	0xa0, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(	0x80, DEF_STR( 2C_1C ))
/*	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ))*/
	PORT_DIPSETTING(	0xe0, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(	0x20, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(	0x40, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(	0x60, DEF_STR( 1C_6C ))

	PORT_START	/* DSW 3 */
	PORT_BIT( 0x0f, 0x00, IPT_UNUSED )		/* Merged with collision bits */
	PORT_BIT( 0x30, 0x00, IPT_UNUSED )
	PORT_DIPNAME( 0x40, 0x40, "Tachometer" )
	PORT_DIPSETTING(	0x40, "Analog (Meter)")
	PORT_DIPSETTING(	0x00, "Digital (led)")
	PORT_DIPNAME( 0x80, 0x80, "Sound System" )
	PORT_DIPSETTING(	0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x00, "Cockpit")

	PORT_START		/* IN0 */
	PORT_ANALOG( 0xff, 0, IPT_DIAL | IPF_CENTER, 10, 30, 0, 0 )
INPUT_PORTS_END


/*********************************************************************
 * Graphics layouts
 *********************************************************************/

static struct GfxLayout numlayout =
{
	10,8,	/* 10*8 characters */
	16,		/* 16 characters */
	1,		/* 1 bit per pixel */
	{ 0 },	/* bitplane offsets */
	{ 9*8, 8*8, 7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	10*8	/* every character uses 10 consecutive bytes */
};

static struct GfxLayout tachlayout =
{
	16,1,	/* 16*1 characters */
	2,		/* 2 characters */
	1,		/* 1 bit per pixel */
	{ 0 },	/* bitplane offsets */
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0 },
	1*8		/* every character uses 1 consecutive byte */
};

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,		/* 2 bits per pixel */
	{ 256*8*8, 0 },	/* bitplane offsets */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8		/* every character uses 8 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX4, 0x0000, &numlayout,	512,   1 },
	{ REGION_GFX4, 0x0100, &tachlayout,	512,   3 },
	{ REGION_GFX3, 0x0000, &charlayout,	512,   3 },
	{ -1 }
};


/*********************************************************************
 * Sound interfaces
 *********************************************************************/

static const char *sample_names[]=
{
	"*turbo",
	"01.wav",		/* Trig1 */
	"02.wav",		/* Trig2 */
	"03.wav",		/* Trig3 */
	"04.wav",		/* Trig4 */
	"05.wav",		/* Screech */
	"06.wav",		/* Crash */
	"skidding.wav",	/* Spin */
	"idle.wav",		/* Idle */
	"ambulanc.wav",	/* Ambulance */
	0
};

static struct Samplesinterface samples_interface =
{
	8,			/* eight channels */
	25,			/* volume */
	sample_names
};


/*********************************************************************
 * Machine driver
 *********************************************************************/

static struct MachineDriver machine_driver_turbo =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			5000000,
			readmem,writemem,0,0,
			interrupt,1
		}
	},
	30, DEFAULT_REAL_30HZ_VBLANK_DURATION,
	1,
	turbo_init_machine,

	/* video hardware */
	32*8, 35*8, { 1*8, 32*8-1, 0*8, 35*8-1 },
	gfxdecodeinfo,
	512+6,512+6,
	turbo_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	turbo_vh_eof,
	turbo_vh_start,
	turbo_vh_stop,
	turbo_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SAMPLES,
			&samples_interface
		}
	}
};


/*********************************************************************
 * ROM definitions
 *********************************************************************/

ROM_START( turbo )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "epr1513.bin",  0x0000, 0x2000, 0x0326adfc )
	ROM_LOAD( "epr1514.bin",  0x2000, 0x2000, 0x25af63b0 )
	ROM_LOAD( "epr1515.bin",  0x4000, 0x2000, 0x059c1c36 )

	ROM_REGION( 0x20000, REGION_GFX1 ) /* sprite data */
	ROM_LOAD( "epr1246.rom", 0x00000, 0x2000, 0x555bfe9a )
	ROM_RELOAD(				 0x02000, 0x2000 )
	ROM_LOAD( "epr1247.rom", 0x04000, 0x2000, 0xc8c5e4d5 )
	ROM_RELOAD(				 0x06000, 0x2000 )
	ROM_LOAD( "epr1248.rom", 0x08000, 0x2000, 0x82fe5b94 )
	ROM_RELOAD(				 0x0a000, 0x2000 )
	ROM_LOAD( "epr1249.rom", 0x0c000, 0x2000, 0xe258e009 )
	ROM_LOAD( "epr1250.rom", 0x0e000, 0x2000, 0xaee6e05e )
	ROM_LOAD( "epr1251.rom", 0x10000, 0x2000, 0x292573de )
	ROM_LOAD( "epr1252.rom", 0x12000, 0x2000, 0xaee6e05e )
	ROM_LOAD( "epr1253.rom", 0x14000, 0x2000, 0x92783626 )
	ROM_LOAD( "epr1254.rom", 0x16000, 0x2000, 0xaee6e05e )
	ROM_LOAD( "epr1255.rom", 0x18000, 0x2000, 0x485dcef9 )
	ROM_LOAD( "epr1256.rom", 0x1a000, 0x2000, 0xaee6e05e )
	ROM_LOAD( "epr1257.rom", 0x1c000, 0x2000, 0x4ca984ce )
	ROM_LOAD( "epr1258.rom", 0x1e000, 0x2000, 0xaee6e05e )

	ROM_REGION( 0x4800, REGION_GFX2 ) /* road data */
	ROM_LOAD( "epr1125.rom", 0x0000, 0x0800, 0x65b5d44b )
	ROM_LOAD( "epr1126.rom", 0x0800, 0x0800, 0x685ace1b )
	ROM_LOAD( "epr1127.rom", 0x1000, 0x0800, 0x9233c9ca )
	ROM_LOAD( "epr1238.rom", 0x1800, 0x0800, 0xd94fd83f )
	ROM_LOAD( "epr1239.rom", 0x2000, 0x0800, 0x4c41124f )
	ROM_LOAD( "epr1240.rom", 0x2800, 0x0800, 0x371d6282 )
	ROM_LOAD( "epr1241.rom", 0x3000, 0x0800, 0x1109358a )
	ROM_LOAD( "epr1242.rom", 0x3800, 0x0800, 0x04866769 )
	ROM_LOAD( "epr1243.rom", 0x4000, 0x0800, 0x29854c48 )

	ROM_REGION( 0x1000, REGION_GFX3 )	/* background data */
	ROM_LOAD( "epr1244.rom", 0x0000, 0x0800, 0x17f67424 )
	ROM_LOAD( "epr1245.rom", 0x0800, 0x0800, 0x2ba0b46b )

	ROM_REGION( 0x200, REGION_GFX4 )	/* number data (copied at init time) */


	ROM_REGION( 0x1000, REGION_PROMS ) /* various PROMs */
	ROM_LOAD( "pr1121.bin",	 0x0000, 0x0200, 0x7692f497 )	/* palette */
	ROM_LOAD( "pr1122.bin",	 0x0200, 0x0400, 0x1a86ce70 )	/* sprite priorities */
	ROM_LOAD( "pr1123.bin",	 0x0600, 0x0400, 0x02d2cb52 )	/* sprite/road/background priorities */
	ROM_LOAD( "pr-1118.bin", 0x0a00, 0x0100, 0x07324cfd )	/* background color table */
	ROM_LOAD( "pr1114.bin",	 0x0b00, 0x0020, 0x78aded46 )	/* road red/green color table */
	ROM_LOAD( "pr1117.bin",	 0x0b20, 0x0020, 0xf06d9907 )	/* road green/blue color table */
	ROM_LOAD( "pr1115.bin",	 0x0b40, 0x0020, 0x5394092c )	/* road collision/enable */
	ROM_LOAD( "pr1116.bin",	 0x0b60, 0x0020, 0x3956767d )	/* collision detection */
	ROM_LOAD( "sndprom.bin", 0x0b80, 0x0020, 0xb369a6ae )
	ROM_LOAD( "pr-1119.bin", 0x0c00, 0x0200, 0x628d3f1d )	/* timing - not used */
	ROM_LOAD( "pr-1120.bin", 0x0e00, 0x0200, 0x591b6a68 )	/* timing - not used */
ROM_END

ROM_START( turboa )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "epr1262.rom",  0x0000, 0x2000, 0x1951b83a )
	ROM_LOAD( "epr1263.rom",  0x2000, 0x2000, 0x45e01608 )
	ROM_LOAD( "epr1264.rom",  0x4000, 0x2000, 0x1802f6c7 )

	ROM_REGION( 0x20000, REGION_GFX1 ) /* sprite data */
	ROM_LOAD( "epr1246.rom", 0x00000, 0x2000, 0x555bfe9a )
	ROM_RELOAD(				 0x02000, 0x2000 )
	ROM_LOAD( "epr1247.rom", 0x04000, 0x2000, 0xc8c5e4d5 )
	ROM_RELOAD(				 0x06000, 0x2000 )
	ROM_LOAD( "epr1248.rom", 0x08000, 0x2000, 0x82fe5b94 )
	ROM_RELOAD(				 0x0a000, 0x2000 )
	ROM_LOAD( "epr1249.rom", 0x0c000, 0x2000, 0xe258e009 )
	ROM_LOAD( "epr1250.rom", 0x0e000, 0x2000, 0xaee6e05e )
	ROM_LOAD( "epr1251.rom", 0x10000, 0x2000, 0x292573de )
	ROM_LOAD( "epr1252.rom", 0x12000, 0x2000, 0xaee6e05e )
	ROM_LOAD( "epr1253.rom", 0x14000, 0x2000, 0x92783626 )
	ROM_LOAD( "epr1254.rom", 0x16000, 0x2000, 0xaee6e05e )
	ROM_LOAD( "epr1255.rom", 0x18000, 0x2000, 0x485dcef9 )
	ROM_LOAD( "epr1256.rom", 0x1a000, 0x2000, 0xaee6e05e )
	ROM_LOAD( "epr1257.rom", 0x1c000, 0x2000, 0x4ca984ce )
	ROM_LOAD( "epr1258.rom", 0x1e000, 0x2000, 0xaee6e05e )

	ROM_REGION( 0x4800, REGION_GFX2 ) /* road data */
	ROM_LOAD( "epr1125.rom", 0x0000, 0x0800, 0x65b5d44b )
	ROM_LOAD( "epr1126.rom", 0x0800, 0x0800, 0x685ace1b )
	ROM_LOAD( "epr1127.rom", 0x1000, 0x0800, 0x9233c9ca )
	ROM_LOAD( "epr1238.rom", 0x1800, 0x0800, 0xd94fd83f )
	ROM_LOAD( "epr1239.rom", 0x2000, 0x0800, 0x4c41124f )
	ROM_LOAD( "epr1240.rom", 0x2800, 0x0800, 0x371d6282 )
	ROM_LOAD( "epr1241.rom", 0x3000, 0x0800, 0x1109358a )
	ROM_LOAD( "epr1242.rom", 0x3800, 0x0800, 0x04866769 )
	ROM_LOAD( "epr1243.rom", 0x4000, 0x0800, 0x29854c48 )

	ROM_REGION( 0x1000, REGION_GFX3 )	/* background data */
	ROM_LOAD( "epr1244.rom", 0x0000, 0x0800, 0x17f67424 )
	ROM_LOAD( "epr1245.rom", 0x0800, 0x0800, 0x2ba0b46b )

	ROM_REGION( 0x200, REGION_GFX4 )	/* number data (copied at init time) */


	ROM_REGION( 0x1000, REGION_PROMS ) /* various PROMs */
	ROM_LOAD( "pr1121.bin",	 0x0000, 0x0200, 0x7692f497 )	/* palette */
	ROM_LOAD( "pr1122.bin",	 0x0200, 0x0400, 0x1a86ce70 )	/* sprite priorities */
	ROM_LOAD( "pr1123.bin",	 0x0600, 0x0400, 0x02d2cb52 )	/* sprite/road/background priorities */
	ROM_LOAD( "pr-1118.bin", 0x0a00, 0x0100, 0x07324cfd )	/* background color table */
	ROM_LOAD( "pr1114.bin",	 0x0b00, 0x0020, 0x78aded46 )	/* road red/green color table */
	ROM_LOAD( "pr1117.bin",	 0x0b20, 0x0020, 0xf06d9907 )	/* road green/blue color table */
	ROM_LOAD( "pr1115.bin",	 0x0b40, 0x0020, 0x5394092c )	/* road collision/enable */
	ROM_LOAD( "pr1116.bin",	 0x0b60, 0x0020, 0x3956767d )	/* collision detection */
	ROM_LOAD( "sndprom.bin", 0x0b80, 0x0020, 0xb369a6ae )
	ROM_LOAD( "pr-1119.bin", 0x0c00, 0x0200, 0x628d3f1d )	/* timing - not used */
	ROM_LOAD( "pr-1120.bin", 0x0e00, 0x0200, 0x591b6a68 )	/* timing - not used */
ROM_END

ROM_START( turbob )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "epr-1363.cpu",  0x0000, 0x2000, 0x5c110fb6 )
	ROM_LOAD( "epr-1364.cpu",  0x2000, 0x2000, 0x6a341693 )
	ROM_LOAD( "epr-1365.cpu",  0x4000, 0x2000, 0x3b6b0dc8 )

	ROM_REGION( 0x20000, REGION_GFX1 ) /* sprite data */
	ROM_LOAD( "epr1246.rom", 0x00000, 0x2000, 0x555bfe9a )
	ROM_RELOAD(				 0x02000, 0x2000 )
	ROM_LOAD( "mpr1290.rom", 0x04000, 0x2000, 0x95182020 )	/* is this good? */
	ROM_RELOAD(				 0x06000, 0x2000 )
	ROM_LOAD( "epr1248.rom", 0x08000, 0x2000, 0x82fe5b94 )
	ROM_RELOAD(				 0x0a000, 0x2000 )
	ROM_LOAD( "mpr1291.rom", 0x0c000, 0x2000, 0x0e857f82 )	/* is this good? */
	ROM_LOAD( "epr1250.rom", 0x0e000, 0x2000, 0xaee6e05e )
	ROM_LOAD( "epr1251.rom", 0x10000, 0x2000, 0x292573de )
	ROM_LOAD( "epr1252.rom", 0x12000, 0x2000, 0xaee6e05e )
	ROM_LOAD( "epr1253.rom", 0x14000, 0x2000, 0x92783626 )
	ROM_LOAD( "epr1254.rom", 0x16000, 0x2000, 0xaee6e05e )
	ROM_LOAD( "epr1255.rom", 0x18000, 0x2000, 0x485dcef9 )
	ROM_LOAD( "epr1256.rom", 0x1a000, 0x2000, 0xaee6e05e )
	ROM_LOAD( "epr1257.rom", 0x1c000, 0x2000, 0x4ca984ce )
	ROM_LOAD( "epr1258.rom", 0x1e000, 0x2000, 0xaee6e05e )

	ROM_REGION( 0x4800, REGION_GFX2 ) /* road data */
	ROM_LOAD( "epr1125.rom", 0x0000, 0x0800, 0x65b5d44b )
	ROM_LOAD( "epr1126.rom", 0x0800, 0x0800, 0x685ace1b )
	ROM_LOAD( "epr1127.rom", 0x1000, 0x0800, 0x9233c9ca )
	ROM_LOAD( "epr1238.rom", 0x1800, 0x0800, 0xd94fd83f )
	ROM_LOAD( "epr1239.rom", 0x2000, 0x0800, 0x4c41124f )
	ROM_LOAD( "epr1240.rom", 0x2800, 0x0800, 0x371d6282 )
	ROM_LOAD( "epr1241.rom", 0x3000, 0x0800, 0x1109358a )
	ROM_LOAD( "epr1242.rom", 0x3800, 0x0800, 0x04866769 )
	ROM_LOAD( "epr1243.rom", 0x4000, 0x0800, 0x29854c48 )

	ROM_REGION( 0x1000, REGION_GFX3 )	/* background data */
	ROM_LOAD( "epr1244.rom", 0x0000, 0x0800, 0x17f67424 )
	ROM_LOAD( "epr1245.rom", 0x0800, 0x0800, 0x2ba0b46b )

	ROM_REGION( 0x200, REGION_GFX4 )	/* number data (copied at init time) */


	ROM_REGION( 0x1000, REGION_PROMS ) /* various PROMs */
	ROM_LOAD( "pr1121.bin",	 0x0000, 0x0200, 0x7692f497 )	/* palette */
	ROM_LOAD( "pr1122.bin",	 0x0200, 0x0400, 0x1a86ce70 )	/* sprite priorities */
	ROM_LOAD( "pr1123.bin",	 0x0600, 0x0400, 0x02d2cb52 )	/* sprite/road/background priorities */
	ROM_LOAD( "pr-1118.bin", 0x0a00, 0x0100, 0x07324cfd )	/* background color table */
	ROM_LOAD( "pr1114.bin",	 0x0b00, 0x0020, 0x78aded46 )	/* road red/green color table */
	ROM_LOAD( "pr1117.bin",	 0x0b20, 0x0020, 0xf06d9907 )	/* road green/blue color table */
	ROM_LOAD( "pr1115.bin",	 0x0b40, 0x0020, 0x5394092c )	/* road collision/enable */
	ROM_LOAD( "pr1116.bin",	 0x0b60, 0x0020, 0x3956767d )	/* collision detection */
	ROM_LOAD( "sndprom.bin", 0x0b80, 0x0020, 0xb369a6ae )
	ROM_LOAD( "pr-1119.bin", 0x0c00, 0x0200, 0x628d3f1d )	/* timing - not used */
	ROM_LOAD( "pr-1120.bin", 0x0e00, 0x0200, 0x591b6a68 )	/* timing - not used */
ROM_END


/*********************************************************************
 * ROM decoding
 *********************************************************************/

static void rom_decode(void)
{
/*
 * The table is arranged this way (second half is mirror image of first)
 *
 *		0  1  2	 3	4  5  6	 7	8  9  A	 B	C  D  E	 F
 *
 * 0   00 00 00 00 01 01 01 01 02 02 02 02 03 03 03 03
 * 1   04 04 04 04 05 05 05 05 06 06 06 06 07 07 07 07
 * 2   08 08 08 08 09 09 09 09 0A 0A 0A 0A 0B 0B 0B 0B
 * 3   0C 0C 0C 0C 0D 0D 0D 0D 0E 0E 0E 0E 0F 0F 0F 0F
 * 4   10 10 10 10 11 11 11 11 12 12 12 12 13 13 13 13
 * 5   14 14 14 14 15 15 15 15 16 16 16 16 17 17 17 17
 * 6   18 18 18 18 19 19 19 19 1A 1A 1A 1A 1B 1B 1B 1B
 * 7   1C 1C 1C 1C 1D 1D 1D 1D 1E 1E 1E 1E 1F 1F 1F 1F
 * 8   1F 1F 1F 1F 1E 1E 1E 1E 1D 1D 1D 1D 1C 1C 1C 1C
 * 9   1B 1B 1B 1B 1A 1A 1A 1A 19 19 19 19 18 18 18 18
 * A   17 17 17 17 16 16 16 16 15 15 15 15 14 14 14 14
 * B   13 13 13 13 12 12 12 12 11 11 11 11 10 10 10 10
 * C   0F 0F 0F 0F 0E 0E 0E 0E 0D 0D 0D 0D 0C 0C 0C 0C
 * D   0B 0B 0B 0B 0A 0A 0A 0A 09 09 09 09 08 08 08 08
 * E   07 07 07 07 06 06 06 06 05 05 05 05 04 04 04 04
 * F   03 03 03 03 02 02 02 02 01 01 01 01 00 00 00 00
 *
 */

	static const UINT8 xortable[4][32]=
	{
		/* Table 0 */
		/* 0x0000-0x3ff */
		/* 0x0800-0xbff */
		/* 0x4000-0x43ff */
		/* 0x4800-0x4bff */
		{ 0x00,0x44,0x0c,0x48,0x00,0x44,0x0c,0x48,
		  0xa0,0xe4,0xac,0xe8,0xa0,0xe4,0xac,0xe8,
		  0x60,0x24,0x6c,0x28,0x60,0x24,0x6c,0x28,
		  0xc0,0x84,0xcc,0x88,0xc0,0x84,0xcc,0x88 },

		/* Table 1 */
		/* 0x0400-0x07ff */
		/* 0x0c00-0x0fff */
		/* 0x1400-0x17ff */
		/* 0x1c00-0x1fff */
		/* 0x2400-0x27ff */
		/* 0x2c00-0x2fff */
		/* 0x3400-0x37ff */
		/* 0x3c00-0x3fff */
		/* 0x4400-0x47ff */
		/* 0x4c00-0x4fff */
		/* 0x5400-0x57ff */
		/* 0x5c00-0x5fff */
		{ 0x00,0x44,0x18,0x5c,0x14,0x50,0x0c,0x48,
		  0x28,0x6c,0x30,0x74,0x3c,0x78,0x24,0x60,
		  0x60,0x24,0x78,0x3c,0x74,0x30,0x6c,0x28,
		  0x48,0x0c,0x50,0x14,0x5c,0x18,0x44,0x00 }, //0x00 --> 0x10 ?

		/* Table 2 */
		/* 0x1000-0x13ff */
		/* 0x1800-0x1bff */
		/* 0x5000-0x53ff */
		/* 0x5800-0x5bff */
		{ 0x00,0x00,0x28,0x28,0x90,0x90,0xb8,0xb8,
		  0x28,0x28,0x00,0x00,0xb8,0xb8,0x90,0x90,
		  0x00,0x00,0x28,0x28,0x90,0x90,0xb8,0xb8,
		  0x28,0x28,0x00,0x00,0xb8,0xb8,0x90,0x90 },

		/* Table 3 */
		/* 0x2000-0x23ff */
		/* 0x2800-0x2bff */
		/* 0x3000-0x33ff */
		/* 0x3800-0x3bff */
		{ 0x00,0x14,0x88,0x9c,0x30,0x24,0xb8,0xac,
		  0x24,0x30,0xac,0xb8,0x14,0x00,0x9c,0x88,
		  0x48,0x5c,0xc0,0xd4,0x78,0x6c,0xf0,0xe4,
		  0x6c,0x78,0xe4,0xf0,0x5c,0x48,0xd4,0xc0 }
	};

	int findtable[]=
	{
		0,1,0,1, /* 0x0000-0x0fff */
		2,1,2,1, /* 0x1000-0x1fff */
		3,1,3,1, /* 0x2000-0x2fff */
		3,1,3,1, /* 0x3000-0x3fff */
		0,1,0,1, /* 0x4000-0x4fff */
		2,1,2,1	 /* 0x5000-0x5fff */
	};

	UINT8 *RAM = memory_region(REGION_CPU1);
	int offs, i, j;
	UINT8 src;

	for (offs = 0x0000; offs < 0x6000; offs++)
	{
		src = RAM[offs];
		i = findtable[offs >> 10];
		j = src >> 2;
		if (src & 0x80) j ^= 0x3f;
		RAM[offs] = src ^ xortable[i][j];
	}
}


/*********************************************************************
 * Driver init
 *********************************************************************/

static void init_turbo(void)
{
	static const UINT8 led_number_data[] =
	{
		0x3e,0x41,0x41,0x41,0x00,0x41,0x41,0x41,0x3e,0x00,
		0x00,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x00,0x00,
		0x3e,0x01,0x01,0x01,0x3e,0x40,0x40,0x40,0x3e,0x00,
		0x3e,0x01,0x01,0x01,0x3e,0x01,0x01,0x01,0x3e,0x00,
		0x00,0x41,0x41,0x41,0x3e,0x01,0x01,0x01,0x00,0x00,
		0x3e,0x40,0x40,0x40,0x3e,0x01,0x01,0x01,0x3e,0x00,
		0x3e,0x40,0x40,0x40,0x3e,0x41,0x41,0x41,0x3e,0x00,
		0x3e,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x00,0x00,
		0x3e,0x41,0x41,0x41,0x3e,0x41,0x41,0x41,0x3e,0x00,
		0x3e,0x41,0x41,0x41,0x3e,0x01,0x01,0x01,0x3e,0x00
	};

	static const UINT8 led_tach_data[] =
	{
		0xff,0x00
	};

	memset(memory_region(REGION_GFX4), 0, memory_region_length(REGION_GFX4));
	memcpy(memory_region(REGION_GFX4), led_number_data, sizeof(led_number_data));
	memcpy(memory_region(REGION_GFX4)+0x100, led_tach_data, sizeof(led_tach_data));
}

static void init_decode_turbo(void)
{
	init_turbo();
	rom_decode();
}


/*********************************************************************
 * Game drivers
 *********************************************************************/

GAMEX( 1981, turbo,  0,     turbo, turbo, turbo,        ROT270, "Sega", "Turbo", GAME_NO_COCKTAIL )
GAMEX( 1981, turboa, turbo, turbo, turbo, decode_turbo, ROT270, "Sega", "Turbo (encrypted set 1)", GAME_NO_COCKTAIL )
GAMEX( 1981, turbob, turbo, turbo, turbo, decode_turbo, ROT270, "Sega", "Turbo (encrypted set 2)", GAME_NO_COCKTAIL )
