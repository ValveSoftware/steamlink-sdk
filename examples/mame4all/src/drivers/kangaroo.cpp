#include "../vidhrdw/kangaroo.cpp"

/***************************************************************************

Kangaroo (c) Atari Games / Sun Electronics Corp 1982

driver by Ville Laitinen

In test mode, to test sound press 1 and 2 player start simultaneously.
Punch + 1 player start moves to the crosshatch pattern.

To enter test mode in Funky Fish, keep the service coin pressed while
resetting

TODO:
- There is a custom microcontroller on the original Kangaroo board which is
  not emulated. This MIGHT cause some problems, but we don't know of any.

***************************************************************************

Kangaroo memory map

CPU #0

0000-0fff tvg75
1000-1fff tvg76
2000-2fff tvg77
3000-3fff tvg78
4000-4fff tvg79
5000-5fff tvg80
8000-bfff VIDEO RAM (four banks)
c000-cfff tvg83/84 (banked)
d000-dfff tvg85/86 (banked)
e000-e3ff RAM


memory mapped ports:
read:
e400      DSW 0
ec00      IN 0
ed00      IN 1
ee00      IN 2
efxx      (4 bits wide) security chip in. It seems to work like a clock.

write:
e800-e801 low/high byte start address of data in picture ROM for DMA
e802-e803 low/high byte start address in bitmap RAM (where picture is to be
          written) during DMA
e804-e805 picture size for DMA, and DMA start
e806      vertical scroll of playfield
e807      horizontal scroll of playfield
e808      bank select latch
e809      A & B bitmap control latch (A=playfield B=motion)
          bit 5 FLIP A
          bit 4 FLIP B
          bit 3 EN A
          bit 2 EN B
          bit 1 PRI A
          bit 0 PRI B
e80a      color shading latch
ec00      command to sound CPU
ed00      coin counters
efxx      (4 bits wide) security chip out

---------------------------------------------------------------------------
CPU #1 (sound)

0000 0fff tvg81
4000 43ff RAM
6000      command from main CPU

I/O ports:
7000      AY-3-8910 write
8000      AY-3-8910 control
---------------------------------------------------------------------------

interrupts:
(CPU#0) standard IM 1 interrupt mode (rst #38 every vblank)
(CPU#1) same here

***************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"



/* vidhrdw */
extern unsigned char *kangaroo_video_control;
extern unsigned char *kangaroo_bank_select;
extern unsigned char *kangaroo_blitter;
extern unsigned char *kangaroo_scroll;
void kangaroo_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int  kangaroo_vh_start(void);
void kangaroo_vh_stop(void);
void kangaroo_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( kangaroo_blitter_w );
WRITE_HANDLER( kangaroo_videoram_w );
WRITE_HANDLER( kangaroo_video_control_w );
WRITE_HANDLER( kangaroo_bank_select_w );
WRITE_HANDLER( kangaroo_color_mask_w );



static void kangaroo_init_machine(void)
{
	/* I think there is a bug in the startup checks of the game. At the very */
	/* beginning, during the RAM check, it goes one byte too far, and ends up */
	/* trying to write, and re-read, location dfff. To the best of my knowledge, */
	/* that is a ROM address, so the test fails and the code keeps jumping back */
	/* at 0000. */
	/* However, a NMI causes a successful reset. Maybe the hardware generates a */
	/* NMI short after power on, therefore masking the bug? The NMI is generated */
	/* by the MB8841 custom microcontroller, so this could be a way to disguise */
	/* the copy protection. */
	/* Anyway, what I do here is just immediately generate the NMI, so the game */
	/* properly starts. */
	cpu_cause_interrupt(0,Z80_NMI_INT);
}


static UINT8 kangaroo_clock=0;


/* I have no idea what the security chip is nor whether it really does,
   this just seems to do the trick -V-
*/

static READ_HANDLER( kangaroo_sec_chip_r )
{
/*  kangaroo_clock = (kangaroo_clock << 1) + 1; */
  kangaroo_clock++;
  return (kangaroo_clock & 0x0f);
}

static WRITE_HANDLER( kangaroo_sec_chip_w )
{
/*  kangaroo_clock = val & 0x0f; */
}



