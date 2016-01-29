#include "../vidhrdw/nova2001.cpp"

/*******************************************************************************

     Nova 2001 - by UPL - 1983

     driver by Howie Cohen, Frank Palazzolo, Alex Pasadyn

     Memory Map:

     Address Range:     R/W:     Function:
     --------------------------------------------------------------------------
     0000 - 7fff        R        Program ROM (7000-7fff mirror of 6000-6fff)
     a000 - a3ff        R/W      Foreground Playfield character RAM
     a400 - a7ff        R/W      Foreground Playfield color modifier RAM
     a800 - abff        R/W      Scrolling Playfield character RAM
     ac00 - a7ff        R/W      Scrolling Playfield color modifier RAM
     b000 - b7ff        R/W      Sprite RAM
     bfff               W        flip screen
     c000               R/W      AY8910 #1 Data R/W
     c001               R/W      AY8910 #2 Data R/W
     c002               W        AY8910 #1 Control W
     c003               W        AY8910 #2 Control W
     c004               R        Interrupt acknowledge / Watchdog reset
     c006               R        Player 1 Controls
     c007               R        Player 2 Controls
     c00e               R        Coin Inputs, etc.
     e000 - e7ff        R/W      Work RAM

*******************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* From vidhrdw/nova2001.c */
extern unsigned char *nova2001_videoram,*nova2001_colorram;
extern size_t nova2001_videoram_size;

void nova2001_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( nova2001_scroll_x_w );
WRITE_HANDLER( nova2001_scroll_y_w );
WRITE_HANDLER( nova2001_flipscreen_w );
void nova2001_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xa000, 0xb7ff, MRA_RAM },
	{ 0xc000, 0xc000, AY8910_read_port_0_r },
	{ 0xc001, 0xc001, AY8910_read_port_1_r },
	{ 0xc004, 0xc004, watchdog_reset_r },
	{ 0xc006, 0xc006, input_port_0_r },
	{ 0xc007, 0xc007, input_port_1_r },
	{ 0xc00e, 0xc00e, input_port_2_r },
	{ 0xe000, 0xe7ff, MRA_RAM },
	{ -1 }	/* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xa000, 0xa3ff, MWA_RAM, &nova2001_videoram, &nova2001_videoram_size },
	{ 0xa400, 0xa7ff, MWA_RAM, &nova2001_colorram },
	{ 0xa800, 0xabff, videoram_w, &videoram, &videoram_size },
	{ 0xac00, 0xafff, colorram_w, &colorram },
	{ 0xb000, 0xb7ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xbfff, 0xbfff, nova2001_flipscreen_w },
	{ 0xc000, 0xc000, AY8910_write_port_0_w },
	{ 0xc001, 0xc001, AY8910_write_port_1_w },
	{ 0xc002, 0xc002, AY8910_control_port_0_w },
	{ 0xc003, 0xc003, AY8910_control_port_1_w },
	{ 0xe000, 0xe7ff, MWA_RAM },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( nova2001 )
    PORT_START
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

    PORT_START    /* player 2 controls */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

    PORT_START
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1 )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )

    PORT_START  /* dsw0 */
    PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
    PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
    PORT_DIPNAME( 0x02, 0x02, DEF_STR( Lives ) )
    PORT_DIPSETTING(    0x02, "3" )
    PORT_DIPSETTING(    0x00, "4" )
    PORT_DIPNAME( 0x04, 0x04, "1st Bonus Life" )
    PORT_DIPSETTING(    0x04, "20000" )
    PORT_DIPSETTING(    0x00, "30000" )
    PORT_DIPNAME( 0x18, 0x18, "Extra Bonus Life" )
    PORT_DIPSETTING(    0x18, "60000" )
    PORT_DIPSETTING(    0x10, "70000" )
    PORT_DIPSETTING(    0x08, "90000" )
    PORT_DIPSETTING(    0x00, "None" )
    PORT_DIPNAME( 0x60, 0x60, DEF_STR( Coinage ) )
    PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING(    0x00, DEF_STR( 2C_2C ) )
    PORT_DIPSETTING(    0x60, DEF_STR( 1C_1C ) )
    PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
    PORT_DIPNAME( 0x80, 0x80, DEF_STR( Free_Play ) )
    PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )

    PORT_START  /* dsw1 */
    PORT_DIPNAME( 0x03, 0x00, DEF_STR( Difficulty ) )
    PORT_DIPSETTING(    0x00, "Easy" )
    PORT_DIPSETTING(    0x03, "Medium" )
    PORT_DIPSETTING(    0x02, "Hard" )
    PORT_DIPSETTING(    0x01, "Hardest" )
    PORT_DIPNAME( 0x04, 0x00, DEF_STR( Demo_Sounds ) )
    PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "High Score Names" )
	PORT_DIPSETTING(    0x00, "3 Letters" )
	PORT_DIPSETTING(    0x08, "8 Letters" )
    PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	256,    /* 256 characters */
	4,       /* 4 bits per pixel */
	{0,1,2,3 }, /* the bitplanes are packed in one nibble */
	{0, 4, 8192*8+0, 8192*8+4, 8, 12, 8192*8+8, 8192*8+12},
	{16*0, 16*1, 16*2, 16*3, 16*4, 16*5, 16*6, 16*7},
	8*16
};

