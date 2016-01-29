#include "../vidhrdw/mnight.cpp"

/***************************************************************************

Mutant Night

driver by Leandro Dardini

TODO:
- must do palette marking, it is overflowing at the moment

***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"

WRITE_HANDLER( mnight_bgvideoram_w );
WRITE_HANDLER( mnight_fgvideoram_w );
WRITE_HANDLER( mnight_sprite_overdraw_w );
WRITE_HANDLER( mnight_background_enable_w );
int  mnight_vh_start(void);
void mnight_vh_stop(void);
void mnight_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

extern unsigned char    *mnight_scrolly_ram;
extern unsigned char    *mnight_scrollx_ram;
extern unsigned char    *mnight_bgenable_ram;
extern unsigned char    *mnight_spoverdraw_ram;
extern unsigned char    *mnight_spriteram;
extern unsigned char    *mnight_background_videoram;
extern unsigned char    *mnight_foreground_videoram;
extern size_t mnight_spriteram_size;
extern size_t mnight_backgroundram_size;
extern size_t mnight_foregroundram_size;

static int mnight_bank_latch = 255, main_cpu_num;

void mnight_init_machine(void)
{
	main_cpu_num = 0;
}

int mnight_interrupt(void)
{
	return 0x00d7;	/* RST 10h */
}

READ_HANDLER( mnight_bankselect_r )
{
	return mnight_bank_latch;
}

WRITE_HANDLER( mnight_bankselect_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1+main_cpu_num);
	int bankaddress;

	if ( data != mnight_bank_latch )
	{
		mnight_bank_latch = data;

		bankaddress = 0x10000 + ((data & 0x7) * 0x4000);
		cpu_setbank(1,&RAM[bankaddress]);	 /* Select 8 banks of 16k */
	}
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xf7ff, MRA_RAM },
	{ 0xf800, 0xf800, input_port_2_r },
	{ 0xf801, 0xf801, input_port_0_r },
	{ 0xf802, 0xf802, input_port_1_r },
	{ 0xf803, 0xf803, input_port_3_r },
	{ 0xf804, 0xf804, input_port_4_r },
	{ 0xfa00, 0xfa00, MRA_RAM },
	{ 0xfa01, 0xfa01, MRA_RAM },
	{ 0xfa02, 0xfa02, mnight_bankselect_r },
	{ 0xfa03, 0xfa03, MRA_RAM },
	{ 0xfa08, 0xfa09, MRA_RAM },
	{ 0xfa0a, 0xfa0b, MRA_RAM },
	{ 0xfa0c, 0xfa0c, MRA_RAM },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xd9ff, MWA_RAM },
	{ 0xda00, 0xdfff, MWA_RAM, &mnight_spriteram, &mnight_spriteram_size },
	{ 0xe000, 0xe7ff, mnight_bgvideoram_w, &mnight_background_videoram, &mnight_backgroundram_size }, // VFY
	{ 0xe800, 0xefff, mnight_fgvideoram_w, &mnight_foreground_videoram, &mnight_foregroundram_size }, //VFY
	{ 0xf000, 0xf5ff, paletteram_RRRRGGGGBBBBxxxx_swap_w, &paletteram },
	{ 0xf600, 0xf7ff, MWA_RAM },
	{ 0xfa00, 0xfa00, soundlatch_w },
	{ 0xfa01, 0xfa01, MWA_RAM },		   // unknown but used
	{ 0xfa02, 0xfa02, mnight_bankselect_w },
	{ 0xfa03, 0xfa03, mnight_sprite_overdraw_w, &mnight_spoverdraw_ram },
	{ 0xfa08, 0xfa09, MWA_RAM, &mnight_scrollx_ram },
	{ 0xfa0a, 0xfa0b, MWA_RAM, &mnight_scrolly_ram },
	{ 0xfa0c, 0xfa0c, mnight_background_enable_w, &mnight_bgenable_ram },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress snd_readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xe000, 0xe000, soundlatch_r },
	{ 0xefee, 0xefee, MRA_NOP },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress snd_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xeff5, 0xeff6, MWA_NOP },			   /* SAMPLE FREQUENCY ??? */
	{ 0xefee, 0xefee, MWA_NOP },			   /* CHIP COMMAND ?? */
	{ -1 }  /* end of table */
};

