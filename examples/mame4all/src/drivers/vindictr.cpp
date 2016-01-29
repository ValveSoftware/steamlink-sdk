#include "../vidhrdw/vindictr.cpp"

/***************************************************************************

vindictr Memory Map
----------------

driver by Aaron Giles


vindictr 68010 MEMORY MAP

Program ROM             000000-09FFFF   R    D15-D0

EEPROM                  0E0001-0E0FFF  R/W   D7-D0    (odd bytes only)
Program RAM             160000-16FFFF  R/W   D15-D0
UNLOCK EEPROM           1Fxxxx          W

Player 1 Input (left)   260000          R    D11-D8 Active lo
Player 2 Input (right)  260010          R    D11-D8 Active lo
      D8:    start
      D9:    fire
      D10:   spare
      D11:   duck

VBLANK                  260010          R    D0 Active lo
Self-test                               R    D1 Active lo
Input buffer full (@260030)             R    D2 Active lo
Output buffer full (@360030)            R    D3 Active lo
ADEOC, end of conversion                R    D4 Active hi

ADC0, analog port       260020          R    Start

Read sound processor    260030          R    D0-D7
Watch Dog               2E0000          W    xx        (128 msec. timeout)
VBLANK Interrupt ack.   360000          W    xx

Video off               360010          W    D5 (active hi)
Video intensity                         W    D1-D4 (0=full on)
EXTRA cpu reset                         W    D0 (lo to reset)

Sound processor reset   360020          W    xx
Write sound processor   360030          W    D0-D7

Color RAM Alpha         3E0000-3E01FF  R/W   D15-D0
Color RAM Motion Object 3E0200-3E03FF  R/W   D15-D0
Color RAM Playfield     3E0400-3E07FE  R/W   D15-D0
Color RAM STAIN         3E0800-3E0FFE  R/W   D15-D0

Playfield Picture RAM   3F0000-3F1FFF  R/W   D15-D0
Motion Object RAM       3F2000-3F3FFF  R/W   D15-D0
Alphanumerics RAM       3F4000-3F4EFF  R/W   D15-D0
Scroll and MOB config   3F4F00-3F4F70  R/W   D15-D0
SLIP pointers           3F4F80-3F4FFF  R/W   D9-D0
Working RAM             3F5000-3F7FFF  R/W   D15-D0

Playfield Picture RAM   FF8000-FF9FFF  R/W   D15-D0
Motion Object RAM       FFA000-FFBFFF  R/W   D15-D0
Alphanumerics RAM       FFC000-FFCEFF  R/W   D15-D0
Scroll and MOB config   FFCF00-FFCF70  R/W   D15-D0
SLIP pointers           FFCF80-FFCFFF  R/W   D9-D0
Working RAM             FFD000-FFFFFF  R/W   D15-D0

****************************************************************************/



#include "driver.h"
#include "machine/atarigen.h"
#include "sndhrdw/atarijsa.h"
#include "vidhrdw/generic.h"


WRITE_HANDLER( vindictr_playfieldram_w );
WRITE_HANDLER( vindictr_paletteram_w );

int vindictr_vh_start(void);
void vindictr_vh_stop(void);
void vindictr_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

void vindictr_scanline_update(int scanline);



/*************************************
 *
 *	Shared RAM handling
 *
 *************************************/

static UINT8 *shared_ram_4;

static READ_HANDLER( shared_ram_1_r ) { return READ_WORD(&atarigen_playfieldram[offset]); }
static READ_HANDLER( shared_ram_2_r ) { return READ_WORD(&atarigen_spriteram[offset]); }
static READ_HANDLER( shared_ram_3_r ) { return READ_WORD(&atarigen_alpharam[offset]); }
static READ_HANDLER( shared_ram_4_r ) { return READ_WORD(&shared_ram_4[offset]); }

//static WRITE_HANDLER( shared_ram_1_w ) { COMBINE_WORD_MEM(&atarigen_playfieldram[offset], data); }
static WRITE_HANDLER( shared_ram_2_w ) { COMBINE_WORD_MEM(&atarigen_spriteram[offset], data); }
static WRITE_HANDLER( shared_ram_3_w ) { COMBINE_WORD_MEM(&atarigen_alpharam[offset], data); }
static WRITE_HANDLER( shared_ram_4_w ) { COMBINE_WORD_MEM(&shared_ram_4[offset], data); }



/*************************************
 *
 *	Initialization
 *
 *************************************/

static void update_interrupts(void)
{
	int newstate = 0;

	if (atarigen_video_int_state)
		newstate |= 4;
	if (atarigen_sound_int_state)
		newstate |= 6;

	if (newstate)
		cpu_set_irq_line(0, newstate, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 7, CLEAR_LINE);
}


