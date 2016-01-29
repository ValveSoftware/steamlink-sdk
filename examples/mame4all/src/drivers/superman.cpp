#include "../vidhrdw/superman.cpp"

/***************************************************************************

Superman memory map

driver by Richard Bush, Howie Cohen

CPU 1 : 68000, uses irq 6

0x000000 - 0x07ffff : ROM
0x300000 ??
0x400000 ??
0x500000 - 0x50000f : Dipswitches a & b, 4 bits to each word
0x600000 ?? 0, 10, 0x4001, 0x4006
0x700000 ??
0x800000 - 0x800003 : sound chip
0x900000 - 0x900fff : c-chip shared RAM space
0xb00000 - 0xb00fff : palette RAM, words in the format xRRRRRGGGGGBBBBB
0xc00000 ??
0xd00000 - 0xd007ff : video attribute RAM
    0000 - 03ff : sprite y coordinate
    0400 - 07ff : tile x & y scroll
0xe00000 - 0xe00fff : object RAM
    0000 - 03ff : sprite number (bit mask 0x3fff)
                  sprite y flip (bit mask 0x4000)
                  sprite x flip (bit mask 0x8000)
    0400 - 07ff : sprite x coordinate (bit mask 0x1ff)
                  sprite color (bit mask 0xf800)
    0800 - 0bff : tile number (bit mask 0x3fff)
                  tile y flip (bit mask 0x4000)
                  tile x flip (bit mask 0x8000)
    0c00 - 0fff : tile color (bit mask 0xf800)
0xe01000 - 0xe03fff : unused(?) portion of object RAM

TODO:
	* Optimize rendering
	* Does high score save work consistently?

***************************************************************************/

#include "driver.h"
#include "cpu/m68000/m68000.h"
#include "vidhrdw/generic.h"


WRITE_HANDLER( supes_attribram_w );
READ_HANDLER( supes_attribram_r );
WRITE_HANDLER( supes_videoram_w );
READ_HANDLER( supes_videoram_r );
void superman_vh_screenrefresh (struct osd_bitmap *bitmap, int full_refresh);
int superman_vh_start (void);
void superman_vh_stop (void);

extern size_t supes_videoram_size;
extern size_t supes_attribram_size;

extern unsigned char *supes_videoram;
extern unsigned char *supes_attribram;

/* Routines found in sndhrdw/rastan.c */
READ_HANDLER( rastan_a001_r );
WRITE_HANDLER( rastan_a000_w );
WRITE_HANDLER( rastan_a001_w );

WRITE_HANDLER( rastan_sound_w );
READ_HANDLER( rastan_sound_r );

static unsigned char *ram; /* for high score save */

void cchip1_init_machine(void);
READ_HANDLER( cchip1_r );
WRITE_HANDLER( cchip1_w );

READ_HANDLER( superman_input_r )
{
	switch (offset)
	{
		case 0x00:
			return readinputport (0);
		case 0x02:
			return readinputport (1);
		case 0x04:
			return readinputport (2);
		case 0x06:
			return readinputport (3);
		default:
			//logerror("superman_input_r offset: %04x\n", offset);
			return 0xff;
	}
}



static WRITE_HANDLER( taito68k_sound_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU2);

	int banknum = ( data - 1 ) & 3;

	cpu_setbank( 2, &RAM[ 0x10000 + ( banknum * 0x4000 ) ] );
}


static struct MemoryReadAddress superman_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x500000, 0x50000f, superman_input_r },	/* DSW A/B */
	{ 0x800000, 0x800003, rastan_sound_r },
	{ 0x900000, 0x900fff, cchip1_r } ,
	{ 0xb00000, 0xb00fff, paletteram_word_r },
	{ 0xd00000, 0xd007ff, supes_attribram_r },
	{ 0xe00000, 0xe03fff, supes_videoram_r },
	{ 0xf00000, 0xf03fff, MRA_BANK1 },	/* Main RAM */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress superman_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x800000, 0x800003, rastan_sound_w },
	{ 0x900000, 0x900fff, cchip1_w },
	{ 0xb00000, 0xb00fff, paletteram_xRRRRRGGGGGBBBBB_word_w, &paletteram },
	{ 0xd00000, 0xd007ff, supes_attribram_w, &supes_attribram, &supes_attribram_size },
	{ 0xe00000, 0xe03fff, supes_videoram_w, &supes_videoram, &supes_videoram_size },
	{ 0xf00000, 0xf03fff, MWA_BANK1, &ram },			/* Main RAM */
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x7fff, MRA_BANK2 },
	{ 0xc000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xe000, YM2610_status_port_0_A_r },
	{ 0xe001, 0xe001, YM2610_read_port_0_r },
	{ 0xe002, 0xe002, YM2610_status_port_0_B_r },
	{ 0xe200, 0xe200, MRA_NOP },
	{ 0xe201, 0xe201, rastan_a001_r },
	{ 0xea00, 0xea00, MRA_NOP },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xdfff, MWA_RAM },
	{ 0xe000, 0xe000, YM2610_control_port_0_A_w },
	{ 0xe001, 0xe001, YM2610_data_port_0_A_w },
	{ 0xe002, 0xe002, YM2610_control_port_0_B_w },
	{ 0xe003, 0xe003, YM2610_data_port_0_B_w },
	{ 0xe200, 0xe200, rastan_a000_w },
	{ 0xe201, 0xe201, rastan_a001_w },
	{ 0xe400, 0xe403, MWA_NOP }, /* pan */
	{ 0xee00, 0xee00, MWA_NOP }, /* ? */
	{ 0xf000, 0xf000, MWA_NOP }, /* ? */
	{ 0xf200, 0xf200, taito68k_sound_bankswitch_w }, /* bankswitch? */
	{ -1 }  /* end of table */
};

