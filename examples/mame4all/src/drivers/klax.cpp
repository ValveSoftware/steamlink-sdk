#include "../vidhrdw/klax.cpp"

/***************************************************************************

Klax Memory Map
---------------

KLAX 68000 MEMORY MAP

Program ROM             000000-05FFFF   R    D[15:0]
Program ROM slapstic    058000-05FFFF   R    D[15:0]   (not used!)

EEPROM                  0E0001-0E0FFF  R/W   D[7:0]    (odd bytes only)
UNLOCK EEPROM           1Fxxxx          W
Watch Dog               2E0000          W    xx        (128 msec. timeout)

Color RAM Motion Object 3E0000-3E03FE  R/W   D[15:8]
Color RAM Playfield     3E0400-3E07FE  R/W   D[15:8]

Playfield Picture RAM   3F0000-3F0EFF  R/W   D[15:0]
MOB config              3F0F00-3F0F70  R/W   D[15:0]
SLIP pointers           3F0F80-3F0FFF  R/W   M.O. link pointers
Playfield palette AM    3F1000-3F1FFF  R/W   D[11:8]
Motion Object RAM       3F2000-3F27FF  R/W   D[15:0]
(Link, Picture, H-Pos, V-Pos, Link... etc.)
Working RAM             3F2800-3F3FFF  R/W   D[15:0]

Player 1 Input (left)   260000          R    D[15:12],D8 Active lo
Player 2 Input (right)  260002          R    D[15:12],D8 Active lo
      D8:    flip
      D12:   right
      D13:   left
      D14:   down
      D15:   up

Status inputs           260000          R    D11,D1,D0
      D0:    coin 1 (left) Active lo
      D1:    coin 2 (right) Active lo
      D11:   self-test Active lo

LATCH                   260000          W    D[12:8]
      D8:    ADPCM chip reset (active lo)
      D9:    Spare
      D10:   Coin Counter 2 (right)
      D11:   Coin Counter 1 (left)
      D12:   Spare
      D13:   Color RAM bank select
  NOTE: RESET clears this latch

4ms Interrupt ack.      360000          W    xx

ADPCM chip              270000         R/W   D[7:0]

****************************************************************************/



#include "driver.h"
#include "machine/atarigen.h"
#include "vidhrdw/generic.h"


WRITE_HANDLER( klax_playfieldram_w );
WRITE_HANDLER( klax_latch_w );

int klax_vh_start(void);
void klax_vh_stop(void);
void klax_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

void klax_scanline_update(int scanline);



/*************************************
 *
 *	Interrupt handling
 *
 *************************************/

static void update_interrupts(void)
{
	int newstate = 0;

	if (atarigen_video_int_state || atarigen_scanline_int_state)
		newstate = 4;

	if (newstate)
		cpu_set_irq_line(0, newstate, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 7, CLEAR_LINE);
}


static void scanline_update(int scanline)
{
	/* update the video */
	klax_scanline_update(scanline);

	/* generate 32V signals */
	if (scanline % 64 == 0 && !(readinputport(0) & 0x800))
		atarigen_scanline_int_gen();
}


static WRITE_HANDLER( interrupt_ack_w )
{
	atarigen_scanline_int_ack_w(offset, data);
	atarigen_video_int_ack_w(offset, data);
}



/*************************************
 *
 *	Initialization
 *
 *************************************/

static void init_machine(void)
{
	atarigen_eeprom_reset();
	atarigen_interrupt_reset(update_interrupts);
	atarigen_scanline_timer_reset(scanline_update, 8);
}



/*************************************
 *
 *	Sound I/O
 *
 *************************************/

static READ_HANDLER( adpcm_r )
{
	return OKIM6295_status_0_r(offset) | 0xff00;
}


