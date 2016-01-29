#include "../vidhrdw/bankp.cpp"

/***************************************************************************

Bank Panic memory map (preliminary)
Similar to Appoooh

driver by Nicola Salmoria


0000-dfff ROM
e000-e7ff RAM
f000-f3ff Video RAM #1
f400-f7ff Color RAM #1
f800-fbff Video RAM #2
fc00-ffff Color RAM #2

I/O
read:
00  IN0
01  IN1
02  IN2
04  DSW

write:
00  SN76496 #1
01  SN76496 #2
02  SN76496 #3
05  horizontal scroll
07  bit 0-1 = at least one of these two controls the playfield priority
    bit 2-3 = ?
    bit 4 = NMI enable
    bit 5 = flip screen
    bit 6-7 = ?

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


extern unsigned char *bankp_videoram2;
extern unsigned char *bankp_colorram2;
void bankp_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( bankp_videoram2_w );
WRITE_HANDLER( bankp_colorram2_w );
WRITE_HANDLER( bankp_scroll_w );
WRITE_HANDLER( bankp_out_w );
int bankp_vh_start(void);
void bankp_vh_stop(void);
void bankp_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xdfff, MRA_ROM },
	{ 0xe000, 0xe7ff, MRA_RAM },
	{ 0xf000, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xdfff, MWA_ROM },
	{ 0xe000, 0xe7ff, MWA_RAM },
	{ 0xf000, 0xf3ff, videoram_w, &videoram, &videoram_size },
	{ 0xf400, 0xf7ff, colorram_w, &colorram },
	{ 0xf800, 0xfbff, bankp_videoram2_w, &bankp_videoram2 },
	{ 0xfc00, 0xffff, bankp_colorram2_w, &bankp_colorram2 },
	{ -1 }	/* end of table */
};


static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r },	/* IN0 */
	{ 0x01, 0x01, input_port_1_r },	/* IN1 */
	{ 0x02, 0x02, input_port_2_r },	/* IN2 */
	{ 0x04, 0x04, input_port_3_r },	/* DSW */
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, SN76496_0_w },
	{ 0x01, 0x01, SN76496_1_w },
	{ 0x02, 0x02, SN76496_2_w },
	{ 0x05, 0x05, bankp_scroll_w },
	{ 0x07, 0x07, bankp_out_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( bankp )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0xf8, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* probably unused */

	PORT_START	/* DSW */
	PORT_DIPNAME( 0x03, 0x00, "Coin A/B" )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x04, 0x00, "Coin C" )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "70K 200K 500K ..." )
	PORT_DIPSETTING(    0x10, "100K 400K 800K ..." )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the bitplanes are packed in one byte */
	{ 8*8+3, 8*8+2, 8*8+1, 8*8+0, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout charlayout2 =
{
	8,8,	/* 8*8 characters */
	2048,	/* 2048 characters */
	3,	/* 3 bits per pixel */
	{ 0, 2048*8*8, 2*2048*8*8 },	/* the bitplanes are separated */
	{ 7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,      0, 32 },
	{ REGION_GFX2, 0, &charlayout2,  32*4, 16 },
	{ -1 } /* end of array */
};



static struct SN76496interface sn76496_interface =
{
	3,	/* 3 chips */
	{ 3867120, 3867120, 3867120 },	/* ?? the main oscillator is 15.46848 Mhz */
	{ 100, 100, 100 }
};



static struct MachineDriver machine_driver_bankp =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3867120,	/* ?? the main oscillator is 15.46848 Mhz */
			readmem,writemem,readport,writeport,
			nmi_interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8, { 3*8, 31*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32, 32*4+16*8,
	bankp_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	bankp_vh_start,
	bankp_vh_stop,
	bankp_vh_screenrefresh,

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

ROM_START( bankp )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "epr6175.bin",  0x0000, 0x4000, 0x044552b8 )
	ROM_LOAD( "epr6174.bin",  0x4000, 0x4000, 0xd29b1598 )
	ROM_LOAD( "epr6173.bin",  0x8000, 0x4000, 0xb8405d38 )
	ROM_LOAD( "epr6176.bin",  0xc000, 0x2000, 0xc98ac200 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "epr6165.bin",  0x0000, 0x2000, 0xaef34a93 )	/* playfield #1 chars */
	ROM_LOAD( "epr6166.bin",  0x2000, 0x2000, 0xca13cb11 )

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "epr6172.bin",  0x0000, 0x2000, 0xc4c4878b )	/* playfield #2 chars */
	ROM_LOAD( "epr6171.bin",  0x2000, 0x2000, 0xa18165a1 )
	ROM_LOAD( "epr6170.bin",  0x4000, 0x2000, 0xb58aa8fa )
	ROM_LOAD( "epr6169.bin",  0x6000, 0x2000, 0x1aa37fce )
	ROM_LOAD( "epr6168.bin",  0x8000, 0x2000, 0x05f3a867 )
	ROM_LOAD( "epr6167.bin",  0xa000, 0x2000, 0x3fa337e1 )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "pr6177.clr",   0x0000, 0x020, 0xeb70c5ae ) 	/* palette */
	ROM_LOAD( "pr6178.clr",   0x0020, 0x100, 0x0acca001 ) 	/* charset #1 lookup table */
	ROM_LOAD( "pr6179.clr",   0x0120, 0x100, 0xe53bafdb ) 	/* charset #2 lookup table */
ROM_END



GAME( 1984, bankp, 0, bankp, bankp, 0, ROT0, "Sega", "Bank Panic" )
