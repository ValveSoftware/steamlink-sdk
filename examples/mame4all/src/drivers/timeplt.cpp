#include "../sndhrdw/timeplt.cpp"
#include "../vidhrdw/timeplt.cpp"

/***************************************************************************

Time Pilot memory map (preliminary)

driver by Nicola Salmoria

Main processor memory map.
0000-5fff ROM
a000-a3ff Color RAM
a400-a7ff Video RAM
a800-afff RAM
b000-b7ff sprite RAM (only areas 0xb010 and 0xb410 are used).

memory mapped ports:

read:
c000      video scan line. This is used by the program to multiplex the cloud
          sprites, drawing them twice offset by 128 pixels.
c200      DSW2
c300      IN0
c320      IN1
c340      IN2
c360      DSW1

write:
c000      command for the audio CPU
c200      watchdog reset
c300      interrupt enable
c302      flip screen
c304      trigger interrupt on audio CPU
c308	  Protection ???  Stuffs in some values computed from ROM content
c30a	  coin counter 1
c30c	  coin counter 2

interrupts:
standard NMI at 0x66

SOUND BOARD:
same as Pooyan

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


extern unsigned char *timeplt_videoram,*timeplt_colorram;

void init_timeplt(void);
void init_psurge(void);
READ_HANDLER( timeplt_scanline_r );
WRITE_HANDLER( timeplt_videoram_w );
WRITE_HANDLER( timeplt_colorram_w );
WRITE_HANDLER( timeplt_flipscreen_w );
int  timeplt_vh_start(void);
void timeplt_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void timeplt_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/* defined in sndhrdw/timeplt.c */
extern struct MemoryReadAddress timeplt_sound_readmem[];
extern struct MemoryWriteAddress timeplt_sound_writemem[];
extern struct AY8910interface timeplt_ay8910_interface;
WRITE_HANDLER( timeplt_sh_irqtrigger_w );



static WRITE_HANDLER( timeplt_coin_counter_w )
{
	coin_counter_w(offset >> 1, data);
}

static READ_HANDLER( psurge_protection_r )
{
	return 0x80;
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6004, 0x6004, psurge_protection_r },	/* psurge only */
	{ 0xa000, 0xbfff, MRA_RAM },
	{ 0xc000, 0xc000, timeplt_scanline_r },
	{ 0xc200, 0xc200, input_port_4_r },	/* DSW2 */
	{ 0xc300, 0xc300, input_port_0_r },	/* IN0 */
	{ 0xc320, 0xc320, input_port_1_r },	/* IN1 */
	{ 0xc340, 0xc340, input_port_2_r },	/* IN2 */
	{ 0xc360, 0xc360, input_port_3_r },	/* DSW1 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0xa000, 0xa3ff, timeplt_colorram_w, &timeplt_colorram },
	{ 0xa400, 0xa7ff, timeplt_videoram_w, &timeplt_videoram },
	{ 0xa800, 0xafff, MWA_RAM },
	{ 0xb010, 0xb03f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xb410, 0xb43f, MWA_RAM, &spriteram_2 },
	{ 0xc000, 0xc000, soundlatch_w },
	{ 0xc200, 0xc200, watchdog_reset_w },
	{ 0xc300, 0xc300, interrupt_enable_w },
	{ 0xc302, 0xc302, timeplt_flipscreen_w },
	{ 0xc304, 0xc304, timeplt_sh_irqtrigger_w },
	{ 0xc30a, 0xc30c, timeplt_coin_counter_w },  /* c30b is not used */
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( timeplt )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "255", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x08, 0x08, "Bonus" )
	PORT_DIPSETTING(    0x08, "10000 50000" )
	PORT_DIPSETTING(    0x00, "20000 60000" )
	PORT_DIPNAME( 0x70, 0x70, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x70, "1 (Easiest)" )
	PORT_DIPSETTING(    0x60, "2" )
	PORT_DIPSETTING(    0x50, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x30, "5 (Average)" )
	PORT_DIPSETTING(    0x20, "6" )
	PORT_DIPSETTING(    0x10, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hardest)" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( psurge )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START /* DSW0 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Initial Energy" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPSETTING(    0x08, "6" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_5C ) )
	PORT_BITX(0x10,     0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Infinite Shots", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Stop at Junctions" )
	PORT_DIPSETTING(    0x80, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 4, 0 },
	{ 0, 1, 2, 3, 8*8+0,8*8+1,8*8+2,8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	256,	/* 256 sprites */
	2,	/* 2 bits per pixel */
	{ 4, 0 },
	{ 0, 1, 2, 3,  8*8, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3,  24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 64 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,        0, 32 },
	{ REGION_GFX2, 0, &spritelayout,   32*4, 64 },
	{ -1 } /* end of array */
};



