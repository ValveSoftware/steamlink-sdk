#include "../vidhrdw/gaelco.cpp"

/***************************************************************************

	Gaelco game hardware from 1991-1996

	Driver by Manuel Abadia <manu@teleline.es>

	Supported games:

		* Big Karnak
		* Biomechanical Toy
		* Maniac Square

	Known games running on this hardware:

		* Squash
		* Thunder Hoop
		* World Rally
		* Strike Back

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"
#include "cpu/m68000/m68000.h"

extern unsigned char *gaelco_vregs;
extern unsigned char *gaelco_videoram;
extern unsigned char *gaelco_spriteram;

/* from vidhrdw/gaelco.c */
READ_HANDLER( gaelco_vram_r );
WRITE_HANDLER( gaelco_vram_w );
void gaelco_vh_stop(void);


#define TILELAYOUT8(NUM) static struct GfxLayout tilelayout8_##NUM =	\
{																		\
	8,8,									/* 8x8 tiles */				\
	NUM/8,									/* number of tiles */		\
	4,										/* bitplanes */				\
	{ 0*NUM*8, 1*NUM*8, 2*NUM*8, 3*NUM*8 }, /* plane offsets */			\
	{ 0,1,2,3,4,5,6,7 },												\
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8 },								\
	8*8																	\
}

#define TILELAYOUT16(NUM) static struct GfxLayout tilelayout16_##NUM =				\
{																					\
	16,16,									/* 16x16 tiles */						\
	NUM/32,									/* number of tiles */					\
	4,										/* bitplanes */							\
	{ 0*NUM*8, 1*NUM*8, 2*NUM*8, 3*NUM*8 }, /* plane offsets */						\
	{ 0,1,2,3,4,5,6,7, 16*8+0,16*8+1,16*8+2,16*8+3,16*8+4,16*8+5,16*8+6,16*8+7 },	\
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8, 8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8 },		\
	32*8																			\
}

#define GFXDECODEINFO(NUM,ENTRIES) static struct GfxDecodeInfo gfxdecodeinfo_##NUM[] =	\
{																						\
	{ REGION_GFX1, 0x000000, &tilelayout8_##NUM,0,	ENTRIES },							\
	{ REGION_GFX1, 0x000000, &tilelayout16_##NUM,0,	ENTRIES },							\
	{ -1 }																				\
}

/*============================================================================
							BIG KARNAK
  ============================================================================*/

int bigkarnk_vh_start( void );
void bigkarnk_vh_screenrefresh( struct osd_bitmap *bitmap,int full_refresh );


static struct MemoryReadAddress bigkarnk_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },			/* ROM */
	{ 0x100000, 0x101fff, gaelco_vram_r },		/* Video RAM */
	{ 0x102000, 0x103fff, MRA_BANK1 },			/* Screen RAM */
	{ 0x200000, 0x2007ff, paletteram_word_r },	/* Palette */
	{ 0x440000, 0x440fff, MRA_BANK3 },			/* Sprite RAM */
	{ 0x700000, 0x700001, input_port_0_r },		/* DIPSW #1 */
	{ 0x700002, 0x700003, input_port_1_r },		/* DIPSW #2 */
	{ 0x700004, 0x700005, input_port_2_r },		/* INPUT #1 */
	{ 0x700006, 0x700007, input_port_3_r },		/* INPUT #2 */
	{ 0x700008, 0x700009, input_port_4_r },		/* Service + Test */
	{ 0xff8000, 0xffffff, MRA_BANK4 },			/* Work RAM */
	{ -1 }
};

WRITE_HANDLER( bigkarnk_sound_command_w )
{
	soundlatch_w(0,data & 0xff);
	cpu_cause_interrupt(1,M6809_INT_FIRQ);
}

WRITE_HANDLER( bigkarnk_coin_w )
{
	switch ((offset >> 4)){
		case 0x00:	/* Coin Lockouts */
		case 0x01:
			coin_lockout_w( (offset >> 4) & 0x01, ~data & 0x01);
			break;
		case 0x02:	/* Coin Counters */
		case 0x03:
			coin_counter_w( (offset >> 4) & 0x01, data & 0x01);
			break;
	}
}

static struct MemoryWriteAddress bigkarnk_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },								/* ROM */
	{ 0x100000, 0x101fff, gaelco_vram_w, &gaelco_videoram },		/* Video RAM */
	{ 0x102000, 0x103fff, MWA_BANK1 },								/* Screen RAM */
	{ 0x108000, 0x108007, MWA_BANK2, &gaelco_vregs },				/* Video Registers */