static void init_machine(void)
{
	atarigen_eeprom_reset();
	atarigen_interrupt_reset(update_interrupts);
	atarigen_scanline_timer_reset(vindictr_scanline_update, 8);
	atarijsa_reset();
}



/*************************************
 *
 *	I/O handling
 *
 *************************************/

static int fake_inputs(int real_port, int fake_port)
{
	int result = readinputport(real_port);
	int fake = readinputport(fake_port);

	if (fake & 0x01)			/* up */
	{
		if (fake & 0x04)		/* up and left */
			result &= ~0x2000;
		else if (fake & 0x08)	/* up and right */
			result &= ~0x1000;
		else					/* up only */
			result &= ~0x3000;
	}
	else if (fake & 0x02)		/* down */
	{
		if (fake & 0x04)		/* down and left */
			result &= ~0x8000;
		else if (fake & 0x08)	/* down and right */
			result &= ~0x4000;
		else					/* down only */
			result &= ~0xc000;
	}
	else if (fake & 0x04)		/* left only */
		result &= ~0x6000;
	else if (fake & 0x08)		/* right only */
		result &= ~0x9000;

	return result;
}


static READ_HANDLER( special_input_r )
{
	int result = 0;

	switch (offset & 0x10)
	{
		case 0x00:
			result = fake_inputs(0, 3);
			break;

		case 0x10:
			result = fake_inputs(1, 4);
			if (atarigen_sound_to_cpu_ready) result ^= 0x0004;
			if (atarigen_cpu_to_sound_ready) result ^= 0x0008;
			result ^= 0x0010;
			break;
	}

	return result;
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress main_readmem[] =
{
	{ 0x000000, 0x09ffff, MRA_ROM },
	{ 0x0e0000, 0x0e0fff, atarigen_eeprom_r },
	{ 0x260000, 0x26001f, special_input_r },
	{ 0x260020, 0x26002f, input_port_2_r },
	{ 0x260030, 0x260031, atarigen_sound_r },
	{ 0x3e0000, 0x3e0fff, MRA_BANK1 },
	{ 0x3f0000, 0x3f1fff, shared_ram_1_r },
	{ 0x3f2000, 0x3f3fff, shared_ram_2_r },
	{ 0x3f4000, 0x3f4fff, shared_ram_3_r },
	{ 0x3f5000, 0x3f7fff, shared_ram_4_r },
	{ 0xff8000, 0xff9fff, shared_ram_1_r },
	{ 0xffa000, 0xffbfff, shared_ram_2_r },
	{ 0xffc000, 0xffcfff, shared_ram_3_r },
	{ 0xffd000, 0xffffff, shared_ram_4_r },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress main_writemem[] =
{
	{ 0x000000, 0x09ffff, MWA_ROM },
	{ 0x0e0000, 0x0e0fff, atarigen_eeprom_w, &atarigen_eeprom, &atarigen_eeprom_size },
	{ 0x1f0000, 0x1fffff, atarigen_eeprom_enable_w },
	{ 0x2e0000, 0x2e0001, watchdog_reset_w },
	{ 0x360000, 0x360001, atarigen_video_int_ack_w },
	{ 0x360010, 0x360011, MWA_NOP },
	{ 0x360020, 0x360021, atarigen_sound_reset_w },
	{ 0x360030, 0x360031, atarigen_sound_w },
	{ 0x3e0000, 0x3e0fff, vindictr_paletteram_w, &paletteram },
	{ 0x3f0000, 0x3f1fff, vindictr_playfieldram_w, &atarigen_playfieldram, &atarigen_playfieldram_size },
	{ 0x3f2000, 0x3f3fff, shared_ram_2_w, &atarigen_spriteram, &atarigen_spriteram_size },
	{ 0x3f4000, 0x3f4fff, shared_ram_3_w, &atarigen_alpharam, &atarigen_alpharam_size },
	{ 0x3f5000, 0x3f7fff, shared_ram_4_w, &shared_ram_4 },
	{ 0xff8000, 0xff9fff, vindictr_playfieldram_w },
	{ 0xffa000, 0xffbfff, shared_ram_2_w },
	{ 0xffc000, 0xffcfff, shared_ram_3_w },
	{ 0xffd000, 0xffffff, shared_ram_4_w },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( vindictr )
	PORT_START		/* 26000 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP    | IPF_PLAYER1 | IPF_2WAY )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP   | IPF_PLAYER1 | IPF_2WAY )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN  | IPF_PLAYER1 | IPF_2WAY )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_PLAYER1 | IPF_2WAY )

	PORT_START		/* 26010 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_SERVICE( 0x0002, IP_ACTIVE_LOW )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x00e0, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP    | IPF_PLAYER2 | IPF_2WAY )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP   | IPF_PLAYER2 | IPF_2WAY )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN  | IPF_PLAYER2 | IPF_2WAY )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN | IPF_PLAYER2 | IPF_2WAY )

	PORT_START		/* 26020 */
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0xfc00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START		/* single joystick */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )

	PORT_START		/* single joystick */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )

	JSA_I_PORT		/* audio port */
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout anlayout =
{
	8,8,	/* 8*8 chars */
	1024,	/* 1024 chars */
	2,		/* 2 bits per pixel */
	{ 0, 4 },
	{ 0, 1, 2, 3, 8, 9, 10, 11 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	8*16	/* every char takes 16 consecutive bytes */
};


static struct GfxLayout pfmolayout =
{
	8,8,	/* 8*8 sprites */
	32768,	/* 32768 of them */
	4,		/* 4 bits per pixel */
	{ 0*8*0x40000, 1*8*0x40000, 2*8*0x40000, 3*8*0x40000 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every sprite takes 8 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &pfmolayout,  256, 32 },		/* sprites & playfield */
	{ REGION_GFX2, 0, &anlayout,      0, 64 },		/* characters 8x8 */
	{ -1 } /* end of array */
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_vindictr =
{
	/* basic machine hardware */
	{
		{
			CPU_M68010,		/* verified */
			ATARI_CLOCK_14MHz/2,
			main_readmem,main_writemem,0,0,
			atarigen_video_int_gen,1
		},
		JSA_I_CPU
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	init_machine,

	/* video hardware */
	42*8, 30*8, { 0*8, 42*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	vindictr_vh_start,
	vindictr_vh_stop,
	vindictr_vh_screenrefresh,

	/* sound hardware */
	JSA_I_STEREO_WITH_POKEY,

	atarigen_nvram_handler
};



/*************************************
 *
 *	ROM definition(s)
 *
 *************************************/

ROM_START( vindictr )
	ROM_REGION( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "vin.d1", 0x00000, 0x10000, 0x2e5135e4 )
	ROM_LOAD_ODD ( "vin.d3", 0x00000, 0x10000, 0xe357fa79 )
	ROM_LOAD_EVEN( "vin.j1", 0x20000, 0x10000, 0x44c77ee0 )
	ROM_LOAD_ODD ( "vin.j3", 0x20000, 0x10000, 0x4deaa77f )
	ROM_LOAD_EVEN( "vin.k1", 0x40000, 0x10000, 0x9a0444ee )
	ROM_LOAD_ODD ( "vin.k3", 0x40000, 0x10000, 0xd5022d78 )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k + 16k for 6502 code */
	ROM_LOAD( "vin.snd",     0x10000, 0x4000, 0xd2212c0a )
	ROM_CONTINUE(            0x04000, 0xc000 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vin.p13",     0x00000, 0x20000, 0x062f8e52 )
	ROM_LOAD( "vin.p14",     0x20000, 0x10000, 0x0e4366fa )
	ROM_RELOAD(              0x30000, 0x10000 )
	ROM_LOAD( "vin.p8",      0x40000, 0x20000, 0x09123b57 )
	ROM_LOAD( "vin.p6",      0x60000, 0x10000, 0x6b757bca )
	ROM_RELOAD(              0x70000, 0x10000 )
	ROM_LOAD( "vin.r13",     0x80000, 0x20000, 0xa5268c4f )
	ROM_LOAD( "vin.r14",     0xa0000, 0x10000, 0x609f619e )
	ROM_RELOAD(              0xb0000, 0x10000 )
	ROM_LOAD( "vin.r8",      0xc0000, 0x20000, 0x2d07fdaa )
	ROM_LOAD( "vin.r6",      0xe0000, 0x10000, 0x0a2aba63 )
	ROM_RELOAD(              0xf0000, 0x10000 )

	ROM_REGION( 0x04000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vin.n17",     0x00000, 0x04000, 0xf99b631a )        /* alpha font */
ROM_END



/*************************************
 *
 *	Driver initialization
 *
 *************************************/

static void rom_decode(void)
{
	int i;

	/* invert the graphics bits on the playfield and motion objects */
	for (i = 0; i < memory_region_length(REGION_GFX1); i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;
}

static void init_vindictr(void)
{
	atarigen_eeprom_default = NULL;

	atarijsa_init(1, 5, 1, 0x0002);

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x4150, 0x4168);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}



GAME( 1988, vindictr, 0, vindictr, vindictr, vindictr, ROT0, "Atari Games", "Vindicators" )
