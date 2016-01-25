#include "../vidhrdw/mrdo.cpp"

/***************************************************************************

Mr Do! memory map (preliminary)

driver by Nicola Salmoria

0000-7fff ROM
8000-83ff color RAM 1
8400-87ff video RAM 1
8800-8bff color RAM 2
8c00-8fff video RAM 2
e000-efff RAM

memory mapped ports:

read:
9803      SECRE 1/6-J2-11
a000      IN0
a001      IN1
a002      DSW1
a003      DSW2

write:
9000-90ff sprites, 64 groups of 4 bytes.
9800      flip (bit 0) playfield priority selector? (bits 1-3)
9801      sound port 1
9802      sound port 2
f000      playfield 0 Y scroll position (not used by Mr. Do!)
f800      playfield 0 X scroll position

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"



extern unsigned char *mrdo_bgvideoram,*mrdo_fgvideoram;
WRITE_HANDLER( mrdo_bgvideoram_w );
WRITE_HANDLER( mrdo_fgvideoram_w );
WRITE_HANDLER( mrdo_scrollx_w );
WRITE_HANDLER( mrdo_scrolly_w );
WRITE_HANDLER( mrdo_flipscreen_w );
void mrdo_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int mrdo_vh_start(void);
void mrdo_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



/* this looks like some kind of protection. The game doesn't clear the screen */
/* if a read from this address doesn't return the value it expects. */
READ_HANDLER( mrdo_SECRE_r )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	return RAM[ cpu_get_reg(Z80_HL) ];
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_RAM },	/* video and color RAM */
	{ 0x9803, 0x9803, mrdo_SECRE_r },
	{ 0xa000, 0xa000, input_port_0_r },	/* IN0 */
	{ 0xa001, 0xa001, input_port_1_r },	/* IN1 */
	{ 0xa002, 0xa002, input_port_2_r },	/* DSW1 */
	{ 0xa003, 0xa003, input_port_3_r },	/* DSW2 */
	{ 0xe000, 0xefff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, mrdo_bgvideoram_w, &mrdo_bgvideoram },
	{ 0x8800, 0x8fff, mrdo_fgvideoram_w, &mrdo_fgvideoram },
	{ 0x9000, 0x90ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x9800, 0x9800, mrdo_flipscreen_w },	/* screen flip + playfield priority */
	{ 0x9801, 0x9801, SN76496_0_w },
	{ 0x9802, 0x9802, SN76496_1_w },
	{ 0xe000, 0xefff, MWA_RAM },
	{ 0xf000, 0xf7ff, mrdo_scrollx_w },
	{ 0xf800, 0xffff, mrdo_scrolly_w },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( mrdo )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_TILT )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "Easy" )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_BITX(    0x04, 0x04, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Rack Test", KEYCODE_F1, IP_JOY_NONE )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Special" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, "Extra" )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x80, "4" )
	PORT_DIPSETTING(    0x40, "5" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* settings 0x01 thru 0x05 all give 1 Coin/1 Credit */
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* settings 0x10 thru 0x50 all give 1 Coin/1 Credit */
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 0, 512*8*8 },	/* the two bitplanes are separated */
	{ 7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	128,	/* 128 sprites */
	2,	/* 2 bits per pixel */
	{ 4, 0 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 3, 2, 1, 0, 8+3, 8+2, 8+1, 8+0,
			16+3, 16+2, 16+1, 16+0, 24+3, 24+2, 24+1, 24+0 },
	{ 0*16, 2*16, 4*16, 6*16, 8*16, 10*16, 12*16, 14*16,
			16*16, 18*16, 20*16, 22*16, 24*16, 26*16, 28*16, 30*16 },
	64*8	/* every sprite takes 64 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,      0, 64 },	/* colors 0-255 directly mapped */
	{ REGION_GFX2, 0, &charlayout,      0, 64 },
	{ REGION_GFX3, 0, &spritelayout, 4*64, 16 },
	{ -1 } /* end of array */
};



static struct SN76496interface sn76496_interface =
{
	2,	/* 2 chips */
	{ 4000000, 4000000 },	/* 4 MHz */
	{ 50, 50 }
};



static struct MachineDriver machine_driver_mrdo =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4 Mhz */
			readmem,writemem,0,0,
			interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 4*8, 28*8-1 },
	gfxdecodeinfo,
	256,64*4+16*4,
	mrdo_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	mrdo_vh_start,
	0,
	mrdo_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SN76496,
			&sn76496_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( mrdo )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a4-01.bin",    0x0000, 0x2000, 0x03dcfba2 )
	ROM_LOAD( "c4-02.bin",    0x2000, 0x2000, 0x0ecdd39c )
	ROM_LOAD( "e4-03.bin",    0x4000, 0x2000, 0x358f5dc2 )
	ROM_LOAD( "f4-04.bin",    0x6000, 0x2000, 0xf4190cfc )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "s8-09.bin",    0x0000, 0x1000, 0xaa80c5b6 )
	ROM_LOAD( "u8-10.bin",    0x1000, 0x1000, 0xd20ec85b )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "r8-08.bin",    0x0000, 0x1000, 0xdbdc9ffa )
	ROM_LOAD( "n8-07.bin",    0x1000, 0x1000, 0x4b9973db )

	ROM_REGION( 0x2000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "h5-05.bin",    0x0000, 0x1000, 0xe1218cc5 )
	ROM_LOAD( "k5-06.bin",    0x1000, 0x1000, 0xb1f68b04 )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "u02--2.bin",   0x0000, 0x0020, 0x238a65d7 )	/* palette (high bits) */
	ROM_LOAD( "t02--3.bin",   0x0020, 0x0020, 0xae263dc0 )	/* palette (low bits) */
	ROM_LOAD( "f10--1.bin",   0x0040, 0x0020, 0x16ee4ca2 )	/* sprite color lookup table */