INPUT_PORTS_START( superman )
	PORT_START /* DSW A */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ))
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* DSW B */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* DSW c */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "50k and every 150k" )
	PORT_DIPSETTING(    0x04, "Bonus 2??" )
	PORT_DIPSETTING(    0x08, "Bonus 3??" )
	PORT_DIPSETTING(    0x00, "Bonus 4??" )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* DSW D */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

#define NUM_TILES 16384
static struct GfxLayout tilelayout =
{
	16,16,  /* 16*16 sprites */
	NUM_TILES,	/* 16384 of them */
	4,	       /* 4 bits per pixel */
	{ 64*8*NUM_TILES + 8, 64*8*NUM_TILES + 0, 8, 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
		8*16, 8*16+1, 8*16+2, 8*16+3, 8*16+4, 8*16+5, 8*16+6, 8*16+7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
		16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 },

	64*8	/* every sprite takes 64 consecutive bytes */
};
#undef NUM_TILES

static struct GfxDecodeInfo superman_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x000000, &tilelayout,    0, 256 },	 /* sprites & playfield */
	{ -1 } /* end of array */
};

/* handler called by the YM2610 emulator when the internal timers cause an IRQ */
static void irqhandler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}


static struct YM2610interface ym2610_interface =
{
	1,	/* 1 chip */
	8000000,	/* 8 MHz ?????? */
	{ 30 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler },
	{ REGION_SOUND1 },
	{ REGION_SOUND1 },
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) }
};


static struct MachineDriver machine_driver_superman =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			8000000,	/* 8 MHz? */
			superman_readmem,superman_writemem,0,0,
			m68_level6_irq,1
		},
		{
			CPU_Z80,
			4000000,	/* 4 MHz ??? */
			sound_readmem, sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2610 */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	cchip1_init_machine,

	/* video hardware */
	48*8, 32*8, { 2*8, 46*8-1, 2*8, 32*8-1 },

	superman_gfxdecodeinfo,
	4096,4096,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	superman_vh_start,
	superman_vh_stop,
	superman_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2610,
			&ym2610_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( superman )
	ROM_REGION( 0x80000, REGION_CPU1 )     /* 512k for 68000 code */
	ROM_LOAD_EVEN( "a10_09.bin", 0x00000, 0x20000, 0x640f1d58 )
	ROM_LOAD_ODD ( "a05_07.bin", 0x00000, 0x20000, 0xfddb9953 )
	ROM_LOAD_EVEN( "a08_08.bin", 0x40000, 0x20000, 0x79fc028e )
	ROM_LOAD_ODD ( "a03_13.bin", 0x40000, 0x20000, 0x9f446a44 )

	ROM_REGION( 0x1c000, REGION_CPU2 )     /* 64k for Z80 code */
	ROM_LOAD( "d18_10.bin", 0x00000, 0x4000, 0x6efe79e8 )
	ROM_CONTINUE(           0x10000, 0xc000 ) /* banked stuff */

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "f01_14.bin", 0x000000, 0x80000, 0x89368c3e ) /* Plane 0, 1 */
	ROM_LOAD( "h01_15.bin", 0x080000, 0x80000, 0x910cc4f9 )
	ROM_LOAD( "j01_16.bin", 0x100000, 0x80000, 0x3622ed2f ) /* Plane 2, 3 */
	ROM_LOAD( "k01_17.bin", 0x180000, 0x80000, 0xc34f27e0 )

	ROM_REGION( 0x80000, REGION_SOUND1 )	/* adpcm samples */
	ROM_LOAD( "e18_01.bin", 0x00000, 0x80000, 0x3cf99786 )
ROM_END



GAMEX( 1988, superman, 0, superman, superman, 0, ROT0, "Taito Corporation", "Superman", GAME_NO_COCKTAIL )