//	{ 0x10800c, 0x10800d, watchdog_reset_w },						/* INT 6 ACK/Watchdog timer */
	{ 0x200000, 0x2007ff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },/* Palette */
	{ 0x440000, 0x440fff, MWA_BANK3, &gaelco_spriteram },			/* Sprite RAM */
	{ 0x70000e, 0x70000f, bigkarnk_sound_command_w },				/* Triggers a FIRQ on the sound CPU */
	{ 0x70000a, 0x70003b, bigkarnk_coin_w },						/* Coin Counters + Coin Lockout */
	{ 0xff8000, 0xffffff, MWA_BANK4 },								/* Work RAM */
	{ -1 }
};


static struct MemoryReadAddress bigkarnk_readmem_snd[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },				/* RAM */
	{ 0x0800, 0x0800, OKIM6295_status_0_r },	/* OKI6295 */
	{ 0x0a00, 0x0a00, YM3812_status_port_0_r },	/* YM3812 */
	{ 0x0b00, 0x0b00, soundlatch_r },			/* Sound latch */
	{ 0x0c00, 0xffff, MRA_ROM },				/* ROM */
	{ -1 }
};

static struct MemoryWriteAddress bigkarnk_writemem_snd[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },				/* RAM */
	{ 0x0800, 0x0800, OKIM6295_data_0_w },		/* OKI6295 */
//	{ 0x0900, 0x0900, MWA_NOP },				/* enable sound output? */
	{ 0x0a00, 0x0a00, YM3812_control_port_0_w },/* YM3812 */
	{ 0x0a01, 0x0a01, YM3812_write_port_0_w },	/* YM3812 */
	{ 0x0c00, 0xffff, MWA_ROM },				/* ROM */
	{ -1 }
};

INPUT_PORTS_START( bigkarnk )
	PORT_START	/* DSW #1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, "Free Play (if Coin B too)" )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, "Free Play (if Coin A too)" )

	PORT_START	/* DSW #2 */
	PORT_DIPNAME( 0x07, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "0" )
	PORT_DIPSETTING(    0x06, "1" )
	PORT_DIPSETTING(    0x05, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x03, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x01, "6" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x18, 0x08, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x18, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Impact" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* 1P INPUTS & COINSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* 2P INPUTS & STARTSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* Service + Test */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_DIPNAME( 0x02, 0x02, "Go to test mode now" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xfc, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END


TILELAYOUT8(0x100000);
TILELAYOUT16(0x100000);

GFXDECODEINFO(0x100000,64);


static struct YM3812interface bigkarnk_ym3812_interface =
{
	1,						/* 1 chip */
	8867000/3,				/* 2.9556667 MHz? */
	{ 60 },					/* volume */
	{ 0 }					/* IRQ handler */
};

static struct OKIM6295interface bigkarnk_okim6295_interface =
{
	1,                  /* 1 chip */
	{ 8000 },			/* 8000 KHz? */
	{ REGION_SOUND1 },  /* memory region */
	{ 100 }				/* volume */
};


static struct MachineDriver machine_driver_bigkarnk =
{
	{
		{
			CPU_M68000,					/* MC68000P10 */
			10000000,					/* 10 MHz */
			bigkarnk_readmem,bigkarnk_writemem,0,0,
			m68_level6_irq,1
		},
		{
			CPU_M6809 | CPU_AUDIO_CPU,	/* 68B09 */
			8867000/4,					/* 2.21675 MHz? */
			bigkarnk_readmem_snd,bigkarnk_writemem_snd,0,0,
			ignore_interrupt,1
		}
	},
	60,DEFAULT_REAL_60HZ_VBLANK_DURATION,
	10,
	0,

	/* video hardware */
	32*16, 32*16, { 0, 320-1, 16, 256-1 },
	gfxdecodeinfo_0x100000,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	bigkarnk_vh_start,
	gaelco_vh_stop,
	bigkarnk_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM3812,
			&bigkarnk_ym3812_interface
		},
		{
			SOUND_OKIM6295,
			&bigkarnk_okim6295_interface
		}
	}
};


ROM_START( bigkarnk )
	ROM_REGION( 0x080000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_EVEN(	"d16",	0x000000, 0x040000, 0x44fb9c73 )
	ROM_LOAD_ODD(	"d19",	0x000000, 0x040000, 0xff79dfdd )

	ROM_REGION( 0x01e000, REGION_CPU2 )	/* 6809 code */
	ROM_LOAD(	"d5",	0x000000, 0x010000, 0x3b73b9c5 )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "h5",	0x000000, 0x080000, 0x20e239ff )
	ROM_RELOAD(		0x080000, 0x080000 )
	ROM_LOAD( "h10",0x100000, 0x080000, 0xab442855 )
	ROM_RELOAD(		0x180000, 0x080000 )
	ROM_LOAD( "h8",	0x200000, 0x080000, 0x83dce5a3 )
	ROM_RELOAD(		0x280000, 0x080000 )
	ROM_LOAD( "h6",	0x300000, 0x080000, 0x24e84b24 )
	ROM_RELOAD(		0x380000, 0x080000 )

	ROM_REGION( 0x040000, REGION_SOUND1 )	/* ADPCM samples - sound chip is OKIM6295 */
	ROM_LOAD( "d1",	0x000000, 0x040000, 0x26444ad1 )