static struct IOWritePort snd_writeport[] =
{
	{ 0x0000, 0x0000, YM2203_control_port_0_w },
	{ 0x0001, 0x0001, YM2203_write_port_0_w },
	{ 0x0080, 0x0080, YM2203_control_port_1_w },
	{ 0x0081, 0x0081, YM2203_write_port_1_w },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( mnight )
	PORT_START /* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START /* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE )	/* keep pressed during boot to enter service mode */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START /* DSW0 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x02, "30k and every 50k" )
	PORT_DIPSETTING(    0x00, "50k and every 80k" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Normal" )
	PORT_DIPSETTING(    0x00, "Difficult" )
	PORT_DIPNAME( 0x08, 0x08, "Free Game" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off )  )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Cabinet) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x80, "2" )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x00, "5" )

	PORT_START /* DSW1 */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_4C ) )
INPUT_PORTS_END

INPUT_PORTS_START( arkarea )
	PORT_START /* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START /* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE )	/* keep pressed during boot to enter service mode */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START /* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x40, "50000 and every 50000" )
	PORT_DIPSETTING(    0x00, "100000 and every 100000" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0x00, "4" )

	PORT_START /* DSW1 */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	 /* 8*8 characters */
	1024,	 /* 1024 characters */
	4,		 /* 4 bits per pixel */
	{0,1,2,3}, /* the bitplanes are packed in one nibble */
	{0, 4, 16384*8+0, 16384*8+4, 8, 12, 16384*8+8, 16384*8+12},
	{16*0, 16*1, 16*2, 16*3, 16*4, 16*5, 16*6, 16*7},
	8*16
};

static struct GfxLayout spritelayout =
{
	16,16,	 /* 16*16 characters */
	1536,	 /* 1536 sprites */
	4,		 /* 4 bits per pixel */
	{0,1,2,3}, /* the bitplanes are packed in one nibble */
	{0,  4,  0x18000*8+0,  0x18000*8+4,  8, 12,  0x18000*8+8, 0x18000*8+12,
		16*8+0, 16*8+4, 16*8+0x18000*8+0, 16*8+0x18000*8+4, 16*8+8, 16*8+12, 16*8+0x18000*8+8, 16*8+0x18000*8+12},
	{16*0, 16*1, 16*2, 16*3, 16*4, 16*5, 16*6, 16*7,
		32*8+16*0, 32*8+16*1, 32*8+16*2, 32*8+16*3, 32*8+16*4, 32*8+16*5, 32*8+16*6, 32*8+16*7},
	8*64
};

static struct GfxLayout bigspritelayout =
{
	32,32,	 /* 32*32 characters */
	384,	 /* 384 sprites */
	4,		 /* 4 bits per pixel */
	{0,1,2,3}, /* the bitplanes are packed in one nibble */
	{0,  4,  0x18000*8+0,  0x18000*8+4,  8, 12,  0x18000*8+8, 0x18000*8+12,
		16*8+0, 16*8+4, 16*8+0x18000*8+0, 16*8+0x18000*8+4, 16*8+8, 16*8+12, 16*8+0x18000*8+8, 16*8+0x18000*8+12,
	 	64*8+0, 64*8+4, 64*8+0x18000*8+0, 64*8+0x18000*8+4, 64*8+8, 64*8+12, 64*8+0x18000*8+8, 64*8+0x18000*8+12,
		64*8+16*8+0, 64*8+16*8+4, 64*8+16*8+0x18000*8+0, 64*8+16*8+0x18000*8+4,
		64*8+16*8+8, 64*8+16*8+12, 64*8+16*8+0x18000*8+8, 64*8+16*8+0x18000*8+12 },
	{16*0, 16*1, 16*2, 16*3, 16*4, 16*5, 16*6, 16*7,
		32*8+16*0, 32*8+16*1, 32*8+16*2, 32*8+16*3, 32*8+16*4, 32*8+16*5, 32*8+16*6, 32*8+16*7,
		128*8+16*0, 128*8+16*1, 128*8+16*2, 128*8+16*3,
		128*8+16*4, 128*8+16*5, 128*8+16*6, 128*8+16*7,
		128*8+32*8+16*0, 128*8+32*8+16*1, 128*8+32*8+16*2, 128*8+32*8+16*3,
		128*8+32*8+16*4, 128*8+32*8+16*5, 128*8+32*8+16*6, 128*8+32*8+16*7 },
	8*64*4
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &spritelayout,     0*16, 16},
	{ REGION_GFX2, 0, &spritelayout,    16*16, 16},
	{ REGION_GFX2, 0, &bigspritelayout, 16*16, 16},
	{ REGION_GFX3, 0, &charlayout,      32*16, 16},
	{ -1} /* end of array */
};