static WRITE_HANDLER( kangaroo_coin_counter_w )
{
	coin_counter_w(0, data & 1);
	coin_counter_w(1, data & 2);
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0xc000, 0xdfff, MRA_BANK1 },
	{ 0xe000, 0xe3ff, MRA_RAM },
	{ 0xe400, 0xe400, input_port_3_r },
	{ 0xec00, 0xec00, input_port_0_r },
	{ 0xed00, 0xed00, input_port_1_r },
	{ 0xee00, 0xee00, input_port_2_r },
	{ 0xef00, 0xef00, kangaroo_sec_chip_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x8000, 0xbfff, kangaroo_videoram_w },
	{ 0xc000, 0xdfff, MWA_ROM },
	{ 0xe000, 0xe3ff, MWA_RAM },
	{ 0xe800, 0xe805, kangaroo_blitter_w, &kangaroo_blitter },
	{ 0xe806, 0xe807, MWA_RAM, &kangaroo_scroll },
	{ 0xe808, 0xe808, kangaroo_bank_select_w, &kangaroo_bank_select },
	{ 0xe809, 0xe809, kangaroo_video_control_w, &kangaroo_video_control },
	{ 0xe80a, 0xe80a, kangaroo_color_mask_w },
	{ 0xec00, 0xec00, soundlatch_w },
	{ 0xed00, 0xed00, kangaroo_coin_counter_w },
	{ 0xef00, 0xefff, kangaroo_sec_chip_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x0fff, MRA_ROM },
	{ 0x4000, 0x43ff, MRA_RAM },
	{ 0x6000, 0x6000, soundlatch_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_ROM },
	{ 0x4000, 0x43ff, MWA_RAM },
	{ -1 }  /* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x7000, 0x7000, AY8910_write_port_0_w },
	{ 0x8000, 0x8000, AY8910_control_port_0_w },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( fnkyfish )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( kangaroo )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_DIPNAME( 0x20, 0x00, "Music" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "10000 30000" )
	PORT_DIPSETTING(    0x0c, "20000 40000" )
	PORT_DIPSETTING(    0x04, "10000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xf0, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x20, "A 2C/1C B 1C/3C" )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, "A 1C/1C B 1C/2C" )
	PORT_DIPSETTING(    0x40, "A 1C/1C B 1C/3C" )
	PORT_DIPSETTING(    0x50, "A 1C/1C B 1C/4C" )
	PORT_DIPSETTING(    0x60, "A 1C/1C B 1C/5C" )
	PORT_DIPSETTING(    0x70, "A 1C/1C B 1C/6C" )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x90, "A 1C/2C B 1C/4C" )
	PORT_DIPSETTING(    0xa0, "A 1C/2C B 1C/5C" )
	PORT_DIPSETTING(    0xe0, "A 1C/2C B 1C/6C" )
	PORT_DIPSETTING(    0xb0, "A 1C/2C B 1C/10C" )
	PORT_DIPSETTING(    0xc0, "A 1C/2C B 1C/11C" )
	PORT_DIPSETTING(    0xd0, "A 1C/2C B 1C/12C" )
	/* 0xe0 gives A 1/2 B 1/6 */
	PORT_DIPSETTING(    0xf0, DEF_STR( Free_Play ) )
INPUT_PORTS_END



