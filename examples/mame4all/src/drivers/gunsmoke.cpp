#include "../vidhrdw/gunsmoke.cpp"

/***************************************************************************

  GUNSMOKE
  ========

  Driver provided by Paul Leaman

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



READ_HANDLER( gunsmoke_bankedrom_r );
extern void gunsmoke_init_machine(void);

extern unsigned char *gunsmoke_bg_scrollx;
extern unsigned char *gunsmoke_bg_scrolly;

WRITE_HANDLER( gunsmoke_c804_w );	/* in vidhrdw/c1943.c */
WRITE_HANDLER( gunsmoke_d806_w );	/* in vidhrdw/c1943.c */
void gunsmoke_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void gunsmoke_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
int gunsmoke_vh_start(void);
void gunsmoke_vh_stop(void);



static READ_HANDLER( gunsmoke_unknown_r )
{
    static int gunsmoke_fixed_data[]={ 0xff, 0x00, 0x00 };
    /*
    The routine at 0x0e69 tries to read data starting at 0xc4c9.
    If this value is zero, it interprets the next two bytes as a
    jump address.

    This was resulting in a reboot which happens at the end of level 3
    if you go too far to the right of the screen when fighting the level boss.

    A non-zero for the first byte seems to be harmless  (although it may not be
    the correct behaviour).

    This could be some devious protection or it could be a bug in the
    arcade game.  It's hard to tell without pulling the code apart.
    */
    return gunsmoke_fixed_data[offset];
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xc000, input_port_0_r },
	{ 0xc001, 0xc001, input_port_1_r },
	{ 0xc002, 0xc002, input_port_2_r },
	{ 0xc003, 0xc003, input_port_3_r },
	{ 0xc004, 0xc004, input_port_4_r },
    { 0xc4c9, 0xc4cb, gunsmoke_unknown_r },
    { 0xd000, 0xd3ff, videoram_r },
    { 0xd400, 0xd7ff, colorram_r },
	{ 0xe000, 0xffff, MRA_RAM }, /* Work + sprite RAM */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc800, 0xc800, soundlatch_w },
	{ 0xc804, 0xc804, gunsmoke_c804_w },	/* ROM bank switch, screen flip */
	{ 0xc806, 0xc806, MWA_NOP }, /* Watchdog ?? */
	{ 0xd000, 0xd3ff, videoram_w, &videoram, &videoram_size },
	{ 0xd400, 0xd7ff, colorram_w, &colorram },
	{ 0xd800, 0xd801, MWA_RAM, &gunsmoke_bg_scrolly },
	{ 0xd802, 0xd802, MWA_RAM, &gunsmoke_bg_scrollx },
	{ 0xd806, 0xd806, gunsmoke_d806_w },	/* sprites and bg enable */
	{ 0xe000, 0xefff, MWA_RAM },
	{ 0xf000, 0xffff, MWA_RAM, &spriteram, &spriteram_size },
	{ -1 }	/* end of table */
};



