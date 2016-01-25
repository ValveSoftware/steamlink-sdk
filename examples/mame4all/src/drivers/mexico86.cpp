#include "../machine/mexico86.cpp"
#include "../vidhrdw/mexico86.cpp"

/***************************************************************************

Kick & Run - (c) 1987 Taito

Ernesto Corvi
ernesto@imagina.com

Notes:
- 4 players mode is not emulated. THis involves some shared RAM and a subboard.
  There is additional code for a third Z80 in the bootleg version, I don't
  know if it's related or if its just a replacement for the 68705.

- kicknrun does a PS4 STOP ERROR short after boot, but works afterwards.
  PS4 is the mcu.

- kikikai sometimes crashes, might be a synchronization issue

***************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"

/* in machine/mexico86.c */
extern unsigned char *mexico86_protection_ram;
int mexico86_m68705_interrupt(void);
READ_HANDLER( mexico86_68705_portA_r );
WRITE_HANDLER( mexico86_68705_portA_w );
WRITE_HANDLER( mexico86_68705_ddrA_w );
READ_HANDLER( mexico86_68705_portB_r );
WRITE_HANDLER( mexico86_68705_portB_w );
WRITE_HANDLER( mexico86_68705_ddrB_w );

/* in vidhrdw/mexico86.c */
extern unsigned char *mexico86_videoram,*mexico86_objectram;
extern size_t mexico86_objectram_size;
void mexico86_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( mexico86_bankswitch_w );
void mexico86_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void kikikai_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



static unsigned char *shared;

static READ_HANDLER( shared_r )
{
	return shared[offset];
}

static WRITE_HANDLER( shared_w )
{
	shared[offset] = data;
}

