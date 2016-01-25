#include "../vidhrdw/dcon.cpp"

/***************************************************************************

	D-Con							(c) 1992 Success

	Success seems related to Seibu - this game runs on Seibu hardware.

	Emulation by Bryan McPhail, mish@tendril.co.uk

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"
#include "sndhrdw/seibu.h"

WRITE_HANDLER( dcon_background_w );
WRITE_HANDLER( dcon_foreground_w );
WRITE_HANDLER( dcon_midground_w );
WRITE_HANDLER( dcon_text_w );
WRITE_HANDLER( dcon_control_w );

int dcon_vh_start(void);
WRITE_HANDLER( dcon_control_w );
void dcon_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

extern unsigned char *dcon_back_data,*dcon_fore_data,*dcon_mid_data,*dcon_scroll_ram;

/***************************************************************************/

static WRITE_HANDLER( dcon_sound_w )
{
	seibu_soundlatch_w(offset,data&0xff); /* Convert 16 bit write to 8 bit */
}

static READ_HANDLER( dcon_control_r )
{
	switch (offset)
	{
		case 0: /* Dip Switches */
			return (readinputport(4) + (readinputport(3) << 8));

		case 2: /* Player 1 & Player 2 joysticks & fire buttons */
			return (readinputport(0) + (readinputport(1) << 8));

		case 4: /* Credits */
			return readinputport(2);
	}
	return 0xffff;
}

static READ_HANDLER( dcon_background_r ) { return READ_WORD(&dcon_back_data[offset]); }
static READ_HANDLER( dcon_foreground_r ) { return READ_WORD(&dcon_fore_data[offset]); }
static READ_HANDLER( dcon_midground_r ) { return READ_WORD(&dcon_mid_data[offset]); }

/******************************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x00000, 0x7ffff, MRA_ROM },
	{ 0x80000, 0x8bfff, MRA_BANK2 },
	{ 0x8c000, 0x8c7ff, dcon_background_r },
	{ 0x8c800, 0x8cfff, dcon_foreground_r },
	{ 0x8d000, 0x8d7ff, dcon_midground_r },
	{ 0x8e800, 0x8f7ff, paletteram_word_r },
	{ 0x8f800, 0x8ffff, MRA_BANK3 },
	{ 0xa0000, 0xa000f, MRA_NOP }, /* Unused sound cpu read */
	{ 0xe0000, 0xe0007, dcon_control_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x00000, 0x7ffff, MWA_ROM },
	{ 0x80000, 0x8bfff, MWA_BANK2 },
	{ 0x8c000, 0x8c7ff, dcon_background_w, &dcon_back_data },
	{ 0x8c800, 0x8cfff, dcon_foreground_w, &dcon_fore_data },
	{ 0x8d000, 0x8d7ff, dcon_midground_w, &dcon_mid_data },
	{ 0x8d800, 0x8e7ff, dcon_text_w, &videoram },
	{ 0x8e800, 0x8f7ff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },
	{ 0x8f800, 0x8ffff, MWA_BANK3, &spriteram },
	{ 0xa0000, 0xa000f, dcon_sound_w, &seibu_shared_sound_ram },
	{ 0xc001c, 0xc001d, dcon_control_w },
	{ 0xc0020, 0xc002b, MWA_BANK4, &dcon_scroll_ram },
	{ 0xc0000, 0xc00ff, MWA_NOP },
	{ -1 }	/* end of table */
};

/******************************************************************************/

SEIBU_SOUND_SYSTEM_YM3812_MEMORY_MAP(MRA_NOP); /* No coin port in this game */

/******************************************************************************/

INPUT_PORTS_START( dcon )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_UNKNOWN)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switch A */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
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

	PORT_START	/* Dip switch B */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
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

/******************************************************************************/

static struct GfxLayout dcon_charlayout =
{
	8,8,		/* 8*8 characters */
	4096,
	4,			/* 4 bits per pixel */
	{ 0,4,(0x10000*8)+0,0x10000*8+4,  },
	{ 3,2,1,0, 11,10,9,8 ,8,9,10,11,0,1,2,3, },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	128
};

