#include "../vidhrdw/appoooh.cpp"

/***************************************************************************

Appoooh memory map (preliminary)
Similar to Bank Panic

driver by Tatsuyuki Satoh


0000-9fff ROM
a000-dfff BANKED ROM
e000-e7ff RAM
e800-efff RAM??

write:
f000-f01f Sprite RAM #1
f020-f3ff Video  RAM #1
f420-f7ff Color  RAM #1
f800-f81f Sprite RAM #2
f820-fbff Video  RAM #2
fc20-ffff Color  RAM #2

I/O

read:
00  IN0
01  IN1
03  DSW
04  IN2

write:
00  SN76496 #1
01  SN76496 #2
02  SN76496 #3
03  MSM5205 address write
04  bit 0   = NMI enable
    bit 1   = flipscreen
    bit 2-3 = ?
    bit 4-5 = priority
    bit 6   = bank rom select
    bit 7   = ?
05  horizontal scroll ??

Credits:
- Tatsuyuki Satoh: MAME driver

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char *appoooh_videoram2;
extern unsigned char *appoooh_colorram2;
extern unsigned char *appoooh_spriteram2;

void appoooh_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( appoooh_scroll_w );
WRITE_HANDLER( appoooh_videoram2_w );
WRITE_HANDLER( appoooh_colorram2_w );
WRITE_HANDLER( appoooh_out_w );
int appoooh_vh_start(void);
void appoooh_vh_stop(void);
void appoooh_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

static unsigned char *adpcmptr = 0;
static int appoooh_adpcm_data;


static void appoooh_adpcm_int (int num)
{
	if( adpcmptr )
	{
		if( appoooh_adpcm_data==-1)
		{
			appoooh_adpcm_data = *adpcmptr++;
			MSM5205_data_w(0,appoooh_adpcm_data >> 4);
			if(appoooh_adpcm_data==0x70)
			{
				adpcmptr = 0;
				MSM5205_reset_w(0,1);
			}
		}else{
			MSM5205_data_w(0,appoooh_adpcm_data & 0x0f );
			appoooh_adpcm_data =-1;
		}
	}
}
/* adpcm address write */
static WRITE_HANDLER( appoooh_adpcm_w )
{
	unsigned char *RAM = memory_region(REGION_SOUND1);
	adpcmptr  = &RAM[data*256];
	MSM5205_reset_w(0,0);
	appoooh_adpcm_data=-1;
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x9fff, MRA_ROM },
	{ 0xa000, 0xdfff, MRA_BANK1 },
	{ 0xe000, 0xe7ff, MRA_RAM },
	{ 0xe800, 0xefff, MRA_RAM }, /* RAM ? */
	{ 0xf000, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xdfff, MWA_ROM },
	{ 0xe000, 0xe7ff, MWA_RAM },
	{ 0xe800, 0xefff, MWA_RAM }, /* RAM ? */
	{ 0xf000, 0xf01f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xf020, 0xf3ff, videoram_w, &videoram, &videoram_size },
	{ 0xf420, 0xf7ff, colorram_w, &colorram },
	{ 0xf800, 0xf81f, MWA_RAM, &appoooh_spriteram2 },
	{ 0xf820, 0xfbff, appoooh_videoram2_w, &appoooh_videoram2 },
	{ 0xfc20, 0xffff, appoooh_colorram2_w, &appoooh_colorram2 },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r },	/* IN0 */
	{ 0x01, 0x01, input_port_1_r },	/* IN1 */
	{ 0x03, 0x03, input_port_3_r },	/* DSW */
	{ 0x04, 0x04, input_port_2_r },	/* IN2 */
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, SN76496_0_w },
	{ 0x01, 0x01, SN76496_1_w },
	{ 0x02, 0x02, SN76496_2_w },
	{ 0x03, 0x03, appoooh_adpcm_w },
	{ 0x04, 0x04, appoooh_out_w  },
	{ 0x05, 0x05, appoooh_scroll_w }, /* unknown */
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( appoooh )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
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
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x80, "Hard" )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	2048,	/* 2048 characters */
	3,	/* 3 bits per pixel */
	{ 2*2048*8*8, 1*2048*8*8, 0*2048*8*8 },	/* the bitplanes are separated */
	{ 7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 8*8 characters */
	512,	/* 512 characters */
	3,	/* 3 bits per pixel */
	{ 2*2048*8*8, 1*2048*8*8, 0*2048*8*8 },	/* the bitplanes are separated */
	{ 7, 6, 5, 4, 3, 2, 1, 0 ,
	  8*8+7,8*8+6,8*8+5,8*8+4,8*8+3,8*8+2,8*8+1,8*8+0},
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8	/* every char takes 8 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,        0, 32 },
	{ REGION_GFX2, 0, &charlayout,     32*8, 32 },
	{ REGION_GFX1, 0, &spritelayout,      0, 32 },
	{ REGION_GFX2, 0, &spritelayout,   32*8, 32 },
	{ -1 } /* end of array */
};