static struct AY8910interface ay8910_interface =
{
	1,  /* 1 chip */
	10000000/8,     /* 1.25 MHz */
	{ 50 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_kangaroo =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			10000000/4, /* 2.5 Mhz */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_16BIT_PORT | CPU_AUDIO_CPU,
			10000000/4, /* 2.5 MHz */
			sound_readmem,sound_writemem,0,sound_writeport,
			interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,   /* frames per second, vblank duration */
	1,  /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	kangaroo_init_machine,

	/* video hardware */
	32*8, 32*8, { 0, 255, 8, 255-8 },
	0,
	24,0,
	kangaroo_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	kangaroo_vh_start,
	kangaroo_vh_stop,
	kangaroo_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( fnkyfish )
	ROM_REGION( 0x14000, REGION_CPU1 ) /* 64k for code + 16k for banked ROMs */
	ROM_LOAD( "tvg_64.0",    0x0000,  0x1000, 0xaf728803 )
	ROM_LOAD( "tvg_65.1",    0x1000,  0x1000, 0x71959e6b )
	ROM_LOAD( "tvg_66.2",    0x2000,  0x1000, 0x5ccf68d4 )
	ROM_LOAD( "tvg_67.3",    0x3000,  0x1000, 0x938ff36f )

	ROM_LOAD( "tvg_69.v0",   0x10000, 0x1000, 0xcd532d0b ) /* graphics ROMs */
	ROM_LOAD( "tvg_71.v2",   0x11000, 0x1000, 0xa59c9713 )
	ROM_LOAD( "tvg_70.v1",   0x12000, 0x1000, 0xfd308ef1 )
	ROM_LOAD( "tvg_72.v3",   0x13000, 0x1000, 0x6ae9b584 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound */
	ROM_LOAD( "tvg_68.8",    0x0000,  0x1000, 0xd36bb2be )
ROM_END

ROM_START( kangaroo )
	ROM_REGION( 0x14000, REGION_CPU1 ) /* 64k for code + 16k for banked ROMs */
	ROM_LOAD( "tvg_75.0",    0x0000,  0x1000, 0x0d18c581 )
	ROM_LOAD( "tvg_76.1",    0x1000,  0x1000, 0x5978d37a )
	ROM_LOAD( "tvg_77.2",    0x2000,  0x1000, 0x522d1097 )
	ROM_LOAD( "tvg_78.3",    0x3000,  0x1000, 0x063da970 )
	ROM_LOAD( "tvg_79.4",    0x4000,  0x1000, 0x9e5cf8ca )
	ROM_LOAD( "tvg_80.5",    0x5000,  0x1000, 0x2fc18049 )

	ROM_LOAD( "tvg_83.v0",   0x10000, 0x1000, 0xc0446ca6 ) /* graphics ROMs */
	ROM_LOAD( "tvg_85.v2",   0x11000, 0x1000, 0x72c52695 )
	ROM_LOAD( "tvg_84.v1",   0x12000, 0x1000, 0xe4cb26c2 )
	ROM_LOAD( "tvg_86.v3",   0x13000, 0x1000, 0x9e6a599f )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound */
	ROM_LOAD( "tvg_81.8",    0x0000,  0x1000, 0xfb449bfd )

	ROM_REGION( 0x0800, REGION_CPU3 )  /* 8k for the MB8841 custom microcontroller (currently not emulated) */
	ROM_LOAD( "tvg_82.12",   0x0000,  0x0800, 0x57766f69 )
ROM_END

ROM_START( kangaroa )
	ROM_REGION( 0x14000, REGION_CPU1 ) /* 64k for code + 16k for banked ROMs */
	ROM_LOAD( "tvg_75.0",    0x0000,  0x1000, 0x0d18c581 )
	ROM_LOAD( "tvg_76.1",    0x1000,  0x1000, 0x5978d37a )
	ROM_LOAD( "tvg_77.2",    0x2000,  0x1000, 0x522d1097 )
	ROM_LOAD( "tvg_78.3",    0x3000,  0x1000, 0x063da970 )
	ROM_LOAD( "tvg79.bin",   0x4000,  0x1000, 0x82a26c7d )
	ROM_LOAD( "tvg80.bin",   0x5000,  0x1000, 0x3dead542 )

	ROM_LOAD( "tvg_83.v0",   0x10000, 0x1000, 0xc0446ca6 ) /* graphics ROMs */
	ROM_LOAD( "tvg_85.v2",   0x11000, 0x1000, 0x72c52695 )
	ROM_LOAD( "tvg_84.v1",   0x12000, 0x1000, 0xe4cb26c2 )
	ROM_LOAD( "tvg_86.v3",   0x13000, 0x1000, 0x9e6a599f )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound */
	ROM_LOAD( "tvg_81.8",    0x0000,  0x1000, 0xfb449bfd )

	ROM_REGION( 0x0800, REGION_CPU3 )  /* 8k for the MB8841 custom microcontroller (currently not emulated) */
	ROM_LOAD( "tvg_82.12",   0x0000,  0x0800, 0x57766f69 )
ROM_END

ROM_START( kangarob )
	ROM_REGION( 0x14000, REGION_CPU1 ) /* 64k for code + 16k for banked ROMs */
	ROM_LOAD( "tvg_75.0",    0x0000,  0x1000, 0x0d18c581 )
	ROM_LOAD( "tvg_76.1",    0x1000,  0x1000, 0x5978d37a )
	ROM_LOAD( "tvg_77.2",    0x2000,  0x1000, 0x522d1097 )
	ROM_LOAD( "tvg_78.3",    0x3000,  0x1000, 0x063da970 )
	ROM_LOAD( "tvg_79.4",    0x4000,  0x1000, 0x9e5cf8ca )
	ROM_LOAD( "k6",          0x5000,  0x1000, 0x7644504a )

	ROM_LOAD( "tvg_83.v0",   0x10000, 0x1000, 0xc0446ca6 ) /* graphics ROMs */
	ROM_LOAD( "tvg_85.v2",   0x11000, 0x1000, 0x72c52695 )
	ROM_LOAD( "tvg_84.v1",   0x12000, 0x1000, 0xe4cb26c2 )
	ROM_LOAD( "tvg_86.v3",   0x13000, 0x1000, 0x9e6a599f )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sound */
	ROM_LOAD( "tvg_81.8",    0x0000,  0x1000, 0xfb449bfd )
ROM_END



GAME( 1981, fnkyfish, 0,        kangaroo, fnkyfish, 0, ROT90, "Sun Electronics", "Funky Fish" )
GAME( 1982, kangaroo, 0,        kangaroo, kangaroo, 0, ROT90, "Sun Electronics", "Kangaroo" )
GAME( 1982, kangaroa, kangaroo, kangaroo, kangaroo, 0, ROT90, "[Sun Electronics] (Atari license)", "Kangaroo (Atari)" )
GAME( 1982, kangarob, kangaroo, kangaroo, kangaroo, 0, ROT90, "bootleg", "Kangaroo (bootleg)" )