static struct GfxLayout dcon_tilelayout =
{
	16,16,	/* 16*16 tiles */
	4096,		/* 2048*4 tiles */
	4,		/* 4 bits per pixel */
	{ 8,12, 0,4 },
	{
		3,2,1,0,19,18,17,16,
		512+3,512+2,512+1,512+0,
		512+11+8,512+10+8,512+9+8,512+8+8,
	},
	{
		0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
		8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32,
	},
	1024
};

static struct GfxLayout dcon_spritelayout =
{
	16,16,	/* 16*16 tiles */
	4096*4,		/* 2048*4 tiles */
	4,		/* 4 bits per pixel */
	{  8,12, 0,4 },
	{
		3,2,1,0,19,18,17,16,
		512+3,512+2,512+1,512+0,
		512+11+8,512+10+8,512+9+8,512+8+8,
	},
	{
		0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
		8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32,
	},
	1024
};

static struct GfxDecodeInfo dcon_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &dcon_charlayout,    1024+768, 16 },
	{ REGION_GFX2, 0, &dcon_tilelayout,    1024+0,   16 },
	{ REGION_GFX3, 0, &dcon_tilelayout,    1024+512, 16 },
	{ REGION_GFX4, 0, &dcon_tilelayout,    1024+256, 16 },
	{ REGION_GFX5, 0, &dcon_spritelayout,         0, 64 },
	{ -1 } /* end of array */
};

/******************************************************************************/

/* Parameters: YM3812 frequency, Oki frequency, Oki memory region */
SEIBU_SOUND_SYSTEM_YM3812_HARDWARE(4000000,8000,REGION_SOUND1);

static struct MachineDriver machine_driver_dcon =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			10000000,
			readmem,writemem,0,0,
			m68_level4_irq,1
		},
		{
			SEIBU_SOUND_SYSTEM_CPU(4000000)
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* CPU interleave  */
	seibu_sound_init_1,

	/* video hardware */
	40*8, 32*8, { 0*8, 40*8-1, 0*8, 28*8-1 },

	dcon_gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	dcon_vh_start,
	0,
	dcon_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		SEIBU_SOUND_SYSTEM_YM3812_INTERFACE
	}
};

/***************************************************************************/

ROM_START( dcon )
	ROM_REGION( 0x80000, REGION_CPU1 )
	ROM_LOAD_EVEN("p0-0",   0x000000, 0x20000, 0xa767ec15 )
	ROM_LOAD_ODD ("p0-1",   0x000000, 0x20000, 0xa7efa091 )
	ROM_LOAD_EVEN("p1-0",   0x040000, 0x20000, 0x3ec1ef7d )
	ROM_LOAD_ODD ("p1-1",   0x040000, 0x20000, 0x4b8de320 )

	ROM_REGION( 0x18000, REGION_CPU2 )	 /* 64k code for sound Z80 */
	ROM_LOAD( "fm", 0x000000, 0x08000, 0x50450faa )
	ROM_CONTINUE(   0x010000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fix0",  0x000000, 0x10000, 0xab30061f ) /* chars */
	ROM_LOAD( "fix1",  0x010000, 0x10000, 0xa0582115 )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bg1",   0x000000, 0x80000, 0xeac43283 ) /* tiles */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bg3",   0x000000, 0x80000, 0x1408a1e0 ) /* tiles */

	ROM_REGION( 0x080000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "bg2",   0x000000, 0x80000, 0x01864eb6 ) /* tiles */

	ROM_REGION( 0x200000, REGION_GFX5 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "obj0",  0x000000, 0x80000, 0xc3af37db ) /* sprites */
	ROM_LOAD( "obj1",  0x080000, 0x80000, 0xbe1f53ba )
	ROM_LOAD( "obj2",  0x100000, 0x80000, 0x24e0b51c )
	ROM_LOAD( "obj3",  0x180000, 0x80000, 0x5274f02d )

	ROM_REGION( 0x20000, REGION_SOUND1 )	 /* ADPCM samples */
	ROM_LOAD( "pcm", 0x000000, 0x20000, 0xd2133b85 )
ROM_END

/***************************************************************************/

static void init_dcon(void)
{
	install_seibu_sound_speedup(1);
}

/***************************************************************************/

GAMEX( 1992, dcon, 0, dcon, dcon, dcon, ROT0, "Success (Seibu hardware)", "D-Con", GAME_NO_COCKTAIL )
