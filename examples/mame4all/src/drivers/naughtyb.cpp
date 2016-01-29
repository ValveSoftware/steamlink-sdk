#include "../vidhrdw/naughtyb.cpp"

/***************************************************************************

Naughty Boy driver by Sal and John Bugliarisi.
This driver is based largely on MAME's Phoenix driver, since Naughty Boy runs
on similar hardware as Phoenix. Phoenix driver provided by Brad Oliver.
Thanks to Richard Davies for his Phoenix emulator source.


Naughty Boy memory map

0000-3fff 16Kb Program ROM
4000-7fff 1Kb Work RAM (mirrored)
8000-87ff 2Kb Video RAM Charset A (lower priority, mirrored)
8800-8fff 2Kb Video RAM Charset b (higher priority, mirrored)
9000-97ff 2Kb Video Control write-only (mirrored)
9800-9fff 2Kb Video Scroll Register (mirrored)
a000-a7ff 2Kb Sound Control A (mirrored)
a800-afff 2Kb Sound Control B (mirrored)
b000-b7ff 2Kb 8bit Game Control read-only (mirrored)
b800-bfff 1Kb 8bit Dip Switch read-only (mirrored)
c000-0000 16Kb Unused

memory mapped ports:

read-only:
b000-b7ff IN
b800-bfff DSW


Naughty Boy Switch Settings
(C)1982 Cinematronics

 --------------------------------------------------------
|Option |Factory|Descrpt| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
 ------------------------|-------------------------------
|Lives	|		|2		|on |on |	|	|	|	|	|	|
 ------------------------ -------------------------------
|		|	X	|3		|off|on |	|	|	|	|	|	|
 ------------------------ -------------------------------
|		|		|4		|on |off|	|	|	|	|	|	|
 ------------------------ -------------------------------
|		|		|5		|off|off|	|	|	|	|	|	|
 ------------------------ -------------------------------
|Extra	|		|10000	|	|	|on |on |	|	|	|	|
 ------------------------ -------------------------------
|		|	X	|30000	|	|	|off|on |	|	|	|	|
 ------------------------ -------------------------------
|		|		|50000	|	|	|on |off|	|	|	|	|
 ------------------------ -------------------------------
|		|		|70000	|	|	|off|off|	|	|	|	|
 ------------------------ -------------------------------
|Credits|		|2c, 1p |	|	|	|	|on |on |	|	|
 ------------------------ -------------------------------
|		|	X	|1c, 1p |	|	|	|	|off|on |	|	|
 ------------------------ -------------------------------
|		|		|1c, 2p |	|	|	|	|on |off|	|	|
 ------------------------ -------------------------------
|		|		|4c, 3p |	|	|	|	|off|off|	|	|
 ------------------------ -------------------------------
|Dffclty|	X	|Easier |	|	|	|	|	|	|on |	|
 ------------------------ -------------------------------
|		|		|Harder |	|	|	|	|	|	|off|	|
 ------------------------ -------------------------------
| Type	|		|Upright|	|	|	|	|	|	|	|on |
 ------------------------ -------------------------------
|		|		|Cktail |	|	|	|	|	|	|	|off|
 ------------------------ -------------------------------

*
* Pop Flamer
*

Pop Flamer appears to run on identical hardware as Naughty Boy.
The dipswitches are even identical. Spooky.

						1	2	3	4	5	6	7	8
-------------------------------------------------------
Number of Mr. Mouse 2 |ON |ON |   |   |   |   |   |   |
					3 |OFF|ON |   |   |   |   |   |   |
					4 |ON |OFF|   |   |   |   |   |   |
					5 |OFF|OFF|   |   |   |   |   |   |
-------------------------------------------------------
Extra Mouse    10,000 |   |   |ON |ON |   |   |   |   |
			   30,000 |   |   |OFF|ON |   |   |   |   |
			   50,000 |   |   |ON |OFF|   |   |   |   |
			   70,000 |   |   |OFF|OFF|   |   |   |   |
-------------------------------------------------------
Credit	2 coin 1 play |   |   |   |   |ON |ON |   |   |
		1 coin 1 play |   |   |   |   |OFF|ON |   |   |
		1 coin 2 play |   |   |   |   |ON |OFF|   |   |
		1 coin 3 play |   |   |   |   |OFF|OFF|   |   |
-------------------------------------------------------
Skill		   Easier |   |   |   |   |   |   |ON |   |
			   Harder |   |   |   |   |   |   |OFF|   |
-------------------------------------------------------
Game style		Table |   |   |   |   |   |   |   |OFF|
			  Upright |   |   |   |   |   |   |   |ON |


TODO:
	* sounds are a little skanky
	* Figure out how cocktail/upright mode works

 ***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



extern unsigned char *naughtyb_videoram2;
extern unsigned char *naughtyb_scrollreg;

WRITE_HANDLER( naughtyb_videoram2_w );
WRITE_HANDLER( naughtyb_scrollreg_w );
WRITE_HANDLER( naughtyb_videoreg_w );
WRITE_HANDLER( popflame_videoreg_w );
int naughtyb_vh_start(void);
void naughtyb_vh_stop(void);
void naughtyb_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void naughtyb_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( pleiads_sound_control_a_w );
WRITE_HANDLER( pleiads_sound_control_b_w );
int naughtyb_sh_start(const struct MachineSound *msound);
int popflame_sh_start(const struct MachineSound *msound);
void pleiads_sh_stop(void);
void pleiads_sh_update(void);

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x8fff, MRA_RAM },
	{ 0xb000, 0xb7ff, input_port_0_r }, 	/* IN0 */
	{ 0xb800, 0xbfff, input_port_1_r }, 	/* DSW */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x7fff, MWA_RAM },
	{ 0x8000, 0x87ff, videoram_w, &videoram, &videoram_size },
	{ 0x8800, 0x8fff, naughtyb_videoram2_w, &naughtyb_videoram2 },
	{ 0x9000, 0x97ff, naughtyb_videoreg_w },
	{ 0x9800, 0x9fff, MWA_RAM, &naughtyb_scrollreg },
	{ 0xa000, 0xa7ff, pleiads_sound_control_a_w },
	{ 0xa800, 0xafff, pleiads_sound_control_b_w },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress popflame_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x7fff, MWA_RAM },
	{ 0x8000, 0x87ff, videoram_w, &videoram, &videoram_size },
	{ 0x8800, 0x8fff, naughtyb_videoram2_w, &naughtyb_videoram2 },
	{ 0x9000, 0x97ff, popflame_videoreg_w },
	{ 0x9800, 0x9fff, MWA_RAM, &naughtyb_scrollreg },
	{ 0xa000, 0xa7ff, pleiads_sound_control_a_w },
	{ 0xa800, 0xafff, pleiads_sound_control_b_w },
	{ -1 }	/* end of table */
};



