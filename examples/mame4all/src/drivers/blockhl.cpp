#include "../vidhrdw/blockhl.cpp"

/***************************************************************************

Block Hole (GX973) (c) 1989 Konami

driver by Nicola Salmoria

Notes:
Quarth works, but Block Hole crashes when it reaches the title screen. An
interrupt happens, and after rti the ROM bank is not the same as before so
it jumps to garbage code.
If you want to see this happen, place a breakpoint at 0x8612, and trace
after that.
The code is almost identical in the two versions, it looks like Quarth is
working just because luckily the interrupt doesn't happen at that point.
It seems that the interrupt handler trashes the selected ROM bank and forces
it to 0. To prevent crashes, I only generate interrupts when the ROM bank is
already 0. There might be another interrupt enable register, but I haven't
found it.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/konami/konami.h" /* for the callback and the firq irq definition */
#include "vidhrdw/konamiic.h"

/* prototypes */
static void blockhl_init_machine( void );
static void blockhl_banking( int lines );


void blockhl_vh_stop( void );
int blockhl_vh_start( void );
void blockhl_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

static int palette_selected;
static unsigned char *ram;
static int rombank;

static int blockhl_interrupt( void )
{
	if (K052109_is_IRQ_enabled() && rombank == 0)	/* kludge to prevent crashes */
		return KONAMI_INT_IRQ;
	else
		return ignore_interrupt();
}

static READ_HANDLER( bankedram_r )
{
	if (palette_selected)
		return paletteram_r(offset);
	else
		return ram[offset];
}

static WRITE_HANDLER( bankedram_w )
{
	if (palette_selected)
		paletteram_xBBBBBGGGGGRRRRR_swap_w(offset,data);
	else
		ram[offset] = data;
}

WRITE_HANDLER( blockhl_sh_irqtrigger_w )
{
	cpu_cause_interrupt(1,0xff);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x1f94, 0x1f94, input_port_4_r },
	{ 0x1f95, 0x1f95, input_port_0_r },
	{ 0x1f96, 0x1f96, input_port_1_r },
	{ 0x1f97, 0x1f97, input_port_2_r },
	{ 0x1f98, 0x1f98, input_port_3_r },
	{ 0x0000, 0x3fff, K052109_051960_r },
	{ 0x4000, 0x57ff, MRA_RAM },
	{ 0x5800, 0x5fff, bankedram_r },
	{ 0x6000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x1f84, 0x1f84, soundlatch_w },
	{ 0x1f88, 0x1f88, blockhl_sh_irqtrigger_w },
	{ 0x1f8c, 0x1f8c, watchdog_reset_w },
	{ 0x0000, 0x3fff, K052109_051960_w },
	{ 0x4000, 0x57ff, MWA_RAM },
	{ 0x5800, 0x5fff, bankedram_w, &ram },
	{ 0x6000, 0x7fff, MWA_ROM },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xa000, 0xa000, soundlatch_r },
	{ 0xc001, 0xc001, YM2151_status_port_0_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xc000, 0xc000, YM2151_register_port_0_w },
	{ 0xc001, 0xc001, YM2151_data_port_0_w },
	{ 0xe00c, 0xe00d, MWA_NOP },		/* leftover from missing 007232? */
	{ -1 }	/* end of table */
};

/***************************************************************************

	Input Ports

***************************************************************************/

INPUT_PORTS_START( blockhl )
	PORT_START	/* PLAYER 1 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* PLAYER 2 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* DSW #1 */
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
//	PORT_DIPSETTING(    0x00, "Invalid" )

	PORT_START	/* DSW #2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
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
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Normal" )
	PORT_DIPSETTING(    0x20, "Difficult" )
	PORT_DIPSETTING(    0x00, "Very Difficult" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW #3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


/***************************************************************************

	Machine Driver

***************************************************************************/

static struct YM2151interface ym2151_interface =
{
	1, /* 1 chip */
	3579545, /* 3.579545 MHz */
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) },
	{ 0 },
	{ 0 }
};

static struct MachineDriver machine_driver_blockhl =
{
	/* basic machine hardware */
	{
		{
			CPU_KONAMI,		/* Konami custom 052526 */
			3000000,		/* ? */
			readmem,writemem,0,0,
            blockhl_interrupt,1
        },
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,		/* ? */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	blockhl_init_machine,

	/* video hardware */
	64*8, 32*8, { 14*8, (64-14)*8-1, 2*8, 30*8-1 },
	0,	/* gfx decoded by konamiic.c */
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	blockhl_vh_start,
	blockhl_vh_stop,
	blockhl_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	}
};


