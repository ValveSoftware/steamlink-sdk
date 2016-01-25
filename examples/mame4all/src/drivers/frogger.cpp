#include "../vidhrdw/frogger.cpp"
#include "../sndhrdw/frogger.cpp"

/***************************************************************************

Frogger memory map (preliminary)

0000-3fff ROM
8000-87ff RAM
a800-abff Video RAM
b000-b0ff Object RAM
b000-b03f screen attributes
b040-b05f sprites
b060-b0ff unused?

read:
8800      Watchdog Reset
e000      IN0
e002      IN1
e004      IN2

*
 * IN0 (all bits are inverted)
 * bit 7 : COIN 1
 * bit 6 : COIN 2
 * bit 5 : LEFT player 1
 * bit 4 : RIGHT player 1
 * bit 3 : SHOOT 1 player 1
 * bit 2 : CREDIT
 * bit 1 : SHOOT 2 player 1
 * bit 0 : UP player 2 (TABLE only)
 *
*
 * IN1 (all bits are inverted)
 * bit 7 : START 1
 * bit 6 : START 2
 * bit 5 : LEFT player 2 (TABLE only)
 * bit 4 : RIGHT player 2 (TABLE only)
 * bit 3 : SHOOT 1 player 2 (TABLE only)
 * bit 2 : SHOOT 2 player 2 (TABLE only)
 * bit 1 :\ nr of lives
 * bit 0 :/ 00 = 3  01 = 5  10 = 7  11 = 256
*
 * IN2 (all bits are inverted)
 * bit 7 : unused
 * bit 6 : DOWN player 1
 * bit 5 : unused
 * bit 4 : UP player 1
 * bit 3 : COCKTAIL or UPRIGHT cabinet (0 = UPRIGHT)
 * bit 2 :\ coins per play
 * bit 1 :/
 * bit 0 : DOWN player 2 (TABLE only)
 *

write:
b808      interrupt enable
b80c      screen horizontal flip
b810      screen vertical flip
b818      coin counter 1
b81c      coin counter 2
d000      To AY-3-8910 port A (commands for the second Z80)
d002      trigger interrupt on sound CPU


SOUND BOARD:
0000-17ff ROM
4000-43ff RAM

I/0 ports:
read:
40      8910 #1  read

write
40      8910 #1  write
80      8910 #1  control

interrupts:
interrupt mode 1 triggered by the main CPU

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



extern unsigned char *frogger_attributesram;
void frogger_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( frogger_attributes_w );
WRITE_HANDLER( frogger_flipscreen_w );
void frogger_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void frogger2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

READ_HANDLER( frogger_portB_r );
WRITE_HANDLER( frogger_sh_irqtrigger_w );
WRITE_HANDLER( frogger2_sh_irqtrigger_w );

static WRITE_HANDLER( frogger_counterb_w )
{
	coin_counter_w (1, data);
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x8800, 0x8800, watchdog_reset_r },
	{ 0xa800, 0xabff, MRA_RAM },	/* video RAM */
	{ 0xb000, 0xb05f, MRA_RAM },	/* screen attributes, sprites */
	{ 0xe000, 0xe000, input_port_0_r },	/* IN0 */
	{ 0xe002, 0xe002, input_port_1_r },	/* IN1 */
	{ 0xe004, 0xe004, input_port_2_r },	/* IN2 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xa800, 0xabff, videoram_w, &videoram, &videoram_size },
	{ 0xb000, 0xb03f, frogger_attributes_w, &frogger_attributesram },
	{ 0xb040, 0xb05f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xb808, 0xb808, interrupt_enable_w },
	{ 0xb80c, 0xb80c, frogger_flipscreen_w },
	{ 0xb818, 0xb818, coin_counter_w },
	{ 0xb81c, 0xb81c, frogger_counterb_w },
	{ 0xd000, 0xd000, soundlatch_w },
	{ 0xd002, 0xd002, frogger_sh_irqtrigger_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress froggrmc_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x9000, 0x93ff, MRA_RAM },	/* video RAM */
	{ 0x9800, 0x985f, MRA_RAM },	/* screen attributes, sprites */
	{ 0xa000, 0xa000, input_port_0_r },	/* IN0 */
	{ 0xa800, 0xa800, input_port_1_r },	/* IN1 */
	{ 0xb000, 0xb000, input_port_2_r },	/* IN2 */
	{ 0xb800, 0xb800, watchdog_reset_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress froggrmc_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x9000, 0x93ff, videoram_w, &videoram, &videoram_size },
	{ 0x9800, 0x983f, frogger_attributes_w, &frogger_attributesram },
	{ 0x9840, 0x985f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xa800, 0xa800, soundlatch_w },
	{ 0xb000, 0xb000, interrupt_enable_w },
	{ 0xb001, 0xb001, frogger2_sh_irqtrigger_w },
	{ 0xb006, 0xb006, frogger_flipscreen_w },
	{ -1 }	/* end of table */
};