static WRITE_HANDLER( adpcm_w )
{
	if (!(data & 0x00ff0000))
		OKIM6295_data_0_w(offset, data & 0xff);
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x0e0000, 0x0e0fff, atarigen_eeprom_r },
	{ 0x260000, 0x260001, input_port_0_r },
	{ 0x260002, 0x260003, input_port_1_r },
	{ 0x270000, 0x270001, adpcm_r },
	{ 0x3e0000, 0x3e07ff, MRA_BANK1 },
	{ 0x3f0000, 0x3f1fff, MRA_BANK2 },
	{ 0x3f2000, 0x3f27ff, MRA_BANK3 },
	{ 0x3f2800, 0x3f3fff, MRA_BANK4 },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x0e0000, 0x0e0fff, atarigen_eeprom_w, &atarigen_eeprom, &atarigen_eeprom_size },
	{ 0x1f0000, 0x1fffff, atarigen_eeprom_enable_w },
	{ 0x260000, 0x260001, klax_latch_w },
	{ 0x270000, 0x270001, adpcm_w },
	{ 0x2e0000, 0x2e0001, watchdog_reset_w },
	{ 0x360000, 0x360001, interrupt_ack_w },
	{ 0x3e0000, 0x3e07ff, atarigen_expanded_666_paletteram_w, &paletteram },
	{ 0x3f0000, 0x3f1fff, klax_playfieldram_w, &atarigen_playfieldram, &atarigen_playfieldram_size },
	{ 0x3f2000, 0x3f27ff, MWA_BANK3, &atarigen_spriteram, &atarigen_spriteram_size },
	{ 0x3f2800, 0x3f3fff, MWA_BANK4 },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( klax )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x00fc, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0600, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER1 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER1 )

	PORT_START
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0600, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x0800, IP_ACTIVE_LOW )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout pflayout =
{
	8,8,	/* 8*8 sprites */
	8192,	/* 8192 of them */
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0, 4, 0x20000*8+0, 0x20000*8+4, 8, 12, 0x20000*8+8, 0x20000*8+12 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};


static struct GfxLayout molayout =
{
	8,8,	/* 8*8 sprites */
	4096,	/* 4096 of them */
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0, 4, 0x10000*8+0, 0x10000*8+4, 8, 12, 0x10000*8+8, 0x10000*8+12 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &pflayout,  256, 16 },		/* sprites & playfield */
	{ REGION_GFX2, 0, &molayout,    0, 16 },		/* sprites & playfield */
	{ -1 } /* end of array */
};



/*************************************
 *
 *	Sound definitions
 *
 *************************************/

static struct OKIM6295interface okim6295_interface =
{
	1,					/* 1 chip */
	{ ATARI_CLOCK_14MHz/4/4/132 },
	{ REGION_SOUND1 },
	{ 100 }
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_klax =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,		/* verified */
			ATARI_CLOCK_14MHz/2,
			readmem,writemem,0,0,
			atarigen_video_int_gen,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	init_machine,

	/* video hardware */
	42*8, 30*8, { 0*8, 42*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	512, 512,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_SUPPORTS_DIRTY,
	0,
	klax_vh_start,
	klax_vh_stop,
	klax_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	},

	atarigen_nvram_handler
};



/*************************************
 *
 *	ROM definition(s)
 *
 *************************************/