/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( blockhl )
	ROM_REGION( 0x18800, REGION_CPU1 ) /* code + banked roms + space for banked RAM */
	ROM_LOAD( "973l02.e21", 0x10000, 0x08000, 0xe14f849a )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "973d01.g6",  0x0000, 0x8000, 0xeeee9d92 )

	ROM_REGION( 0x20000, REGION_GFX1 ) /* graphics (addressable by the main CPU) */
	ROM_LOAD_GFX_EVEN( "973f07.k15", 0x00000, 0x08000, 0x1a8cd9b4 )	/* tiles */
	ROM_LOAD_GFX_ODD ( "973f08.k18", 0x00000, 0x08000, 0x952b51a6 )
	ROM_LOAD_GFX_EVEN( "973f09.k20", 0x10000, 0x08000, 0x77841594 )
	ROM_LOAD_GFX_ODD ( "973f10.k23", 0x10000, 0x08000, 0x09039fab )

	ROM_REGION( 0x20000, REGION_GFX2 ) /* graphics (addressable by the main CPU) */
	ROM_LOAD_GFX_EVEN( "973f06.k12", 0x00000, 0x08000, 0x51acfdb6 )	/* sprites */
	ROM_LOAD_GFX_ODD ( "973f05.k9",  0x00000, 0x08000, 0x4cfea298 )
	ROM_LOAD_GFX_EVEN( "973f04.k7",  0x10000, 0x08000, 0x69ca41bd )
	ROM_LOAD_GFX_ODD ( "973f03.k4",  0x10000, 0x08000, 0x21e98472 )

	ROM_REGION( 0x0100, REGION_PROMS )	/* PROMs */
	ROM_LOAD( "973a11.h10", 0x0000, 0x0100, 0x46d28fe9 )	/* priority encoder (not used) */
ROM_END

ROM_START( quarth )
	ROM_REGION( 0x18800, REGION_CPU1 ) /* code + banked roms + space for banked RAM */
	ROM_LOAD( "973j02.e21", 0x10000, 0x08000, 0x27a90118 )
	ROM_CONTINUE(           0x08000, 0x08000 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "973d01.g6",  0x0000, 0x8000, 0xeeee9d92 )

	ROM_REGION( 0x20000, REGION_GFX1 ) /* graphics (addressable by the main CPU) */
	ROM_LOAD_GFX_EVEN( "973e07.k15", 0x00000, 0x08000, 0x0bd6b0f8 )	/* tiles */
	ROM_LOAD_GFX_ODD ( "973e08.k18", 0x00000, 0x08000, 0x104d0d5f )
	ROM_LOAD_GFX_EVEN( "973e09.k20", 0x10000, 0x08000, 0xbd3a6f24 )
	ROM_LOAD_GFX_ODD ( "973e10.k23", 0x10000, 0x08000, 0xcf5e4b86 )

	ROM_REGION( 0x20000, REGION_GFX2 ) /* graphics (addressable by the main CPU) */
	ROM_LOAD_GFX_EVEN( "973e06.k12", 0x00000, 0x08000, 0x0d58af85 )	/* sprites */
	ROM_LOAD_GFX_ODD ( "973e05.k9",  0x00000, 0x08000, 0x15d822cb )
	ROM_LOAD_GFX_EVEN( "973e04.k7",  0x10000, 0x08000, 0xd70f4a2c )
	ROM_LOAD_GFX_ODD ( "973e03.k4",  0x10000, 0x08000, 0x2c5a4b4b )

	ROM_REGION( 0x0100, REGION_PROMS )	/* PROMs */
	ROM_LOAD( "973a11.h10", 0x0000, 0x0100, 0x46d28fe9 )	/* priority encoder (not used) */
ROM_END


/***************************************************************************

  Game driver(s)

***************************************************************************/

static void blockhl_banking( int lines )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int offs;

	/* bits 0-1 = ROM bank */
	rombank = lines & 0x03;
	offs = 0x10000 + (lines & 0x03) * 0x2000;
	cpu_setbank(1,&RAM[offs]);

	/* bits 3/4 = coin counters */
	coin_counter_w(0,lines & 0x08);
	coin_counter_w(1,lines & 0x10);

	/* bit 5 = select palette RAM or work RAM at 5800-5fff */
	palette_selected = ~lines & 0x20;

	/* bit 6 = enable char ROM reading through the video RAM */
	K052109_set_RMRD_line( ( lines & 0x40 ) ? ASSERT_LINE : CLEAR_LINE );

	/* bit 7 used but unknown */

	/* other bits unknown */

	//if ((lines & 0x84) != 0x80) logerror("%04x: setlines %02x\n",cpu_get_pc(),lines);
}

static void blockhl_init_machine( void )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	konami_cpu_setlines_callback = blockhl_banking;

	paletteram = &RAM[0x18000];
}


static void init_blockhl(void)
{
	konami_rom_deinterleave_2(REGION_GFX1);
	konami_rom_deinterleave_2(REGION_GFX2);
}



GAME( 1989, blockhl, 0,       blockhl, blockhl, blockhl, ROT0, "Konami", "Block Hole" )
GAME( 1989, quarth,  blockhl, blockhl, blockhl, blockhl, ROT0, "Konami", "Quarth (Japan)" )