static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc800, 0xc800, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xdfff, MWA_RAM },
	{ 0xe000, 0xe000, YM2203_control_port_0_w },
	{ 0xe001, 0xe001, YM2203_write_port_0_w },
	{ 0xe002, 0xe002, YM2203_control_port_1_w },
	{ 0xe003, 0xe003, YM2203_write_port_1_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( gunsmoke )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably unused */

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably unused */

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x03, "30k, 100k & every 100k")
	PORT_DIPSETTING(    0x02, "30k, 80k & every 80k" )
	PORT_DIPSETTING(    0x01, "30k & 100K only")
	PORT_DIPSETTING(    0x00, "30k, 100k & every 150k" )
	PORT_DIPNAME( 0x04, 0x04, "Demonstration" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ))
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ))
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Difficult" )
	PORT_DIPSETTING(    0x00, "Very Difficult" )
	PORT_DIPNAME( 0x40, 0x40, "Freeze" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ))
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ))
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ))
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ))
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ))
INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	2,	/* 2 bits per pixel */
	{ 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every char takes 16 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	2048,	/* 2048 sprites */
	4,      /* 4 bits per pixel */
	{ 2048*64*8+4, 2048*64*8+0, 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			32*8+0, 32*8+1, 32*8+2, 32*8+3, 33*8+0, 33*8+1, 33*8+2, 33*8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout tilelayout =
{
	32,32,  /* 32*32 tiles */
	512,    /* 512 tiles */
	4,      /* 4 bits per pixel */
	{ 512*256*8+4, 512*256*8+0, 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			64*8+0, 64*8+1, 64*8+2, 64*8+3, 65*8+0, 65*8+1, 65*8+2, 65*8+3,
			128*8+0, 128*8+1, 128*8+2, 128*8+3, 129*8+0, 129*8+1, 129*8+2, 129*8+3,
			192*8+0, 192*8+1, 192*8+2, 192*8+3, 193*8+0, 193*8+1, 193*8+2, 193*8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16,
			16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16,
			24*16, 25*16, 26*16, 27*16, 28*16, 29*16, 30*16, 31*16 },
	256*8	/* every tile takes 256 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,            0, 32 },
	{ REGION_GFX2, 0, &tilelayout,         32*4, 16 }, /* Tiles */
	{ REGION_GFX3, 0, &spritelayout, 32*4+16*16, 16 }, /* Sprites */
	{ -1 } /* end of array */
};



static struct YM2203interface ym2203_interface =
{
	2,			/* 2 chips */
	1500000,	/* 1.5 MHz (?) */
	{ YM2203_VOL(14,22), YM2203_VOL(14,22) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_gunsmoke =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,        /* 4 Mhz (?) */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,	/* 3 Mhz (?) */
			sound_readmem,sound_writemem,0,0,
			interrupt,4
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256,32*4+16*16+16*16,
	gunsmoke_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	gunsmoke_vh_start,
	gunsmoke_vh_stop,
	gunsmoke_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( gunsmoke )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 2*64k for code */
	ROM_LOAD( "09n_gs03.bin", 0x00000, 0x8000, 0x40a06cef ) /* Code 0000-7fff */
	ROM_LOAD( "10n_gs04.bin", 0x10000, 0x8000, 0x8d4b423f ) /* Paged code */
	ROM_LOAD( "12n_gs05.bin", 0x18000, 0x8000, 0x2b5667fb ) /* Paged code */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "14h_gs02.bin", 0x00000, 0x8000, 0xcd7a2c38 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11f_gs01.bin", 0x00000, 0x4000, 0xb61ece9b ) /* Characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "06c_gs13.bin", 0x00000, 0x8000, 0xf6769fc5 ) /* 32x32 tiles planes 2-3 */
	ROM_LOAD( "05c_gs12.bin", 0x08000, 0x8000, 0xd997b78c )
	ROM_LOAD( "04c_gs11.bin", 0x10000, 0x8000, 0x125ba58e )
	ROM_LOAD( "02c_gs10.bin", 0x18000, 0x8000, 0xf469c13c )
	ROM_LOAD( "06a_gs09.bin", 0x20000, 0x8000, 0x539f182d ) /* 32x32 tiles planes 0-1 */
	ROM_LOAD( "05a_gs08.bin", 0x28000, 0x8000, 0xe87e526d )
	ROM_LOAD( "04a_gs07.bin", 0x30000, 0x8000, 0x4382c0d2 )
	ROM_LOAD( "02a_gs06.bin", 0x38000, 0x8000, 0x4cafe7a6 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "06n_gs22.bin", 0x00000, 0x8000, 0xdc9c508c ) /* Sprites planes 2-3 */
	ROM_LOAD( "04n_gs21.bin", 0x08000, 0x8000, 0x68883749 ) /* Sprites planes 2-3 */
	ROM_LOAD( "03n_gs20.bin", 0x10000, 0x8000, 0x0be932ed ) /* Sprites planes 2-3 */
	ROM_LOAD( "01n_gs19.bin", 0x18000, 0x8000, 0x63072f93 ) /* Sprites planes 2-3 */
	ROM_LOAD( "06l_gs18.bin", 0x20000, 0x8000, 0xf69a3c7c ) /* Sprites planes 0-1 */
	ROM_LOAD( "04l_gs17.bin", 0x28000, 0x8000, 0x4e98562a ) /* Sprites planes 0-1 */
	ROM_LOAD( "03l_gs16.bin", 0x30000, 0x8000, 0x0d99c3b3 ) /* Sprites planes 0-1 */
	ROM_LOAD( "01l_gs15.bin", 0x38000, 0x8000, 0x7f14270e ) /* Sprites planes 0-1 */

	ROM_REGION( 0x8000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "11c_gs14.bin", 0x00000, 0x8000, 0x0af4f7eb )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "03b_g-01.bin", 0x0000, 0x0100, 0x02f55589 )	/* red component */
	ROM_LOAD( "04b_g-02.bin", 0x0100, 0x0100, 0xe1e36dd9 )	/* green component */
	ROM_LOAD( "05b_g-03.bin", 0x0200, 0x0100, 0x989399c0 )	/* blue component */
	ROM_LOAD( "09d_g-04.bin", 0x0300, 0x0100, 0x906612b5 )	/* char lookup table */
	ROM_LOAD( "14a_g-06.bin", 0x0400, 0x0100, 0x4a9da18b )	/* tile lookup table */
	ROM_LOAD( "15a_g-07.bin", 0x0500, 0x0100, 0xcb9394fc )	/* tile palette bank */
	ROM_LOAD( "09f_g-09.bin", 0x0600, 0x0100, 0x3cee181e )	/* sprite lookup table */
	ROM_LOAD( "08f_g-08.bin", 0x0700, 0x0100, 0xef91cdd2 )	/* sprite palette bank */
ROM_END

ROM_START( gunsmrom )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 2*64k for code */
	ROM_LOAD( "9n_gs03.bin",  0x00000, 0x8000, 0x592f211b ) /* Code 0000-7fff */
	ROM_LOAD( "10n_gs04.bin", 0x10000, 0x8000, 0x8d4b423f ) /* Paged code */
	ROM_LOAD( "12n_gs05.bin", 0x18000, 0x8000, 0x2b5667fb ) /* Paged code */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "14h_gs02.bin", 0x00000, 0x8000, 0xcd7a2c38 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11f_gs01.bin", 0x00000, 0x4000, 0xb61ece9b ) /* Characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "06c_gs13.bin", 0x00000, 0x8000, 0xf6769fc5 ) /* 32x32 tiles planes 2-3 */
	ROM_LOAD( "05c_gs12.bin", 0x08000, 0x8000, 0xd997b78c )
	ROM_LOAD( "04c_gs11.bin", 0x10000, 0x8000, 0x125ba58e )
	ROM_LOAD( "02c_gs10.bin", 0x18000, 0x8000, 0xf469c13c )
	ROM_LOAD( "06a_gs09.bin", 0x20000, 0x8000, 0x539f182d ) /* 32x32 tiles planes 0-1 */
	ROM_LOAD( "05a_gs08.bin", 0x28000, 0x8000, 0xe87e526d )
	ROM_LOAD( "04a_gs07.bin", 0x30000, 0x8000, 0x4382c0d2 )
	ROM_LOAD( "02a_gs06.bin", 0x38000, 0x8000, 0x4cafe7a6 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "06n_gs22.bin", 0x00000, 0x8000, 0xdc9c508c ) /* Sprites planes 2-3 */
	ROM_LOAD( "04n_gs21.bin", 0x08000, 0x8000, 0x68883749 ) /* Sprites planes 2-3 */
	ROM_LOAD( "03n_gs20.bin", 0x10000, 0x8000, 0x0be932ed ) /* Sprites planes 2-3 */
	ROM_LOAD( "01n_gs19.bin", 0x18000, 0x8000, 0x63072f93 ) /* Sprites planes 2-3 */
	ROM_LOAD( "06l_gs18.bin", 0x20000, 0x8000, 0xf69a3c7c ) /* Sprites planes 0-1 */
	ROM_LOAD( "04l_gs17.bin", 0x28000, 0x8000, 0x4e98562a ) /* Sprites planes 0-1 */
	ROM_LOAD( "03l_gs16.bin", 0x30000, 0x8000, 0x0d99c3b3 ) /* Sprites planes 0-1 */
	ROM_LOAD( "01l_gs15.bin", 0x38000, 0x8000, 0x7f14270e ) /* Sprites planes 0-1 */

	ROM_REGION( 0x8000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "11c_gs14.bin", 0x00000, 0x8000, 0x0af4f7eb )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "03b_g-01.bin", 0x0000, 0x0100, 0x02f55589 )	/* red component */
	ROM_LOAD( "04b_g-02.bin", 0x0100, 0x0100, 0xe1e36dd9 )	/* green component */
	ROM_LOAD( "05b_g-03.bin", 0x0200, 0x0100, 0x989399c0 )	/* blue component */
	ROM_LOAD( "09d_g-04.bin", 0x0300, 0x0100, 0x906612b5 )	/* char lookup table */
	ROM_LOAD( "14a_g-06.bin", 0x0400, 0x0100, 0x4a9da18b )	/* tile lookup table */
	ROM_LOAD( "15a_g-07.bin", 0x0500, 0x0100, 0xcb9394fc )	/* tile palette bank */
	ROM_LOAD( "09f_g-09.bin", 0x0600, 0x0100, 0x3cee181e )	/* sprite lookup table */
	ROM_LOAD( "08f_g-08.bin", 0x0700, 0x0100, 0xef91cdd2 )	/* sprite palette bank */
ROM_END

ROM_START( gunsmokj )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 2*64k for code */
	ROM_LOAD( "gs03_9n.rom",  0x00000, 0x8000, 0xb56b5df6 ) /* Code 0000-7fff */
	ROM_LOAD( "10n_gs04.bin", 0x10000, 0x8000, 0x8d4b423f ) /* Paged code */
	ROM_LOAD( "12n_gs05.bin", 0x18000, 0x8000, 0x2b5667fb ) /* Paged code */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "14h_gs02.bin", 0x00000, 0x8000, 0xcd7a2c38 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11f_gs01.bin", 0x00000, 0x4000, 0xb61ece9b ) /* Characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "06c_gs13.bin", 0x00000, 0x8000, 0xf6769fc5 ) /* 32x32 tiles planes 2-3 */
	ROM_LOAD( "05c_gs12.bin", 0x08000, 0x8000, 0xd997b78c )
	ROM_LOAD( "04c_gs11.bin", 0x10000, 0x8000, 0x125ba58e )
	ROM_LOAD( "02c_gs10.bin", 0x18000, 0x8000, 0xf469c13c )
	ROM_LOAD( "06a_gs09.bin", 0x20000, 0x8000, 0x539f182d ) /* 32x32 tiles planes 0-1 */
	ROM_LOAD( "05a_gs08.bin", 0x28000, 0x8000, 0xe87e526d )
	ROM_LOAD( "04a_gs07.bin", 0x30000, 0x8000, 0x4382c0d2 )
	ROM_LOAD( "02a_gs06.bin", 0x38000, 0x8000, 0x4cafe7a6 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "06n_gs22.bin", 0x00000, 0x8000, 0xdc9c508c ) /* Sprites planes 2-3 */
	ROM_LOAD( "04n_gs21.bin", 0x08000, 0x8000, 0x68883749 ) /* Sprites planes 2-3 */
	ROM_LOAD( "03n_gs20.bin", 0x10000, 0x8000, 0x0be932ed ) /* Sprites planes 2-3 */
	ROM_LOAD( "01n_gs19.bin", 0x18000, 0x8000, 0x63072f93 ) /* Sprites planes 2-3 */
	ROM_LOAD( "06l_gs18.bin", 0x20000, 0x8000, 0xf69a3c7c ) /* Sprites planes 0-1 */
	ROM_LOAD( "04l_gs17.bin", 0x28000, 0x8000, 0x4e98562a ) /* Sprites planes 0-1 */
	ROM_LOAD( "03l_gs16.bin", 0x30000, 0x8000, 0x0d99c3b3 ) /* Sprites planes 0-1 */
	ROM_LOAD( "01l_gs15.bin", 0x38000, 0x8000, 0x7f14270e ) /* Sprites planes 0-1 */

	ROM_REGION( 0x8000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "11c_gs14.bin", 0x00000, 0x8000, 0x0af4f7eb )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "03b_g-01.bin", 0x0000, 0x0100, 0x02f55589 )	/* red component */
	ROM_LOAD( "04b_g-02.bin", 0x0100, 0x0100, 0xe1e36dd9 )	/* green component */
	ROM_LOAD( "05b_g-03.bin", 0x0200, 0x0100, 0x989399c0 )	/* blue component */
	ROM_LOAD( "09d_g-04.bin", 0x0300, 0x0100, 0x906612b5 )	/* char lookup table */
	ROM_LOAD( "14a_g-06.bin", 0x0400, 0x0100, 0x4a9da18b )	/* tile lookup table */
	ROM_LOAD( "15a_g-07.bin", 0x0500, 0x0100, 0xcb9394fc )	/* tile palette bank */
	ROM_LOAD( "09f_g-09.bin", 0x0600, 0x0100, 0x3cee181e )	/* sprite lookup table */
	ROM_LOAD( "08f_g-08.bin", 0x0700, 0x0100, 0xef91cdd2 )	/* sprite palette bank */
ROM_END

ROM_START( gunsmoka )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 2*64k for code */
	ROM_LOAD( "gs03.9n",      0x00000, 0x8000, 0x51dc3f76 ) /* Code 0000-7fff */
	ROM_LOAD( "gs04.10n",     0x10000, 0x8000, 0x5ecf31b8 ) /* Paged code */
	ROM_LOAD( "gs05.12n",     0x18000, 0x8000, 0x1c9aca13 ) /* Paged code */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "14h_gs02.bin", 0x00000, 0x8000, 0xcd7a2c38 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11f_gs01.bin", 0x00000, 0x4000, 0xb61ece9b ) /* Characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "06c_gs13.bin", 0x00000, 0x8000, 0xf6769fc5 ) /* 32x32 tiles planes 2-3 */
	ROM_LOAD( "05c_gs12.bin", 0x08000, 0x8000, 0xd997b78c )
	ROM_LOAD( "04c_gs11.bin", 0x10000, 0x8000, 0x125ba58e )
	ROM_LOAD( "02c_gs10.bin", 0x18000, 0x8000, 0xf469c13c )
	ROM_LOAD( "06a_gs09.bin", 0x20000, 0x8000, 0x539f182d ) /* 32x32 tiles planes 0-1 */
	ROM_LOAD( "05a_gs08.bin", 0x28000, 0x8000, 0xe87e526d )
	ROM_LOAD( "04a_gs07.bin", 0x30000, 0x8000, 0x4382c0d2 )
	ROM_LOAD( "02a_gs06.bin", 0x38000, 0x8000, 0x4cafe7a6 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "06n_gs22.bin", 0x00000, 0x8000, 0xdc9c508c ) /* Sprites planes 2-3 */
	ROM_LOAD( "04n_gs21.bin", 0x08000, 0x8000, 0x68883749 ) /* Sprites planes 2-3 */
	ROM_LOAD( "03n_gs20.bin", 0x10000, 0x8000, 0x0be932ed ) /* Sprites planes 2-3 */
	ROM_LOAD( "01n_gs19.bin", 0x18000, 0x8000, 0x63072f93 ) /* Sprites planes 2-3 */
	ROM_LOAD( "06l_gs18.bin", 0x20000, 0x8000, 0xf69a3c7c ) /* Sprites planes 0-1 */
	ROM_LOAD( "04l_gs17.bin", 0x28000, 0x8000, 0x4e98562a ) /* Sprites planes 0-1 */
	ROM_LOAD( "03l_gs16.bin", 0x30000, 0x8000, 0x0d99c3b3 ) /* Sprites planes 0-1 */
	ROM_LOAD( "01l_gs15.bin", 0x38000, 0x8000, 0x7f14270e ) /* Sprites planes 0-1 */

	ROM_REGION( 0x8000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "11c_gs14.bin", 0x00000, 0x8000, 0x0af4f7eb )

	ROM_REGION( 0x0800, REGION_PROMS )
	ROM_LOAD( "03b_g-01.bin", 0x0000, 0x0100, 0x02f55589 )	/* red component */
	ROM_LOAD( "04b_g-02.bin", 0x0100, 0x0100, 0xe1e36dd9 )	/* green component */
	ROM_LOAD( "05b_g-03.bin", 0x0200, 0x0100, 0x989399c0 )	/* blue component */
	ROM_LOAD( "09d_g-04.bin", 0x0300, 0x0100, 0x906612b5 )	/* char lookup table */
	ROM_LOAD( "14a_g-06.bin", 0x0400, 0x0100, 0x4a9da18b )	/* tile lookup table */
	ROM_LOAD( "15a_g-07.bin", 0x0500, 0x0100, 0xcb9394fc )	/* tile palette bank */
	ROM_LOAD( "09f_g-09.bin", 0x0600, 0x0100, 0x3cee181e )	/* sprite lookup table */
	ROM_LOAD( "08f_g-08.bin", 0x0700, 0x0100, 0xef91cdd2 )	/* sprite palette bank */
ROM_END



/*
  All the sets are almost identical apart from gunsmoka which is quite
  different: the levels are in a different order, and the "Demonstration" dip
  switch has no effect.
 */
GAMEX( 1985, gunsmoke, 0,        gunsmoke, gunsmoke, 0, ROT270, "Capcom", "Gun.Smoke (World)", GAME_NO_COCKTAIL )
GAMEX( 1985, gunsmrom, gunsmoke, gunsmoke, gunsmoke, 0, ROT270, "Capcom (Romstar license)", "Gun.Smoke (US set 1)", GAME_NO_COCKTAIL )
GAMEX( 1986, gunsmoka, gunsmoke, gunsmoke, gunsmoke, 0, ROT270, "Capcom", "Gun.Smoke (US set 2)", GAME_NO_COCKTAIL )
GAMEX( 1985, gunsmokj, gunsmoke, gunsmoke, gunsmoke, 0, ROT270, "Capcom", "Gun.Smoke (Japan)", GAME_NO_COCKTAIL )