/***************************************************************************

  Naughty Boy doesn't have VBlank interrupts.
  Interrupts are still used by the game: but they are related to coin
  slots.

***************************************************************************/

int naughtyb_interrupt(void)
{
	if (readinputport(2) & 1)
		return nmi_interrupt();
	else return ignore_interrupt();
}

INPUT_PORTS_START( naughtyb )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )

	PORT_START	/* DSW0 & VBLANK */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x00, "2" )
	PORT_DIPSETTING(	0x01, "3" )
	PORT_DIPSETTING(	0x02, "4" )
	PORT_DIPSETTING(	0x03, "5" )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x00, "10000" )
	PORT_DIPSETTING(	0x04, "30000" )
	PORT_DIPSETTING(	0x08, "50000" )
	PORT_DIPSETTING(	0x0c, "70000" )
	PORT_DIPNAME( 0x30, 0x10, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x10, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x20, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x30, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x00, "Easy" )
	PORT_DIPSETTING(	0x40, "Hard" )
	/* This is a bit of a mystery. Bit 0x80 is read as the vblank, but
	   it apparently also controls cocktail/table mode. */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )

	PORT_START	/* FAKE */
	/* The coin slots are not memory mapped. */
	/* This fake input port is used by the interrupt */
	/* handler to be notified of coin insertions. We use IMPULSE to */
	/* trigger exactly one interrupt, without having to check when the */
	/* user releases the key. */
		PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,		/* 2 bits per pixel */
	{ 512*8*8, 0 }, /* the two bitplanes are separated */
	{ 7, 6, 5, 4, 3, 2, 1, 0 }, /* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8 	/* every char takes 8 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,	  0, 32 },
	{ REGION_GFX2, 0, &charlayout, 32*4, 32 },
	{ -1 } /* end of array */
};