ROM_END

ROM_START( mrdot )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "d1",           0x0000, 0x2000, 0x3dcd9359 )
	ROM_LOAD( "d2",           0x2000, 0x2000, 0x710058d8 )
	ROM_LOAD( "d3",           0x4000, 0x2000, 0x467d12d8 )
	ROM_LOAD( "d4",           0x6000, 0x2000, 0xfce9afeb )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "d9",           0x0000, 0x1000, 0xde4cfe66 )
	ROM_LOAD( "d10",          0x1000, 0x1000, 0xa6c2f38b )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "r8-08.bin",    0x0000, 0x1000, 0xdbdc9ffa )
	ROM_LOAD( "n8-07.bin",    0x1000, 0x1000, 0x4b9973db )

	ROM_REGION( 0x2000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "h5-05.bin",    0x0000, 0x1000, 0xe1218cc5 )
	ROM_LOAD( "k5-06.bin",    0x1000, 0x1000, 0xb1f68b04 )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "u02--2.bin",   0x0000, 0x0020, 0x238a65d7 )	/* palette (high bits) */
	ROM_LOAD( "t02--3.bin",   0x0020, 0x0020, 0xae263dc0 )	/* palette (low bits) */
	ROM_LOAD( "f10--1.bin",   0x0040, 0x0020, 0x16ee4ca2 )	/* sprite color lookup table */
ROM_END

ROM_START( mrdofix )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "d1",           0x0000, 0x2000, 0x3dcd9359 )
	ROM_LOAD( "d2",           0x2000, 0x2000, 0x710058d8 )
	ROM_LOAD( "dofix.d3",     0x4000, 0x2000, 0x3a7d039b )
	ROM_LOAD( "dofix.d4",     0x6000, 0x2000, 0x32db845f )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "d9",           0x0000, 0x1000, 0xde4cfe66 )
	ROM_LOAD( "d10",          0x1000, 0x1000, 0xa6c2f38b )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "r8-08.bin",    0x0000, 0x1000, 0xdbdc9ffa )
	ROM_LOAD( "n8-07.bin",    0x1000, 0x1000, 0x4b9973db )

	ROM_REGION( 0x2000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "h5-05.bin",    0x0000, 0x1000, 0xe1218cc5 )
	ROM_LOAD( "k5-06.bin",    0x1000, 0x1000, 0xb1f68b04 )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "u02--2.bin",   0x0000, 0x0020, 0x238a65d7 )  /* palette (high bits) */
	ROM_LOAD( "t02--3.bin",   0x0020, 0x0020, 0xae263dc0 )  /* palette (low bits) */
	ROM_LOAD( "f10--1.bin",   0x0040, 0x0020, 0x16ee4ca2 )  /* sprite color lookup table */
ROM_END

ROM_START( mrlo )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "mrlo01.bin",   0x0000, 0x2000, 0x6f455e7d )
	ROM_LOAD( "d2",           0x2000, 0x2000, 0x710058d8 )
	ROM_LOAD( "dofix.d3",     0x4000, 0x2000, 0x3a7d039b )
	ROM_LOAD( "mrlo04.bin",   0x6000, 0x2000, 0x49c10274 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mrlo09.bin",   0x0000, 0x1000, 0xfdb60d0d )
	ROM_LOAD( "mrlo10.bin",   0x1000, 0x1000, 0x0492c10e )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "r8-08.bin",    0x0000, 0x1000, 0xdbdc9ffa )
	ROM_LOAD( "n8-07.bin",    0x1000, 0x1000, 0x4b9973db )

	ROM_REGION( 0x2000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "h5-05.bin",    0x0000, 0x1000, 0xe1218cc5 )
	ROM_LOAD( "k5-06.bin",    0x1000, 0x1000, 0xb1f68b04 )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "u02--2.bin",   0x0000, 0x0020, 0x238a65d7 )	/* palette (high bits) */
	ROM_LOAD( "t02--3.bin",   0x0020, 0x0020, 0xae263dc0 )	/* palette (low bits) */
	ROM_LOAD( "f10--1.bin",   0x0040, 0x0020, 0x16ee4ca2 )	/* sprite color lookup table */
ROM_END