ROM_END


/*============================================================================
					BIOMECHANICAL TOY & MANIAC SQUARE
  ============================================================================*/

int maniacsq_vh_start( void );
void maniacsq_vh_screenrefresh( struct osd_bitmap *bitmap,int full_refresh );


static struct MemoryReadAddress maniacsq_readmem[] =
{
	{ 0x000000, 0x0fffff, MRA_ROM },			/* ROM */
	{ 0x100000, 0x101fff, gaelco_vram_r },		/* Video RAM */
	{ 0x200000, 0x2007ff, paletteram_word_r },	/* Palette */
	{ 0x440000, 0x440fff, MRA_BANK2 },			/* Sprite RAM */
	{ 0x700000, 0x700001, input_port_0_r },		/* DIPSW #2 */
	{ 0x700002, 0x700003, input_port_1_r },		/* DIPSW #1 */
	{ 0x700004, 0x700005, input_port_2_r },		/* INPUT #1 */
	{ 0x700006, 0x700007, input_port_3_r },		/* INPUT #2 */
	{ 0x70000e, 0x70000f, OKIM6295_status_0_r },/* OKI6295 status register */
	{ 0xff0000, 0xffffff, MRA_BANK3 },			/* Work RAM */
	{ -1 }
};

static WRITE_HANDLER( OKIM6295_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_SOUND1);

	memcpy(&RAM[0x30000], &RAM[0x40000 + (data & 0x0f)*0x10000], 0x10000);
}

static struct MemoryWriteAddress maniacsq_writemem[] =
{
	{ 0x000000, 0x0fffff, MWA_ROM },								/* ROM */
	{ 0x100000, 0x101fff, gaelco_vram_w, &gaelco_videoram },		/* Video RAM */
	{ 0x108000, 0x108007, MWA_BANK1, &gaelco_vregs },				/* Video Registers */
//	{ 0x10800c, 0x10800d, watchdog_reset_w },						/* INT 6 ACK/Watchdog timer */
	{ 0x200000, 0x2007ff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },/* Palette */
	{ 0x440000, 0x440fff, MWA_BANK2, &gaelco_spriteram },			/* Sprite RAM */
	{ 0x70000c, 0x70000d, OKIM6295_bankswitch_w },					/* OKI6295 bankswitch */
	{ 0x70000e, 0x70000f, OKIM6295_data_0_w },						/* OKI6295 data register */
	{ 0xff0000, 0xffffff, MWA_BANK3 },								/* Work RAM */
	{ -1 }
};


INPUT_PORTS_START( maniacsq )

PORT_START	/* DSW #2 */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Sound Type" )
	PORT_DIPSETTING(    0x00, "Stereo" )
	PORT_DIPSETTING(    0x08, "Mono" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x80, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

PORT_START	/* DSW #1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, "1C/1C or Free Play (if Coin A too)" )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, "1C/1C or Free Play (if Coin B too)" )

PORT_START	/* 1P INPUTS & COINSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

PORT_START	/* 2P INPUTS & STARTSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )
INPUT_PORTS_END


INPUT_PORTS_START( biomtoy )
	PORT_START	/* DSW #2 */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "0" )
	PORT_DIPSETTING(    0x10, "1" )
	PORT_DIPSETTING(    0x30, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x80, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )

	PORT_START	/* DSW #1 */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )

	PORT_START	/* 1P INPUTS & COINSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* 2P INPUTS & STARTSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )
INPUT_PORTS_END


static struct OKIM6295interface maniacsq_okim6295_interface =
{
	1,                  /* 1 chip */
	{ 8000 },			/* 8000 KHz? */
	{ REGION_SOUND1 },  /* memory region */
	{ 100 }				/* volume */
};

static struct MachineDriver machine_driver_maniacsq =
{
	{
		{
			CPU_M68000,
			24000000/2,			/* 12 MHz */
			maniacsq_readmem,maniacsq_writemem,0,0,
			m68_level6_irq,1
		}
	},
	60,DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	32*16, 32*16, { 0, 320-1, 16, 256-1 },
	gfxdecodeinfo_0x100000,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	maniacsq_vh_start,
	gaelco_vh_stop,
	maniacsq_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_OKIM6295,
			&maniacsq_okim6295_interface
		}
	}
};