static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x17ff, MRA_ROM },
	{ 0x4000, 0x43ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x17ff, MWA_ROM },
	{ 0x4000, 0x43ff, MWA_RAM },
	{ -1 }	/* end of table */
};



static struct IOReadPort sound_readport[] =
{
	{ 0x40, 0x40, AY8910_read_port_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x80, 0x80, AY8910_control_port_0_w },
	{ 0x40, 0x40, AY8910_write_port_0_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( frogger )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* 1P shoot2 - unused */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* 1P shoot1 - unused */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* IN1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x02, "7" )
	PORT_BITX( 0,       0x03, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "256", IP_KEY_NONE, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* 2P shoot2 - unused */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* 2P shoot1 - unused */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, "A 2/1 B 2/1 C 2/1" )
	PORT_DIPSETTING(    0x04, "A 2/1 B 1/3 C 2/1" )
	PORT_DIPSETTING(    0x00, "A 1/1 B 1/1 C 1/1" )
	PORT_DIPSETTING(    0x06, "A 1/1 B 1/6 C 1/1" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( froggrmc )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN3 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x80, "5" )
	PORT_DIPSETTING(    0x40, "7" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "256", IP_KEY_NONE, IP_JOY_NONE )

	PORT_START	/* IN2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, "A 2/1 B 2/1 C 2/1" )
	PORT_DIPSETTING(    0x04, "A 2/1 B 1/3 C 2/1" )
	PORT_DIPSETTING(    0x06, "A 1/1 B 1/1 C 1/1" )
	PORT_DIPSETTING(    0x00, "A 1/1 B 1/6 C 1/1" )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 256*8*8, 0 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 64*16*16, 0 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,     0, 16 },
	{ REGION_GFX1, 0, &spritelayout,   0,  8 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	1,	/* 1 chip */
	14318000/8,	/* 1.78975 Mhz */
	{ MIXERG(80,MIXER_GAIN_2x,MIXER_PAN_CENTER) },
	{ soundlatch_r },
	{ frogger_portB_r },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_frogger =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			readmem,writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32,64,
	frogger_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	frogger_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

static struct MachineDriver machine_driver_froggrmc =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			froggrmc_readmem,froggrmc_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32,64,
	frogger_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	frogger2_vh_screenrefresh,

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


ROM_START( frogger )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "frogger.26",   0x0000, 0x1000, 0x597696d6 )
	ROM_LOAD( "frogger.27",   0x1000, 0x1000, 0xb6e6fcc3 )
	ROM_LOAD( "frsm3.7",      0x2000, 0x1000, 0xaca22ae0 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "frogger.608",  0x0000, 0x0800, 0xe8ab0256 )
	ROM_LOAD( "frogger.609",  0x0800, 0x0800, 0x7380a48f )
	ROM_LOAD( "frogger.610",  0x1000, 0x0800, 0x31d7eb27 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "frogger.606",  0x0000, 0x0800, 0xf524ee30 )
	ROM_LOAD( "frogger.607",  0x0800, 0x0800, 0x05f7d883 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "pr-91.6l",     0x0000, 0x0020, 0x413703bf )
ROM_END