static struct CustomSound_interface naughtyb_custom_interface =
{
	naughtyb_sh_start,
	pleiads_sh_stop,
	pleiads_sh_update
};

static struct CustomSound_interface popflame_custom_interface =
{
	popflame_sh_start,
	pleiads_sh_stop,
	pleiads_sh_update
};

static struct TMS36XXinterface tms3615_interface =
{
	1,
	{ 60		},	/* mixing level */
	{ TMS3615	},	/* TMS36xx subtype */
	{ 350		},	/* base clock (one octave below A) */
	/*
	 * Decay times of the voices; NOTE: it's unknown if
	 * the the TMS3615 mixes more than one voice internally.
	 * A wav taken from Pop Flamer sounds like there
	 * are at least no 'odd' harmonics (5 1/3' and 2 2/3')
	 */
	{ {0.15,0.20,0,0,0,0} }
};



static struct MachineDriver machine_driver_naughtyb =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			1500000,	/* 3 Mhz ? */
			readmem,writemem,0,0,
			naughtyb_interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	36*8, 28*8, { 0*8, 36*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	256,32*4+32*4,
	naughtyb_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	naughtyb_vh_start,
	naughtyb_vh_stop,
	naughtyb_vh_screenrefresh,

	/* sound hardware */
	/* uses the TMS3615NS for sound */
	0,0,0,0,
	{
		{
			SOUND_TMS36XX,
			&tms3615_interface
		},
		{
			SOUND_CUSTOM,
			&naughtyb_custom_interface
		}
	}
};