ROM_START( maniacsq )
	ROM_REGION( 0x100000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_EVEN(	"d18",	0x000000, 0x020000, 0x740ecab2 )
	ROM_LOAD_ODD(	"d16",	0x000000, 0x020000, 0xc6c42729 )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "f3",	0x000000, 0x040000, 0xe7f6582b )
	ROM_RELOAD(		0x080000, 0x040000 )
	/* 0x040000-0x07ffff and 0x0c0000-0x0fffff empty */
	ROM_LOAD( "f2",	0x100000, 0x040000, 0xca43a5ae )
	ROM_RELOAD(		0x180000, 0x040000 )
	/* 0x140000-0x17ffff and 0x1c0000-0x1fffff empty */
	ROM_LOAD( "f1",	0x200000, 0x040000, 0xfca112e8 )
	ROM_RELOAD(		0x280000, 0x040000 )
	/* 0x240000-0x27ffff and 0x2c0000-0x2fffff empty */
	ROM_LOAD( "f0",	0x300000, 0x040000, 0x6e829ee8 )
	ROM_RELOAD(		0x380000, 0x040000 )
	/* 0x340000-0x37ffff and 0x3c0000-0x3fffff empty */

	ROM_REGION( 0x140000, REGION_SOUND1 )	/* ADPCM samples - sound chip is OKIM6295 */
	ROM_LOAD( "c1",	0x000000, 0x080000, 0x2557f2d6 )
	/* 0x00000-0x2ffff is fixed, 0x30000-0x3ffff is bank switched from all the ROMs */
	ROM_RELOAD(		0x040000, 0x080000 )
	ROM_RELOAD(		0x0c0000, 0x080000 )
ROM_END


ROM_START( biomtoy )
	ROM_REGION( 0x100000, REGION_CPU1 )	/* 68000 code */
	ROM_LOAD_EVEN(	"d18",	0x000000, 0x080000, 0x4569ce64 )
	ROM_LOAD_ODD(	"d16",	0x000000, 0x080000, 0x739449bd )

	ROM_REGION( 0x400000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	/* weird gfx ordering */
	ROM_LOAD( "h6",		0x040000, 0x040000, 0x9416a729 )
	ROM_CONTINUE(		0x0c0000, 0x040000 )
	ROM_LOAD( "j6",		0x000000, 0x040000, 0xe923728b )
	ROM_CONTINUE(		0x080000, 0x040000 )
	ROM_LOAD( "h7",		0x140000, 0x040000, 0x9c984d7b )
	ROM_CONTINUE(		0x1c0000, 0x040000 )
	ROM_LOAD( "j7",		0x100000, 0x040000, 0x0e18fac2 )
	ROM_CONTINUE(		0x180000, 0x040000 )
	ROM_LOAD( "h9",		0x240000, 0x040000, 0x8c1f6718 )
	ROM_CONTINUE(		0x2c0000, 0x040000 )
	ROM_LOAD( "j9",		0x200000, 0x040000, 0x1c93f050 )
	ROM_CONTINUE(		0x280000, 0x040000 )
	ROM_LOAD( "h10",	0x340000, 0x040000, 0xaca1702b )
	ROM_CONTINUE(		0x3c0000, 0x040000 )
	ROM_LOAD( "j10",	0x300000, 0x040000, 0x8e3e96cc )
	ROM_CONTINUE(		0x380000, 0x040000 )

	ROM_REGION( 0x140000, REGION_SOUND1 )	/* ADPCM samples - sound chip is OKIM6295 */
	ROM_LOAD( "c1",	0x000000, 0x080000, 0x0f02de7e )
	/* 0x00000-0x2ffff is fixed, 0x30000-0x3ffff is bank switched from all the ROMs */
	ROM_RELOAD(		0x040000, 0x080000 )
	ROM_LOAD( "c3",	0x0c0000, 0x080000, 0x914e4bbc )
ROM_END



GAME( 1991, bigkarnk, 0, bigkarnk, bigkarnk, 0, ROT0_16BIT, "Gaelco", "Big Karnak" )
GAME( 1995, biomtoy,  0, maniacsq, biomtoy,  0, ROT0_16BIT, "Gaelco", "Biomechanical Toy (Unprotected)" )
GAME( 1996, maniacsq, 0, maniacsq, maniacsq, 0, ROT0_16BIT, "Gaelco", "Maniac Square (Prototype)" )
