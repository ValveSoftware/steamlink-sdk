#include "../vidhrdw/cheekyms.cpp"

/*************************************************************************
 Universal Cheeky Mouse Driver
 (c)Lee Taylor May/June 1998, All rights reserved.

 For use only in offical Mame releases.
 Not to be distributed as part of any commerical work.
**************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"




void cheekyms_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void cheekyms_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( cheekyms_sprite_w );
WRITE_HANDLER( cheekyms_port_40_w );
WRITE_HANDLER( cheekyms_port_80_w );


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM},
	{ 0x3000, 0x33ff, MRA_RAM},
	{ 0x3800, 0x3bff, MRA_RAM},	/* screen RAM */
	{ -1 }	/* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x3000, 0x33ff, MWA_RAM },
	{ 0x3800, 0x3bff, videoram_w, &videoram, &videoram_size },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r },
	{ 0x01, 0x01, input_port_1_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x20, 0x3f, cheekyms_sprite_w },
	{ 0x40, 0x40, cheekyms_port_40_w },
	{ 0x80, 0x80, cheekyms_port_80_w },
	{ -1 }	/* end of table */
};


static int cheekyms_interrupt(void)
{
	if (readinputport(2) & 1)	/* Coin */
		return nmi_interrupt();
	else return interrupt();
}


INPUT_PORTS_START( cheekyms )
	PORT_START      /* IN0 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
//PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x40, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x40, "3000" )
	PORT_DIPSETTING(    0x80, "4500" )
	PORT_DIPSETTING(    0xc0, "6000" )
	PORT_DIPSETTING(    0x00, "None" )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START	/* FAKE */
	/* The coin slots are not memory mapped. Coin  causes a NMI, */
	/* This fake input port is used by the interrupt */
	/* handler to be notified of coin insertions. We use IMPULSE to */
	/* trigger exactly one interrupt, without having to check when the */
	/* user releases the key. */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 16*16 sprites */
	256,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 256*8*8 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 64*32*8, 0 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	{ 0*16, 1*16,  2*16,  3*16,  4*16,  5*16,  6*16,  7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	32*8	/* every sprite takes 32 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   0,    32 },
	{ REGION_GFX2, 0, &spritelayout, 32*4, 16 },
	{ -1 } /* end of array */
};


static struct DACinterface dac_interface =
{
	1,
	{ 100 }
};


static struct MachineDriver machine_driver_cheekyms =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			5000000/2,  /* 2.5 Mhz */
			readmem, writemem,
			readport, writeport,
			cheekyms_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
  	32*8, 32*8, { 0*8, 32*8-1, 4*8, 28*8-1 },
	gfxdecodeinfo,
	64*3,64*3,
	cheekyms_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	cheekyms_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};




/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( cheekyms )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "cm03.c5",       0x0000, 0x0800, 0x1ad0cb40 )
	ROM_LOAD( "cm04.c6",       0x0800, 0x0800, 0x2238f607 )
	ROM_LOAD( "cm05.c7",       0x1000, 0x0800, 0x4169eba8 )
	ROM_LOAD( "cm06.c8",       0x1800, 0x0800, 0x7031660c )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cm01.c1",       0x0000, 0x0800, 0x26f73bd7 )
	ROM_LOAD( "cm02.c2",       0x0800, 0x0800, 0x885887c3 )

	ROM_REGION( 0x1000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cm07.n5",       0x0000, 0x0800, 0x2738c88d )
	ROM_LOAD( "cm08.n6",       0x0800, 0x0800, 0xb3fbd4ac )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "cm.m8",         0x0000, 0x0020, 0x2386bc68 )	 /* Character colors \ Selected by Bit 6 of Port 0x80 */
	ROM_LOAD( "cm.m9",         0x0020, 0x0020, 0xdb9c59a5 )	 /* Character colors /                                */
	ROM_LOAD( "cm.p3",         0x0040, 0x0020, 0x6ac41516 )  /* Sprite colors */
ROM_END



GAMEX( 1980?, cheekyms, 0, cheekyms, cheekyms, 0, ROT270, "Universal", "Cheeky Mouse", GAME_WRONG_COLORS | GAME_IMPERFECT_SOUND )