/* Exactly the same but for the writemem handler */
static struct MachineDriver machine_driver_popflame =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			1500000,	/* 3 Mhz ? */
			readmem,popflame_writemem,0,0,
			naughtyb_interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	36*8, 28*8, { 0*8, 36*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	256,32*4+32*4,
	naughtyb_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	naughtyb_vh_start,
	naughtyb_vh_stop,
	naughtyb_vh_screenrefresh,

	/* sound hardware */
	/* uses the TMS3615NS for sound */
	0,0,0,0,
	{
		{
			SOUND_TMS36XX,
			&tms3615_interface
		},
		{
			SOUND_CUSTOM,
			&popflame_custom_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( naughtyb )
	ROM_REGION( 0x10000, REGION_CPU1 )		/* 64k for code */
	ROM_LOAD( "1.30",	   0x0000, 0x0800, 0xf6e1178e )
	ROM_LOAD( "2.29",	   0x0800, 0x0800, 0xb803eb8c )
	ROM_LOAD( "3.28",	   0x1000, 0x0800, 0x004d0ba7 )
	ROM_LOAD( "4.27",	   0x1800, 0x0800, 0x3c7bcac6 )
	ROM_LOAD( "5.26",	   0x2000, 0x0800, 0xea80f39b )
	ROM_LOAD( "6.25",	   0x2800, 0x0800, 0x66d9f942 )
	ROM_LOAD( "7.24",	   0x3000, 0x0800, 0x00caf9be )
	ROM_LOAD( "8.23",	   0x3800, 0x0800, 0x17c3b6fb )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15.44",	   0x0000, 0x0800, 0xd692f9c7 )
	ROM_LOAD( "16.43",	   0x0800, 0x0800, 0xd3ba8b27 )
	ROM_LOAD( "13.46",	   0x1000, 0x0800, 0xc1669cd5 )
	ROM_LOAD( "14.45",	   0x1800, 0x0800, 0xeef2c8e5 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11.48",	   0x0000, 0x0800, 0x75ec9710 )
	ROM_LOAD( "12.47",	   0x0800, 0x0800, 0xef0706c3 )
	ROM_LOAD( "9.50",	   0x1000, 0x0800, 0x8c8db764 )
	ROM_LOAD( "10.49",	   0x1800, 0x0800, 0xc97c97b9 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "6301-1.63", 0x0000, 0x0100, 0x98ad89a1 ) /* palette low bits */
	ROM_LOAD( "6301-1.64", 0x0100, 0x0100, 0x909107d4 ) /* palette high bits */
ROM_END

ROM_START( naughtya )
	ROM_REGION( 0x10000, REGION_CPU1 )		/* 64k for code */
	ROM_LOAD( "91", 	   0x0000, 0x0800, 0x42b14bc7 )
	ROM_LOAD( "92", 	   0x0800, 0x0800, 0xa24674b4 )
	ROM_LOAD( "3.28",	   0x1000, 0x0800, 0x004d0ba7 )
	ROM_LOAD( "4.27",	   0x1800, 0x0800, 0x3c7bcac6 )
	ROM_LOAD( "95", 	   0x2000, 0x0800, 0xe282f1b8 )
	ROM_LOAD( "96", 	   0x2800, 0x0800, 0x61178ff2 )
	ROM_LOAD( "97", 	   0x3000, 0x0800, 0x3cafde88 )
	ROM_LOAD( "8.23",	   0x3800, 0x0800, 0x17c3b6fb )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15.44",	   0x0000, 0x0800, 0xd692f9c7 )
	ROM_LOAD( "16.43",	   0x0800, 0x0800, 0xd3ba8b27 )
	ROM_LOAD( "13.46",	   0x1000, 0x0800, 0xc1669cd5 )
	ROM_LOAD( "14.45",	   0x1800, 0x0800, 0xeef2c8e5 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11.48",	   0x0000, 0x0800, 0x75ec9710 )
	ROM_LOAD( "12.47",	   0x0800, 0x0800, 0xef0706c3 )
	ROM_LOAD( "9.50",	   0x1000, 0x0800, 0x8c8db764 )
	ROM_LOAD( "10.49",	   0x1800, 0x0800, 0xc97c97b9 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "6301-1.63", 0x0000, 0x0100, 0x98ad89a1 ) /* palette low bits */
	ROM_LOAD( "6301-1.64", 0x0100, 0x0100, 0x909107d4 ) /* palette high bits */
ROM_END

ROM_START( naughtyc )
	ROM_REGION( 0x10000, REGION_CPU1 )		/* 64k for code */
	ROM_LOAD( "nb1ic30",   0x0000, 0x0800, 0x3f482fa3 )
	ROM_LOAD( "nb2ic29",   0x0800, 0x0800, 0x7ddea141 )
	ROM_LOAD( "nb3ic28",   0x1000, 0x0800, 0x8c72a069 )
	ROM_LOAD( "nb4ic27",   0x1800, 0x0800, 0x30feae51 )
	ROM_LOAD( "nb5ic26",   0x2000, 0x0800, 0x05242fd0 )
	ROM_LOAD( "nb6ic25",   0x2800, 0x0800, 0x7a12ffea )
	ROM_LOAD( "nb7ic24",   0x3000, 0x0800, 0x9cc287df )
	ROM_LOAD( "nb8ic23",   0x3800, 0x0800, 0x4d84ff2c )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15.44",	   0x0000, 0x0800, 0xd692f9c7 )
	ROM_LOAD( "16.43",	   0x0800, 0x0800, 0xd3ba8b27 )
	ROM_LOAD( "13.46",	   0x1000, 0x0800, 0xc1669cd5 )
	ROM_LOAD( "14.45",	   0x1800, 0x0800, 0xeef2c8e5 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nb11ic48",  0x0000, 0x0800, 0x23271a13 )
	ROM_LOAD( "12.47",	   0x0800, 0x0800, 0xef0706c3 )
	ROM_LOAD( "nb9ic50",   0x1000, 0x0800, 0xd6949c27 )
	ROM_LOAD( "10.49",	   0x1800, 0x0800, 0xc97c97b9 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "6301-1.63", 0x0000, 0x0100, 0x98ad89a1 ) /* palette low bits */
	ROM_LOAD( "6301-1.64", 0x0100, 0x0100, 0x909107d4 ) /* palette high bits */
ROM_END

ROM_START( popflame )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "ic86.pop",	  0x0000, 0x1000, 0x5e32bbdf )
	ROM_LOAD( "ic80.pop",	  0x1000, 0x1000, 0xb77abf3d )
	ROM_LOAD( "ic94.pop",	  0x2000, 0x1000, 0x945a3c0f )
	ROM_LOAD( "ic100.pop",	  0x3000, 0x1000, 0xf9f2343b )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic13.pop",	  0x0000, 0x1000, 0x2367131e )
	ROM_LOAD( "ic3.pop",	  0x1000, 0x1000, 0xdeed0a8b )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic29.pop",	  0x0000, 0x1000, 0x7b54f60f )
	ROM_LOAD( "ic38.pop",	  0x1000, 0x1000, 0xdd2d9601 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "ic53",		  0x0000, 0x0100, 0x6e66057f ) /* palette low bits */
	ROM_LOAD( "ic54",		  0x0100, 0x0100, 0x236bc771 ) /* palette high bits */
ROM_END

ROM_START( popflama )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "popflama.30",	 0x0000, 0x1000, 0xa9bb0e8a )
	ROM_LOAD( "popflama.28",	 0x1000, 0x1000, 0xdebe6d03 )
	ROM_LOAD( "popflama.26",	 0x2000, 0x1000, 0x09df0d4d )
	ROM_LOAD( "popflama.24",	 0x3000, 0x1000, 0xf399d553 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic13.pop",	  0x0000, 0x1000, 0x2367131e )
	ROM_LOAD( "ic3.pop",	  0x1000, 0x1000, 0xdeed0a8b )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic29.pop",	  0x0000, 0x1000, 0x7b54f60f )
	ROM_LOAD( "ic38.pop",	  0x1000, 0x1000, 0xdd2d9601 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "ic53",		  0x0000, 0x0100, 0x6e66057f ) /* palette low bits */
	ROM_LOAD( "ic54",		  0x0100, 0x0100, 0x236bc771 ) /* palette high bits */
ROM_END

ROM_START( popflamb )
	ROM_REGION( 0x10000, REGION_CPU1 )		/* 64k for code */
	ROM_LOAD( "ic86.bin",	  0x0000, 0x1000, 0x06397a4b )
	ROM_LOAD( "ic80.pop",	  0x1000, 0x1000, 0xb77abf3d )
	ROM_LOAD( "ic94.bin",	  0x2000, 0x1000, 0xae5248ae )
	ROM_LOAD( "ic100.pop",	  0x3000, 0x1000, 0xf9f2343b )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic13.pop",	  0x0000, 0x1000, 0x2367131e )
	ROM_LOAD( "ic3.pop",	  0x1000, 0x1000, 0xdeed0a8b )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ic29.pop",	  0x0000, 0x1000, 0x7b54f60f )
	ROM_LOAD( "ic38.pop",	  0x1000, 0x1000, 0xdd2d9601 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "ic53",		  0x0000, 0x0100, 0x6e66057f ) /* palette low bits */
	ROM_LOAD( "ic54",		  0x0100, 0x0100, 0x236bc771 ) /* palette high bits */
ROM_END



GAMEX( 1982, naughtyb, 0,		 naughtyb, naughtyb, 0, ROT90, "Jaleco", "Naughty Boy", GAME_NO_COCKTAIL )
GAMEX( 1982, naughtya, naughtyb, naughtyb, naughtyb, 0, ROT90, "bootleg", "Naughty Boy (bootleg)", GAME_NO_COCKTAIL )
GAMEX( 1982, naughtyc, naughtyb, naughtyb, naughtyb, 0, ROT90, "Jaleco (Cinematronics license)", "Naughty Boy (Cinematronics)", GAME_NO_COCKTAIL )
GAMEX( 1982, popflame, 0,		 popflame, naughtyb, 0, ROT90, "Jaleco", "Pop Flamer (set 1)", GAME_NO_COCKTAIL )
GAMEX( 1982, popflama, popflame, popflame, naughtyb, 0, ROT90, "Jaleco", "Pop Flamer (set 2)", GAME_NO_COCKTAIL )
GAMEX( 1982, popflamb, popflame, popflame, naughtyb, 0, ROT90, "Jaleco (Stern License)", "Pop Flamer (set 3)", GAME_NO_COCKTAIL )