ROM_START( frogseg1 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "frogger.26",   0x0000, 0x1000, 0x597696d6 )
	ROM_LOAD( "frogger.27",   0x1000, 0x1000, 0xb6e6fcc3 )
	ROM_LOAD( "frogger.34",   0x2000, 0x1000, 0xed866bab )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "frogger.608",  0x0000, 0x0800, 0xe8ab0256 )
	ROM_LOAD( "frogger.609",  0x0800, 0x0800, 0x7380a48f )
	ROM_LOAD( "frogger.610",  0x1000, 0x0800, 0x31d7eb27 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "frogger.606",  0x0000, 0x0800, 0xf524ee30 )
	ROM_LOAD( "frogger.607",  0x0800, 0x0800, 0x05f7d883 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "pr-91.6l",     0x0000, 0x0020, 0x413703bf )
ROM_END

ROM_START( frogseg2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "frogger.ic5",  0x0000, 0x1000, 0xefab0c79 )
	ROM_LOAD( "frogger.ic6",  0x1000, 0x1000, 0xaeca9c13 )
	ROM_LOAD( "frogger.ic7",  0x2000, 0x1000, 0xdd251066 )
	ROM_LOAD( "frogger.ic8",  0x3000, 0x1000, 0xbf293a02 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "frogger.608",  0x0000, 0x0800, 0xe8ab0256 )
	ROM_LOAD( "frogger.609",  0x0800, 0x0800, 0x7380a48f )
	ROM_LOAD( "frogger.610",  0x1000, 0x0800, 0x31d7eb27 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "frogger.606",  0x0000, 0x0800, 0xf524ee30 )
	ROM_LOAD( "frogger.607",  0x0800, 0x0800, 0x05f7d883 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "pr-91.6l",     0x0000, 0x0020, 0x413703bf )
ROM_END

ROM_START( froggrmc )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "epr-1031.15",  0x0000, 0x1000, 0x4b7c8d11 )
	ROM_LOAD( "epr-1032.16",  0x1000, 0x1000, 0xac00b9d9 )
	ROM_LOAD( "epr-1033.33",  0x2000, 0x1000, 0xbc1d6fbc )
	ROM_LOAD( "epr-1034.34",  0x3000, 0x1000, 0x9efe7399 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "epr-1082.42",  0x0000, 0x1000, 0x802843c2 )
	ROM_LOAD( "epr-1035.43",  0x1000, 0x0800, 0x14e74148 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "epr-1036.1k",  0x0000, 0x0800, 0x658745f8 )
	ROM_LOAD( "frogger.607",  0x0800, 0x0800, 0x05f7d883 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "pr-91.6l",     0x0000, 0x0020, 0x413703bf )
ROM_END



static void init_frogger(void)
{
	int A;
	unsigned char *RAM;


	/* the first ROM of the second CPU has data lines D0 and D1 swapped. Decode it. */
	RAM = memory_region(REGION_CPU2);
	for (A = 0;A < 0x0800;A++)
		RAM[A] = (RAM[A] & 0xfc) | ((RAM[A] & 1) << 1) | ((RAM[A] & 2) >> 1);

	/* likewise, the first gfx ROM has data lines D0 and D1 swapped. Decode it. */
	RAM = memory_region(REGION_GFX1);
	for (A = 0;A < 0x0800;A++)
		RAM[A] = (RAM[A] & 0xfc) | ((RAM[A] & 1) << 1) | ((RAM[A] & 2) >> 1);
}

static void init_froggrmc(void)
{
	int A;
	unsigned char *RAM;


	/* the first ROM of the second CPU has data lines D0 and D1 swapped. Decode it. */
	RAM = memory_region(REGION_CPU2);
	for (A = 0;A < 0x1000;A++)
		RAM[A] = (RAM[A] & 0xfc) | ((RAM[A] & 1) << 1) | ((RAM[A] & 2) >> 1);
}



GAME( 1981, frogger,  0,       frogger,  frogger,  frogger,  ROT90, "Konami", "Frogger" )
GAME( 1981, frogseg1, frogger, frogger,  frogger,  frogger,  ROT90, "[Konami] (Sega license)", "Frogger (Sega set 1)" )
GAME( 1981, frogseg2, frogger, frogger,  frogger,  frogger,  ROT90, "[Konami] (Sega license)", "Frogger (Sega set 2)" )

/* this version runs on modified Moon Cresta hardware */
GAME( 1981, froggrmc, frogger, froggrmc, froggrmc, froggrmc, ROT90, "bootleg?", "Frogger (modified Moon Cresta hardware)" )