/*
$f008 - write
bit 7 = ? (unused?)
bit 6 = ? (unused?)
bit 5 = ? (unused?)
bit 4 = ? (usually set in game)
bit 3 = ? (usually set in game)
bit 2 = sound cpu reset line
bit 1 = microcontroller reset line
bit 0 = ? (unused?)
*/
static WRITE_HANDLER( mexico86_f008_w )
{
	cpu_set_reset_line(1,(data & 4) ? CLEAR_LINE : ASSERT_LINE);
	cpu_set_reset_line(2,(data & 2) ? CLEAR_LINE : ASSERT_LINE);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },	/* banked roms */
	{ 0xc000, 0xe7ff, shared_r },	/* shared with sound cpu */
	{ 0xe800, 0xe8ff, MRA_RAM },	/* protection ram */
	{ 0xe900, 0xefff, MRA_RAM },
	{ 0xf010, 0xf010, input_port_5_r },
	{ 0xf800, 0xffff, MRA_RAM },	/* communication ram - to connect 4 players's subboard */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xe7ff, shared_w, &shared },	/* shared with sound cpu */
	{ 0xc000, 0xcfff, MWA_RAM, &mexico86_videoram },
	{ 0xd500, 0xd7ff, MWA_RAM, &mexico86_objectram, &mexico86_objectram_size },
	{ 0xe800, 0xe8ff, MWA_RAM, &mexico86_protection_ram },	/* shared with mcu */
	{ 0xe900, 0xefff, MWA_RAM },
	{ 0xf000, 0xf000, mexico86_bankswitch_w },	/* program and gfx ROM banks */
	{ 0xf008, 0xf008, mexico86_f008_w },	/* cpu reset lines + other unknown stuff */
	{ 0xf018, 0xf018, MWA_NOP },	// watchdog_reset_w },
	{ 0xf800, 0xffff, MWA_RAM },	/* communication ram */
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xa7ff, shared_r },
	{ 0xa800, 0xbfff, MRA_RAM },
	{ 0xc000, 0xc000, YM2203_status_port_0_r },
	{ 0xc001, 0xc001, YM2203_read_port_0_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0xa7ff, shared_w },
	{ 0xa800, 0xbfff, MWA_RAM },
	{ 0xc000, 0xc000, YM2203_control_port_0_w },
	{ 0xc001, 0xc001, YM2203_write_port_0_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress m68705_readmem[] =
{
	{ 0x0000, 0x0000, mexico86_68705_portA_r },
	{ 0x0001, 0x0001, mexico86_68705_portB_r },
	{ 0x0002, 0x0002, input_port_0_r },	/* COIN */
	{ 0x0010, 0x007f, MRA_RAM },
	{ 0x0080, 0x07ff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress m68705_writemem[] =
{
	{ 0x0000, 0x0000, mexico86_68705_portA_w },
	{ 0x0001, 0x0001, mexico86_68705_portB_w },
	{ 0x0004, 0x0004, mexico86_68705_ddrA_w },
	{ 0x0005, 0x0005, mexico86_68705_ddrB_w },
	{ 0x000a, 0x000a, MWA_NOP },	/* looks like a bug in the code, writes to */
									/* 0x0a (=10dec) instead of 0x10 */
	{ 0x0010, 0x007f, MWA_RAM },
	{ 0x0080, 0x07ff, MWA_ROM },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( mexico86 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE )	/* service 2 */

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	/* When Bit 1 is On, the machine waits a signal from another one */
	/* Seems like if you can join two cabinets, one as master */
	/* and the other as slave, probably to play four players */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x08, "Timer" )
	PORT_DIPSETTING(    0x04, "Slow" )
	PORT_DIPSETTING(    0x08, "Normal" )
	PORT_DIPSETTING(    0x0c, "Fast" )
	PORT_DIPSETTING(    0x00, "Fastest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* The following dip seems to be related with the first one */
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Max Players" )
	PORT_DIPSETTING(    0x80, "2" )
	PORT_DIPSETTING(    0x00, "4" )

	PORT_START
	/* the following is actually service coin 1 */
	PORT_BITX(0x01, IP_ACTIVE_LOW, IPT_SERVICE, "Advance", KEYCODE_F1, IP_JOY_NONE )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( kikikai )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50000 100000" )
	PORT_DIPSETTING(    0x0c, "70000 150000" )
	PORT_DIPSETTING(    0x08, "70000 200000" )
	PORT_DIPSETTING(    0x04, "100000 300000" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Number Match" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,
	4*2048,
	4,
	{ 0x20000*8, 0x20000*8+4, 0, 4 },
	{ 3, 2, 1, 0, 8+3, 8+2, 8+1, 8+0 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   0, 16 },
	{ -1 } /* end of array */
};



static struct YM2203interface ym2203_interface =
{
	1,			/* 1 chip */
	3000000,	/* 3 MHz ??? */
	{ YM2203_VOL(40,40) },
	{ input_port_3_r },
	{ input_port_4_r },
	{ 0 },
	{ 0 }
};



#define MACHINEDRIVER(NAME) 														\
static struct MachineDriver machine_driver_##NAME = 								\
{																					\
	{																				\
		{																			\
			CPU_Z80,																\
			6000000,		/* 6 MHz??? */											\
			readmem,writemem,0,0,													\
			ignore_interrupt,0	/* IRQs are triggered by the 68705 */				\
		},																			\
		{																			\
			CPU_Z80,																\
			6000000,		/* 6 MHz??? */											\
			sound_readmem,sound_writemem,0,0,										\
			interrupt,1																\
		},																			\
		{																			\
			CPU_M68705,																\
			4000000/2,	/* xtal is 4MHz (????) I think it's divided by 2 internally */	\
			m68705_readmem,m68705_writemem,0,0,										\
			mexico86_m68705_interrupt,2												\
		}																			\
	},																				\
	60, DEFAULT_60HZ_VBLANK_DURATION,  /* frames per second, vblank duration */		\
	100,	/* 100 CPU slices per frame - an high value to ensure proper */			\
			/* synchronization of the CPUs */										\
	0,																				\
																					\
	/* video hardware */															\
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },										\
	gfxdecodeinfo,																	\
	256, 256,																		\
	mexico86_vh_convert_color_prom,													\
																					\
	VIDEO_TYPE_RASTER,																\
	0,																				\
	0,																				\
	0,																				\
	NAME##_vh_screenrefresh,														\
																					\
	/* sound hardware */															\
	0,0,0,0,																		\
	{																				\
		{																			\
			SOUND_YM2203,															\
			&ym2203_interface														\
		}																			\
	}																				\
};


MACHINEDRIVER( mexico86 )
MACHINEDRIVER( kikikai )


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( kicknrun )
	ROM_REGION( 0x28000, REGION_CPU1 )	 /* 196k for code */
	ROM_LOAD( "a87-08.bin", 0x00000, 0x08000, 0x715e1b04 ) /* 1st half, main code		 */
	ROM_CONTINUE(           0x20000, 0x08000 )			   /* 2nd half, banked at 0x8000 */
	ROM_LOAD( "a87-07.bin", 0x10000, 0x10000, 0x6cb6ebfe ) /* banked at 0x8000			 */

	ROM_REGION( 0x10000, REGION_CPU2 )	 /* 64k for the audio cpu */
	ROM_LOAD( "a87-06.bin", 0x0000, 0x8000, 0x1625b587 )

	ROM_REGION( 0x0800, REGION_CPU3 )	/* 2k for the microcontroller */
	ROM_LOAD( "knrmcu.bin",   0x0000, 0x0800, BADCRC(0x8e821fa0) )	/* manually crafted from the Mexico '86 one */

	ROM_REGION( 0x40000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a87-05.bin", 0x08000, 0x08000, 0x4eee3a8a )
	ROM_CONTINUE(           0x00000, 0x08000 )
	ROM_LOAD( "a87-04.bin", 0x10000, 0x08000, 0x8b438d20 )
	ROM_RELOAD(             0x18000, 0x08000 )
	ROM_LOAD( "a87-03.bin", 0x28000, 0x08000, 0xf42e8a88 )
	ROM_CONTINUE(           0x20000, 0x08000 )
	ROM_LOAD( "a87-02.bin", 0x30000, 0x08000, 0x64f1a85f )
	ROM_RELOAD(             0x38000, 0x08000 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "a87-10.bin", 0x0000, 0x0100, 0xbe6eb1f0 )
	ROM_LOAD( "a87-12.bin", 0x0100, 0x0100, 0x3e953444 )
	ROM_LOAD( "a87-11.bin", 0x0200, 0x0100, 0x14f6c28d )
ROM_END

ROM_START( mexico86 )
	ROM_REGION( 0x28000, REGION_CPU1 )	 /* 196k for code */
	ROM_LOAD( "2_g.bin",    0x00000, 0x08000, 0x2bbfe0fb ) /* 1st half, main code		 */
	ROM_CONTINUE(           0x20000, 0x08000 )			   /* 2nd half, banked at 0x8000 */
	ROM_LOAD( "1_f.bin",    0x10000, 0x10000, 0x0b93e68e ) /* banked at 0x8000			 */

	ROM_REGION( 0x10000, REGION_CPU2 )	 /* 64k for the audio cpu */
	ROM_LOAD( "a87-06.bin", 0x0000, 0x8000, 0x1625b587 )

	ROM_REGION( 0x0800, REGION_CPU3 )	/* 2k for the microcontroller */
	ROM_LOAD( "68_h.bin",   0x0000, 0x0800, 0xff92f816 )

	ROM_REGION( 0x40000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "4_d.bin",    0x08000, 0x08000, 0x57cfdbca )
	ROM_CONTINUE(           0x00000, 0x08000 )
	ROM_LOAD( "5_c.bin",    0x10000, 0x08000, 0xe42fa143 )
	ROM_RELOAD(             0x18000, 0x08000 )
	ROM_LOAD( "6_b.bin",    0x28000, 0x08000, 0xa4607989 )
	ROM_CONTINUE(           0x20000, 0x08000 )
	ROM_LOAD( "7_a.bin",    0x30000, 0x08000, 0x245036b1 )
	ROM_RELOAD(             0x38000, 0x08000 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "a87-10.bin", 0x0000, 0x0100, 0xbe6eb1f0 )
	ROM_LOAD( "a87-12.bin", 0x0100, 0x0100, 0x3e953444 )
	ROM_LOAD( "a87-11.bin", 0x0200, 0x0100, 0x14f6c28d )
ROM_END

ROM_START( kikikai )
	ROM_REGION( 0x28000, REGION_CPU1 )	 /* 196k for code */
	ROM_LOAD( "a85-17.rom", 0x00000, 0x08000, 0xc141d5ab ) /* 1st half, main code		 */
	ROM_CONTINUE(           0x20000, 0x08000 )			   /* 2nd half, banked at 0x8000 */
	ROM_LOAD( "a85-16.rom", 0x10000, 0x10000, 0x4094d750 ) /* banked at 0x8000			 */

	ROM_REGION( 0x10000, REGION_CPU2 )	 /* 64k for the audio cpu */
	ROM_LOAD( "a85-11.rom", 0x0000, 0x8000, 0xcc3539db )

	ROM_REGION( 0x0800, REGION_CPU3 )	/* 2k for the microcontroller */
	ROM_LOAD( "knightb.uc", 0x0000, 0x0800, 0x3cc2bbe4 )

	ROM_REGION( 0x40000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a85-15.rom", 0x00000, 0x10000, 0xaebc8c32 )
	ROM_LOAD( "a85-14.rom", 0x10000, 0x10000, 0xa9df0453 )
	ROM_LOAD( "a85-13.rom", 0x20000, 0x10000, 0x3eeaf878 )
	ROM_LOAD( "a85-12.rom", 0x30000, 0x10000, 0x91e58067 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "a85-08.rom", 0x0000, 0x0100, 0xd15f61a8 )
	ROM_LOAD( "a85-10.rom", 0x0100, 0x0100, 0x8fc3fa86 )
	ROM_LOAD( "a85-09.rom", 0x0200, 0x0100, 0xb931c94d )
ROM_END



GAME( 1986, kicknrun, 0,        mexico86, mexico86, 0, ROT0, "Taito Corporation", "Kick and Run" )
GAME( 1986, mexico86, kicknrun, mexico86, mexico86, 0, ROT0, "bootleg", "Mexico 86" )
GAMEX(1986, kikikai,  0,        kikikai,  kikikai,  0, ROT90, "Taito Corporation", "KiKi KaiKai", GAME_NOT_WORKING )