static struct MachineDriver machine_driver_timeplt =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz (?) */
			readmem,writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318180/8,	/* 1.789772727 MHz */						\
			timeplt_sound_readmem,timeplt_sound_writemem,0,0,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32,32*4+64*4,
	timeplt_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	timeplt_vh_start,
	0,
	timeplt_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&timeplt_ay8910_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( timeplt )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "tm1",          0x0000, 0x2000, 0x1551f1b9 )
	ROM_LOAD( "tm2",          0x2000, 0x2000, 0x58636cb5 )
	ROM_LOAD( "tm3",          0x4000, 0x2000, 0xff4e0d83 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "tm7",          0x0000, 0x1000, 0xd66da813 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tm6",          0x0000, 0x2000, 0xc2507f40 )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tm4",          0x0000, 0x2000, 0x7e437c3e )
	ROM_LOAD( "tm5",          0x2000, 0x2000, 0xe8ca87b9 )

	ROM_REGION( 0x0240, REGION_PROMS )
	ROM_LOAD( "timeplt.b4",   0x0000, 0x0020, 0x34c91839 ) /* palette */
	ROM_LOAD( "timeplt.b5",   0x0020, 0x0020, 0x463b2b07 ) /* palette */
	ROM_LOAD( "timeplt.e9",   0x0040, 0x0100, 0x4bbb2150 ) /* sprite lookup table */
	ROM_LOAD( "timeplt.e12",  0x0140, 0x0100, 0xf7b7663e ) /* char lookup table */
ROM_END

ROM_START( timepltc )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "cd1y",         0x0000, 0x2000, 0x83ec72c2 )
	ROM_LOAD( "cd2y",         0x2000, 0x2000, 0x0dcf5287 )
	ROM_LOAD( "cd3y",         0x4000, 0x2000, 0xc789b912 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "tm7",          0x0000, 0x1000, 0xd66da813 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tm6",          0x0000, 0x2000, 0xc2507f40 )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tm4",          0x0000, 0x2000, 0x7e437c3e )
	ROM_LOAD( "tm5",          0x2000, 0x2000, 0xe8ca87b9 )

	ROM_REGION( 0x0240, REGION_PROMS )
	ROM_LOAD( "timeplt.b4",   0x0000, 0x0020, 0x34c91839 ) /* palette */
	ROM_LOAD( "timeplt.b5",   0x0020, 0x0020, 0x463b2b07 ) /* palette */
	ROM_LOAD( "timeplt.e9",   0x0040, 0x0100, 0x4bbb2150 ) /* sprite lookup table */
	ROM_LOAD( "timeplt.e12",  0x0140, 0x0100, 0xf7b7663e ) /* char lookup table */
ROM_END

ROM_START( spaceplt )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "sp1",          0x0000, 0x2000, 0xac8ca3ae )
	ROM_LOAD( "sp2",          0x2000, 0x2000, 0x1f0308ef )
	ROM_LOAD( "sp3",          0x4000, 0x2000, 0x90aeca50 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "tm7",          0x0000, 0x1000, 0xd66da813 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sp6",          0x0000, 0x2000, 0x76caa8af )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sp4",          0x0000, 0x2000, 0x3781ce7a )
	ROM_LOAD( "tm5",          0x2000, 0x2000, 0xe8ca87b9 )

	ROM_REGION( 0x0240, REGION_PROMS )
	ROM_LOAD( "timeplt.b4",   0x0000, 0x0020, 0x34c91839 ) /* palette */
	ROM_LOAD( "timeplt.b5",   0x0020, 0x0020, 0x463b2b07 ) /* palette */
	ROM_LOAD( "timeplt.e9",   0x0040, 0x0100, 0x4bbb2150 ) /* sprite lookup table */
	ROM_LOAD( "timeplt.e12",  0x0140, 0x0100, 0xf7b7663e ) /* char lookup table */
ROM_END

ROM_START( psurge )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "p1",           0x0000, 0x2000, 0x05f9ba12 )
	ROM_LOAD( "p2",           0x2000, 0x2000, 0x3ff41576 )
	ROM_LOAD( "p3",           0x4000, 0x2000, 0xe8fe120a )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "p6",           0x0000, 0x1000, 0xb52d01fa )
	ROM_LOAD( "p7",           0x1000, 0x1000, 0x9db5c0ce )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "p4",           0x0000, 0x2000, 0x26fd7f81 )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "p5",           0x0000, 0x2000, 0x6066ec8e )
	ROM_LOAD( "tm5",          0x2000, 0x2000, 0xe8ca87b9 )

	ROM_REGION( 0x0240, REGION_PROMS )
	ROM_LOAD( "timeplt.b4",   0x0000, 0x0020, 0x00000000 ) /* palette */
	ROM_LOAD( "timeplt.b5",   0x0020, 0x0020, 0x00000000 ) /* palette */
	ROM_LOAD( "timeplt.e9",   0x0040, 0x0100, 0x00000000 ) /* sprite lookup table */
	ROM_LOAD( "timeplt.e12",  0x0140, 0x0100, 0x00000000 ) /* char lookup table */
ROM_END



GAME( 1982, timeplt,  0,       timeplt, timeplt, timeplt, ROT270, "Konami", "Time Pilot" )
GAME( 1982, timepltc, timeplt, timeplt, timeplt, timeplt, ROT270, "Konami (Centuri license)", "Time Pilot (Centuri)" )
GAME( 1982, spaceplt, timeplt, timeplt, timeplt, timeplt, ROT270, "bootleg", "Space Pilot" )
GAME( 1988, psurge,   0,       timeplt, psurge,  psurge,  ROT90,  "<unknown>", "Power Surge" )