static struct SN76496interface sn76496_interface =
{
	3,	/* 3 chips */
	{ 18432000/6, 18432000/6, 18432000/6 },	/* ??? */
	{ 30, 30, 30 }
};

static struct MSM5205interface msm5205_interface =
{
	1,					/* 1 chip             */
	384000,				/* 384KHz             */
	{ appoooh_adpcm_int },/* interrupt function */
	{ MSM5205_S64_4B },	/* 6KHz               */
	{ 50 }
};



static struct MachineDriver machine_driver_appoooh =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* ??? the main xtal is 18.432 MHz */
			readmem,writemem,readport,writeport,
			nmi_interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 29*8-1 },
	gfxdecodeinfo,
	32, 32*8+32*8, /* total colors,color_table_len */
	appoooh_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	appoooh_vh_start,
	appoooh_vh_stop,
	appoooh_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SN76496,
			&sn76496_interface
		},
		{
			SOUND_MSM5205,
			&msm5205_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( appoooh )
	ROM_REGION( 0x14000, REGION_CPU1 )	/* 64k for code + 16k bank */
	ROM_LOAD( "epr-5906.bin", 0x00000, 0x2000, 0xfffae7fe )
	ROM_LOAD( "epr-5907.bin", 0x02000, 0x2000, 0x57696cd6 )
	ROM_LOAD( "epr-5908.bin", 0x04000, 0x2000, 0x4537cddc )
	ROM_LOAD( "epr-5909.bin", 0x06000, 0x2000, 0xcf82718d )
	ROM_LOAD( "epr-5910.bin", 0x08000, 0x2000, 0x312636da )
	ROM_LOAD( "epr-5911.bin", 0x0a000, 0x2000, 0x0bc2acaa ) /* bank0      */
	ROM_LOAD( "epr-5913.bin", 0x0c000, 0x2000, 0xf5a0e6a7 ) /* a000-dfff  */
	ROM_LOAD( "epr-5912.bin", 0x10000, 0x2000, 0x3c3915ab ) /* bank1     */
	ROM_LOAD( "epr-5914.bin", 0x12000, 0x2000, 0x58792d4a ) /* a000-dfff */

	ROM_REGION( 0x0c000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "epr-5895.bin", 0x00000, 0x4000, 0x4b0d4294 )	/* playfield #1 chars */
	ROM_LOAD( "epr-5896.bin", 0x04000, 0x4000, 0x7bc84d75 )
	ROM_LOAD( "epr-5897.bin", 0x08000, 0x4000, 0x745f3ffa )

	ROM_REGION( 0x0c000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "epr-5898.bin", 0x00000, 0x4000, 0xcf01644d )	/* playfield #2 chars */
	ROM_LOAD( "epr-5899.bin", 0x04000, 0x4000, 0x885ad636 )
	ROM_LOAD( "epr-5900.bin", 0x08000, 0x4000, 0xa8ed13f3 )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "pr5921.prm",   0x0000, 0x020, 0xf2437229 ) 	/* palette */
	ROM_LOAD( "pr5922.prm",   0x0020, 0x100, 0x85c542bf ) 	/* charset #1 lookup table */
	ROM_LOAD( "pr5923.prm",   0x0120, 0x100, 0x16acbd53 ) 	/* charset #2 lookup table */

	ROM_REGION( 0xa000, REGION_SOUND1 )	/* adpcm voice data */
	ROM_LOAD( "epr-5901.bin", 0x0000, 0x2000, 0x170a10a4 )
	ROM_LOAD( "epr-5902.bin", 0x2000, 0x2000, 0xf6981640 )
	ROM_LOAD( "epr-5903.bin", 0x4000, 0x2000, 0x0439df50 )
	ROM_LOAD( "epr-5904.bin", 0x6000, 0x2000, 0x9988f2ae )
	ROM_LOAD( "epr-5905.bin", 0x8000, 0x2000, 0xfb5cd70e )
ROM_END



GAME( 1984, appoooh, 0, appoooh, appoooh, 0, ROT0, "Sega", "Appoooh" )