static struct GfxLayout spritelayout =
{
	16,16,    /* 16*16 characters */
	64,    /* 64 sprites */
	4,       /* 4 bits per pixel */
	{0,1,2,3}, /* the bitplanes are packed in one nibble */
	{0,  4,  8192*8+0,  8192*8+4,  8, 12,  8192*8+8, 8192*8+12,
			16*8+0, 16*8+4, 16*8+8192*8+0, 16*8+8192*8+4, 16*8+8, 16*8+12, 16*8+8192*8+8, 16*8+8192*8+12},
	{16*0, 16*1, 16*2, 16*3, 16*4, 16*5, 16*6, 16*7,
			32*8+16*0, 32*8+16*1, 32*8+16*2, 32*8+16*3, 32*8+16*4, 32*8+16*5, 32*8+16*6, 32*8+16*7},
	8*64
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,       0, 16 },
	{ REGION_GFX2, 0x0000, &charlayout,   16*16, 16 },
	{ REGION_GFX1, 0x1000, &spritelayout,     0, 16 },
	{ REGION_GFX2, 0x1000, &spritelayout,     0, 16 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	2,	/* 2 chips */
	6000000/3,	/* 2 MHz */
	{ 25, 25 },
	{ 0, input_port_3_r },
	{ 0, input_port_4_r },
	{ nova2001_scroll_x_w }, /* writes are connected to pf scroll */
	{ nova2001_scroll_y_w },
	{ 0 }
};

static struct MachineDriver machine_driver_nova2001 =
{
	{
		{
			CPU_Z80,
			3000000,	/* 3 MHz */
			readmem,writemem,0,0,
			interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	32*8, 32*8, { 0*8, 32*8-1, 4*8, 28*8-1 },

	gfxdecodeinfo,
	32,32*16,
	nova2001_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	nova2001_vh_screenrefresh,

	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};



ROM_START( nova2001 )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "1.6c",         0x0000, 0x2000, 0x368cffc0 )
	ROM_LOAD( "2.6d",         0x2000, 0x2000, 0xbc4e442b )
	ROM_LOAD( "3.6f",         0x4000, 0x2000, 0xb2849038 )
	ROM_LOAD( "4.6g",         0x6000, 0x1000, 0x6b5bb12d )
	ROM_RELOAD(               0x7000, 0x1000 )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5.12s",        0x0000, 0x2000, 0x54198941 )
	ROM_LOAD( "6.12p",        0x2000, 0x2000, 0xcbd90dca )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "7.12n",        0x0000, 0x2000, 0x9ebd8806 )
	ROM_LOAD( "8.12l",        0x2000, 0x2000, 0xd1b18389 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "nova2001.clr", 0x0000, 0x0020, 0xa2fac5cd )
ROM_END

ROM_START( nov2001u )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "nova2001.1",   0x0000, 0x2000, 0xb79461bd )
	ROM_LOAD( "nova2001.2",   0x2000, 0x2000, 0xfab87144 )
	ROM_LOAD( "3.6f",         0x4000, 0x2000, 0xb2849038 )
	ROM_LOAD( "4.6g",         0x6000, 0x1000, 0x6b5bb12d )
	ROM_RELOAD(               0x7000, 0x1000 )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nova2001.5",   0x0000, 0x2000, 0x8ea576e8 )
	ROM_LOAD( "nova2001.6",   0x2000, 0x2000, 0x0c61656c )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "7.12n",        0x0000, 0x2000, 0x9ebd8806 )
	ROM_LOAD( "8.12l",        0x2000, 0x2000, 0xd1b18389 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "nova2001.clr", 0x0000, 0x0020, 0xa2fac5cd )
ROM_END



GAME( 1983, nova2001, 0,        nova2001, nova2001, 0, ROT0, "UPL", "Nova 2001 (Japan)" )
GAME( 1983, nov2001u, nova2001, nova2001, nova2001, 0, ROT0, "UPL (Universal license)", "Nova 2001 (US)" )