static struct YM2203interface ym2203_interface =
{
	2,	 /* 2 chips */
	12000000/8, // lax 11/03/1999  (1250000 -> 1500000 ???)
	{ YM2203_VOL(25,25), YM2203_VOL(25,25)},
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};


static struct MachineDriver machine_driver_mnight =
{
	{
		{
			CPU_Z80,
			6000000,		/* 12000000/2 ??? */
			readmem,writemem,0,0,	/* very sensitive to these settings */
			mnight_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,		/* 12000000/3 ??? */
			snd_readmem,snd_writemem,
			0,snd_writeport,
			interrupt,2
		},
	},
	60, 10000,			/* frames per second, vblank duration */
	10,				/* single CPU, no need for interleaving */
	mnight_init_machine,

	32*8, 32*8, { 0*8, 32*8-1, 4*8, 28*8-1},
	gfxdecodeinfo,
	48*16,48*16,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	mnight_vh_start,
	mnight_vh_stop,
	mnight_vh_screenrefresh,

	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};


ROM_START( mnight )
	ROM_REGION( 0x30000, REGION_CPU1 )
	ROM_LOAD( "mn6-j19.bin",  0x00000, 0x8000, 0x56678d14 )
	ROM_LOAD( "mn5-j17.bin",  0x10000, 0x8000, 0x2a73f88e )
	ROM_LOAD( "mn4-j16.bin",  0x18000, 0x8000, 0xc5e42bb4 )
	ROM_LOAD( "mn3-j14.bin",  0x20000, 0x8000, 0xdf6a4f7a )
	ROM_LOAD( "mn2-j12.bin",  0x28000, 0x8000, 0x9c391d1b )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "mn1-j7.bin",   0x00000, 0x10000, 0xa0782a31 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mn11-b20.bin", 0x00000, 0x4000, 0x4d37e0f4 )   // background tiles
	ROM_CONTINUE(             0x18000, 0x4000 )
	ROM_CONTINUE(             0x04000, 0x4000 )
	ROM_CONTINUE(             0x1c000, 0x4000 )
	ROM_LOAD( "mn12-b22.bin", 0x08000, 0x4000, 0xb22cbbd3 )
	ROM_CONTINUE(             0x20000, 0x4000 )
	ROM_CONTINUE(             0x0c000, 0x4000 )
	ROM_CONTINUE(             0x24000, 0x4000 )
	ROM_LOAD( "mn13-b23.bin", 0x10000, 0x4000, 0x65714070 )
	ROM_CONTINUE(             0x28000, 0x4000 )
	ROM_CONTINUE(             0x14000, 0x4000 )
	ROM_CONTINUE(             0x2c000, 0x4000 )

	ROM_REGION( 0x30000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mn7-e11.bin",  0x00000, 0x4000, 0x4883059c )	  // sprites tiles
	ROM_CONTINUE(             0x18000, 0x4000 )
	ROM_CONTINUE(             0x04000, 0x4000 )
	ROM_CONTINUE(             0x1c000, 0x4000 )
	ROM_LOAD( "mn8-e12.bin",  0x08000, 0x4000, 0x2b91445 )
	ROM_CONTINUE(             0x20000, 0x4000 )
	ROM_CONTINUE(             0x0c000, 0x4000 )
	ROM_CONTINUE(             0x24000, 0x4000 )
	ROM_LOAD( "mn9-e14.bin",  0x10000, 0x4000, 0x9f08d160 )
	ROM_CONTINUE(             0x28000, 0x4000 )
	ROM_CONTINUE(             0x14000, 0x4000 )
	ROM_CONTINUE(             0x2c000, 0x4000 )

	ROM_REGION( 0x08000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mn10-b10.bin", 0x00000, 0x2000, 0x37b8221f )	// foreground tiles OK
	ROM_CONTINUE(             0x04000, 0x2000 )
	ROM_CONTINUE(             0x02000, 0x2000 )
	ROM_CONTINUE(             0x06000, 0x2000 )
ROM_END

ROM_START( arkarea )
	ROM_REGION( 0x30000, REGION_CPU1 )
	ROM_LOAD( "arkarea.008",  0x00000, 0x8000, 0x1ce1b5b9 )
	ROM_LOAD( "arkarea.009",  0x10000, 0x8000, 0xdb1c81d1 )
	ROM_LOAD( "arkarea.010",  0x18000, 0x8000, 0x5a460dae )
	ROM_LOAD( "arkarea.011",  0x20000, 0x8000, 0x63f022c9 )
	ROM_LOAD( "arkarea.012",  0x28000, 0x8000, 0x3c4c65d5 )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "arkarea.013",  0x00000, 0x8000, 0x2d409d58 )

	ROM_REGION( 0x30000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "arkarea.003",  0x00000, 0x4000, 0x6f45a308 )   // background tiles
	ROM_CONTINUE(             0x18000, 0x4000 )
	ROM_CONTINUE(             0x04000, 0x4000 )
	ROM_CONTINUE(             0x1c000, 0x4000 )
	ROM_LOAD( "arkarea.002",  0x08000, 0x4000, 0x051d3482 )
	ROM_CONTINUE(             0x20000, 0x4000 )
	ROM_CONTINUE(             0x0c000, 0x4000 )
	ROM_CONTINUE(             0x24000, 0x4000 )
	ROM_LOAD( "arkarea.001",  0x10000, 0x4000, 0x09d11ab7 )
	ROM_CONTINUE(             0x28000, 0x4000 )
	ROM_CONTINUE(             0x14000, 0x4000 )
	ROM_CONTINUE(             0x2c000, 0x4000 )

	ROM_REGION( 0x30000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "arkarea.007",  0x00000, 0x4000, 0xd5684a27 )   // sprites tiles
	ROM_CONTINUE(             0x18000, 0x4000 )
	ROM_CONTINUE(             0x04000, 0x4000 )
	ROM_CONTINUE(             0x1c000, 0x4000 )
	ROM_LOAD( "arkarea.006",  0x08000, 0x4000, 0x2c0567d6)
	ROM_CONTINUE(             0x20000, 0x4000 )
	ROM_CONTINUE(             0x0c000, 0x4000 )
	ROM_CONTINUE(             0x24000, 0x4000 )
	ROM_LOAD( "arkarea.005",  0x10000, 0x4000, 0x9886004d )
	ROM_CONTINUE(             0x28000, 0x4000 )
	ROM_CONTINUE(             0x14000, 0x4000 )
	ROM_CONTINUE(             0x2c000, 0x4000 )

	ROM_REGION( 0x08000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "arkarea.004",  0x00000, 0x2000, 0x69e36af2 ) // foreground tiles OK
	ROM_CONTINUE(             0x04000, 0x2000 )
	ROM_CONTINUE(             0x02000, 0x2000 )
	ROM_CONTINUE(             0x06000, 0x2000 )
ROM_END



GAME( 1987, mnight,  0, mnight, mnight,  0, ROT0, "UPL (Kawakus license)", "Mutant Night" )
GAME( 1988?,arkarea, 0, mnight, arkarea, 0, ROT0, "UPL", "Ark Area" )