ROM_START( klax )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "136075-6.006", 0x00000, 0x10000, 0xe8991709 )
	ROM_LOAD_ODD ( "136075-6.005", 0x00000, 0x10000, 0x72b8c510 )
	ROM_LOAD_EVEN( "136075-6.008", 0x20000, 0x10000, 0xc7c91a9d )
	ROM_LOAD_ODD ( "136075-6.007", 0x20000, 0x10000, 0xd2021a88 )

	ROM_REGION( 0x40000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136075-2.010", 0x00000, 0x10000, 0x15290a0d )
	ROM_LOAD( "136075-2.012", 0x10000, 0x10000, 0xc0d9eb0f )
	ROM_LOAD( "136075-2.009", 0x20000, 0x10000, 0x6368dbaf )
	ROM_LOAD( "136075-2.011", 0x30000, 0x10000, 0xe83cca91 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136075-2.014", 0x00000, 0x10000, 0x5c551e92 )
	ROM_LOAD( "136075-2.013", 0x10000, 0x10000, 0x36764bbc )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM data */
	ROM_LOAD( "136075-1.015", 0x00000, 0x10000, 0x4d24c768 )
	ROM_LOAD( "136075-1.016", 0x10000, 0x10000, 0x12e9b4b7 )
ROM_END


ROM_START( klax2 )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "136075.006",   0x00000, 0x10000, 0x05c98fc0 )
	ROM_LOAD_ODD ( "136075.005",   0x00000, 0x10000, 0xd461e1ee )
	ROM_LOAD_EVEN( "136075.008",   0x20000, 0x10000, 0xf1b8e588 )
	ROM_LOAD_ODD ( "136075.007",   0x20000, 0x10000, 0xadbe33a8 )

	ROM_REGION( 0x40000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136075-2.010", 0x00000, 0x10000, 0x15290a0d )
	ROM_LOAD( "136075-2.012", 0x10000, 0x10000, 0xc0d9eb0f )
	ROM_LOAD( "136075-2.009", 0x20000, 0x10000, 0x6368dbaf )
	ROM_LOAD( "136075-2.011", 0x30000, 0x10000, 0xe83cca91 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136075-2.014", 0x00000, 0x10000, 0x5c551e92 )
	ROM_LOAD( "136075-2.013", 0x10000, 0x10000, 0x36764bbc )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM data */
	ROM_LOAD( "136075-1.015", 0x00000, 0x10000, 0x4d24c768 )
	ROM_LOAD( "136075-1.016", 0x10000, 0x10000, 0x12e9b4b7 )
ROM_END


ROM_START( klax3 )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "5006",         0x00000, 0x10000, 0x65eb9a31 )
	ROM_LOAD_ODD ( "5005",         0x00000, 0x10000, 0x7be27349 )
	ROM_LOAD_EVEN( "4008",         0x20000, 0x10000, 0xf3c79106 )
	ROM_LOAD_ODD ( "4007",         0x20000, 0x10000, 0xa23cde5d )

	ROM_REGION( 0x40000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136075-2.010", 0x00000, 0x10000, 0x15290a0d )
	ROM_LOAD( "136075-2.012", 0x10000, 0x10000, 0xc0d9eb0f )
	ROM_LOAD( "136075-2.009", 0x20000, 0x10000, 0x6368dbaf )
	ROM_LOAD( "136075-2.011", 0x30000, 0x10000, 0xe83cca91 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136075-2.014", 0x00000, 0x10000, 0x5c551e92 )
	ROM_LOAD( "136075-2.013", 0x10000, 0x10000, 0x36764bbc )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM data */
	ROM_LOAD( "136075-1.015", 0x00000, 0x10000, 0x4d24c768 )
	ROM_LOAD( "136075-1.016", 0x10000, 0x10000, 0x12e9b4b7 )
ROM_END


ROM_START( klaxj )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "136075-3.406", 0x00000, 0x10000, 0xab2aa50b )
	ROM_LOAD_ODD ( "136075-3.405", 0x00000, 0x10000, 0x9dc9a590 )
	ROM_LOAD_EVEN( "136075-2.408", 0x20000, 0x10000, 0x89d515ce )
	ROM_LOAD_ODD ( "136075-2.407", 0x20000, 0x10000, 0x48ce4edb )

	ROM_REGION( 0x40000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136075-2.010", 0x00000, 0x10000, 0x15290a0d )
	ROM_LOAD( "136075-2.012", 0x10000, 0x10000, 0xc0d9eb0f )
	ROM_LOAD( "136075-2.009", 0x20000, 0x10000, 0x6368dbaf )
	ROM_LOAD( "136075-2.011", 0x30000, 0x10000, 0xe83cca91 )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136075-2.014", 0x00000, 0x10000, 0x5c551e92 )
	ROM_LOAD( "136075-2.013", 0x10000, 0x10000, 0x36764bbc )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM data */
	ROM_LOAD( "136075-1.015", 0x00000, 0x10000, 0x4d24c768 )
	ROM_LOAD( "136075-1.016", 0x10000, 0x10000, 0x12e9b4b7 )
ROM_END



/*************************************
 *
 *	Driver initialization
 *
 *************************************/

static void init_klax(void)
{
	atarigen_eeprom_default = NULL;
}



/*************************************
 *
 *	Game driver(s)
 *
 *************************************/

GAME( 1989, klax,  0,    klax, klax, klax, ROT0, "Atari Games", "Klax (set 1)" )
GAME( 1989, klax2, klax, klax, klax, klax, ROT0, "Atari Games", "Klax (set 2)" )
GAME( 1989, klax3, klax, klax, klax, klax, ROT0, "Atari Games", "Klax (set 3)" )
GAME( 1989, klaxj, klax, klax, klax, klax, ROT0, "Atari Games", "Klax (Japan)" )