ROM_START( mrdu )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "d1",           0x0000, 0x2000, 0x3dcd9359 )
	ROM_LOAD( "d2",           0x2000, 0x2000, 0x710058d8 )
	ROM_LOAD( "d3",           0x4000, 0x2000, 0x467d12d8 )
	ROM_LOAD( "du4.bin",      0x6000, 0x2000, 0x893bc218 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "du9.bin",      0x0000, 0x1000, 0x4090dcdc )
	ROM_LOAD( "du10.bin",     0x1000, 0x1000, 0x1e63ab69 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "r8-08.bin",    0x0000, 0x1000, 0xdbdc9ffa )
	ROM_LOAD( "n8-07.bin",    0x1000, 0x1000, 0x4b9973db )

	ROM_REGION( 0x2000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "h5-05.bin",    0x0000, 0x1000, 0xe1218cc5 )
	ROM_LOAD( "k5-06.bin",    0x1000, 0x1000, 0xb1f68b04 )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "u02--2.bin",   0x0000, 0x0020, 0x238a65d7 )	/* palette (high bits) */
	ROM_LOAD( "t02--3.bin",   0x0020, 0x0020, 0xae263dc0 )	/* palette (low bits) */
	ROM_LOAD( "f10--1.bin",   0x0040, 0x0020, 0x16ee4ca2 )	/* sprite color lookup table */
ROM_END

ROM_START( mrdoy )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "dosnow.1",     0x0000, 0x2000, 0xd3454e2c )
	ROM_LOAD( "dosnow.2",     0x2000, 0x2000, 0x5120a6b2 )
	ROM_LOAD( "dosnow.3",     0x4000, 0x2000, 0x96416dbe )
	ROM_LOAD( "dosnow.4",     0x6000, 0x2000, 0xc05051b6 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dosnow.9",     0x0000, 0x1000, 0x85d16217 )
	ROM_LOAD( "dosnow.10",    0x1000, 0x1000, 0x61a7f54b )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dosnow.8",     0x0000, 0x1000, 0x2bd1239a )
	ROM_LOAD( "dosnow.7",     0x1000, 0x1000, 0xac8ffddf )

	ROM_REGION( 0x2000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dosnow.5",     0x0000, 0x1000, 0x7662d828 )
	ROM_LOAD( "dosnow.6",     0x1000, 0x1000, 0x413f88d1 )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "u02--2.bin",   0x0000, 0x0020, 0x238a65d7 )	/* palette (high bits) */
	ROM_LOAD( "t02--3.bin",   0x0020, 0x0020, 0xae263dc0 )	/* palette (low bits) */
	ROM_LOAD( "f10--1.bin",   0x0040, 0x0020, 0x16ee4ca2 )	/* sprite color lookup table */
ROM_END

ROM_START( yankeedo )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a4-01.bin",    0x0000, 0x2000, 0x03dcfba2 )
	ROM_LOAD( "yd_d2.c4",     0x2000, 0x2000, 0x7c9d7ce0 )
	ROM_LOAD( "e4-03.bin",    0x4000, 0x2000, 0x358f5dc2 )
	ROM_LOAD( "f4-04.bin",    0x6000, 0x2000, 0xf4190cfc )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "s8-09.bin",    0x0000, 0x1000, 0xaa80c5b6 )
	ROM_LOAD( "u8-10.bin",    0x1000, 0x1000, 0xd20ec85b )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "r8-08.bin",    0x0000, 0x1000, 0xdbdc9ffa )
	ROM_LOAD( "n8-07.bin",    0x1000, 0x1000, 0x4b9973db )

	ROM_REGION( 0x2000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "yd_d5.h5",     0x0000, 0x1000, 0xf530b79b )
	ROM_LOAD( "yd_d6.k5",     0x1000, 0x1000, 0x790579aa )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "u02--2.bin",   0x0000, 0x0020, 0x238a65d7 )	/* palette (high bits) */
	ROM_LOAD( "t02--3.bin",   0x0020, 0x0020, 0xae263dc0 )	/* palette (low bits) */
	ROM_LOAD( "f10--1.bin",   0x0040, 0x0020, 0x16ee4ca2 )	/* sprite color lookup table */
ROM_END



GAME( 1982, mrdo,     0,    mrdo, mrdo, 0, ROT270, "Universal", "Mr. Do! (Universal)" )
GAME( 1982, mrdot,    mrdo, mrdo, mrdo, 0, ROT270, "Universal (Taito license)", "Mr. Do! (Taito)" )
GAME( 1982, mrdofix,  mrdo, mrdo, mrdo, 0, ROT270, "Universal (Taito license)", "Mr. Do! (bugfixed)" )
GAME( 1982, mrlo,     mrdo, mrdo, mrdo, 0, ROT270, "bootleg", "Mr. Lo!" )
GAME( 1982, mrdu,     mrdo, mrdo, mrdo, 0, ROT270, "bootleg", "Mr. Du!" )
GAME( 1982, mrdoy,    mrdo, mrdo, mrdo, 0, ROT270, "bootleg", "Mr. Do! (Yukidaruma)" )
GAME( 1982, yankeedo, mrdo, mrdo, mrdo, 0, ROT270, "hack", "Yankee DO!" )
