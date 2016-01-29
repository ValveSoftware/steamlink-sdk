#include "../vidhrdw/espial.cpp"

/***************************************************************************

Espial memory map (preliminary)

MAIN CPU:

0000-4fff ROM
5800-5fff RAM
8000-800f sprites code (bits 1-7) and double height (bit 0)
8010-801f sprites X
8400-87ff Video RAM
8800-880f sprites flip x (bit 2) and flip y (bit 3)
8c00-8fff Attribute RAM
9000-900f sprites Y
9010-901f sprites color
9020-903f column scroll
9400-97ff Color RAM
c000-cfff ROM

read:
6081      IN0
6082      IN1
6083      IN2
6084      IN3
6090      read command back from sound CPU
7000      ?

write:
6081      ? - written to twice when the text during the self-test is drawn onscreen
6090      write command to sound CPU
7000      watchdog reset
7100      NMI interrupt acknowledge/enable
7200      flip screen

Interrupts: VBlank -> NMI.
			IRQ -> send sound commands to sound cpu. Runs in interrupt mode 1

SOUND CPU:
0000-1fff ROM
2000-23ff RAM

read:
6000      read command from main CPU

write:
4000      NMI enable
6000      write command back to main CPU

Interrupts: IRQs are triggered by writes to the sound_command location 0x6000 - im 1
			NMIs occur regularly to process and play the sounds

I/0 ports:
write
00        8910  control
01        8910  write
A ?
B ?

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"



extern unsigned char *espial_attributeram;
extern unsigned char *espial_column_scroll;
WRITE_HANDLER( espial_attributeram_w );
void espial_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void espial_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


void espial_init_machine(void)
{
	/* we must start with NMI interrupts disabled */
	//interrupt_enable = 0;
	interrupt_enable_w(0, 0);
}


WRITE_HANDLER( zodiac_master_interrupt_enable_w )
{
	interrupt_enable_w(offset, data ^ 1);
}


int zodiac_master_interrupt(void)
{
	return (cpu_getiloops() == 0) ? nmi_interrupt() : interrupt();
}


WRITE_HANDLER( zodiac_master_soundlatch_w )
{
	soundlatch_w(offset, data);
	cpu_cause_interrupt(1, Z80_IRQ_INT);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x4fff, MRA_ROM },
	{ 0x5800, 0x5fff, MRA_RAM },
	{ 0x7000, 0x7000, MRA_RAM },	/* ?? */
	{ 0x8000, 0x803f, MRA_RAM },
	{ 0x8400, 0x87ff, MRA_RAM },
	{ 0x8c00, 0x903f, MRA_RAM },
	{ 0x9400, 0x97ff, MRA_RAM },
	{ 0x6081, 0x6081, input_port_0_r },	/* IN0 */
	{ 0x6082, 0x6082, input_port_1_r },	/* IN1 */
	{ 0x6083, 0x6083, input_port_2_r },	/* IN2 */
	{ 0x6084, 0x6084, input_port_3_r },	/* IN3 */
	{ 0x6090, 0x6090, soundlatch_r },	/* the main CPU reads the command back from the slave */
	{ 0xc000, 0xcfff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x4fff, MWA_ROM },
	{ 0x5800, 0x5fff, MWA_RAM },
	{ 0x6090, 0x6090, zodiac_master_soundlatch_w },
	{ 0x7000, 0x7000, watchdog_reset_w },
	{ 0x7100, 0x7100, zodiac_master_interrupt_enable_w },
	{ 0x8000, 0x801f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x8400, 0x87ff, videoram_w, &videoram, &videoram_size },
	{ 0x8800, 0x880f, MWA_RAM, &spriteram_3 },
	{ 0x8c00, 0x8fff, espial_attributeram_w, &espial_attributeram },
	{ 0x9000, 0x901f, MWA_RAM, &spriteram_2 },
	{ 0x9020, 0x903f, MWA_RAM, &espial_column_scroll },
	{ 0x9400, 0x97ff, colorram_w, &colorram },
	{ 0xc000, 0xcfff, MWA_ROM },
	{ -1 }	/* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x23ff, MRA_RAM },
	{ 0x6000, 0x6000, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x2000, 0x23ff, MWA_RAM },
	{ 0x4000, 0x4000, interrupt_enable_w },
	{ 0x6000, 0x6000, soundlatch_w },
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( espial )
	PORT_START	/* IN0 */
	PORT_DIPNAME( 0x01, 0x00, "Fire Buttons" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x02, 0x02, "CounterAttack" )	/* you can shoot bullets */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x1c, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x00, "20k and every 70k" )
	PORT_DIPSETTING(	0x20, "50k and every 100k" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, "Test Mode" )	/* ??? */
	PORT_DIPSETTING(	0x00, "Normal" )
	PORT_DIPSETTING(	0x80, "Test" )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )

	PORT_START	/* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	768,	/* 768 characters */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },	/* bits are packed in groups of four */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	128,	/* 128 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 128*32*8 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,    0, 64 },
	{ REGION_GFX2, 0, &spritelayout,  0, 64 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	1,	/* 1 chip */
	1500000,	/* 1.5 MHZ?????? */
	{ 50 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_espial =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz */
			readmem,writemem,0,0,
			zodiac_master_interrupt,2
		},
		{
			CPU_Z80,
			3072000,	/* 2 Mhz?????? */
			sound_readmem,sound_writemem,0,sound_writeport,
			nmi_interrupt,4
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	espial_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256,256,
	espial_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	espial_vh_screenrefresh,

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

ROM_START( espial )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "espial.3",     0x0000, 0x2000, 0x10f1da30 )
	ROM_LOAD( "espial.4",     0x2000, 0x2000, 0xd2adbe39 )
	ROM_LOAD( "espial.6",     0x4000, 0x1000, 0xbaa60bc1 )
	ROM_LOAD( "espial.5",     0xc000, 0x1000, 0x6d7bbfc1 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "espial.1",     0x0000, 0x1000, 0x1e5ec20b )
	ROM_LOAD( "espial.2",     0x1000, 0x1000, 0x3431bb97 )

	ROM_REGION( 0x3000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "espial.8",     0x0000, 0x2000, 0x2f43036f )
	ROM_LOAD( "espial.7",     0x2000, 0x1000, 0xebfef046 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "espial.10",    0x0000, 0x1000, 0xde80fbc1 )
	ROM_LOAD( "espial.9",     0x1000, 0x1000, 0x48c258a0 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "espial.1f",    0x0000, 0x0100, 0xd12de557 ) /* palette low 4 bits */
	ROM_LOAD( "espial.1h",    0x0100, 0x0100, 0x4c84fe70 ) /* palette high 4 bits */
ROM_END

ROM_START( espiale )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "2764.3",       0x0000, 0x2000, 0x0973c8a4 )
	ROM_LOAD( "2764.4",       0x2000, 0x2000, 0x6034d7e5 )
	ROM_LOAD( "2732.6",       0x4000, 0x1000, 0x357025b4 )
	ROM_LOAD( "2732.5",       0xc000, 0x1000, 0xd03a2fc4 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "2732.1",       0x0000, 0x1000, 0xfc7729e9 )
	ROM_LOAD( "2732.2",       0x1000, 0x1000, 0xe4e256da )

	ROM_REGION( 0x3000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "espial.8",     0x0000, 0x2000, 0x2f43036f )
	ROM_LOAD( "espial.7",     0x2000, 0x1000, 0xebfef046 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "espial.10",    0x0000, 0x1000, 0xde80fbc1 )
	ROM_LOAD( "espial.9",     0x1000, 0x1000, 0x48c258a0 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "espial.1f",    0x0000, 0x0100, 0xd12de557 ) /* palette low 4 bits */
	ROM_LOAD( "espial.1h",    0x0100, 0x0100, 0x4c84fe70 ) /* palette high 4 bits */
ROM_END



GAMEX( 1983, espial,  0,      espial, espial, 0, ROT0, "[Orca] Thunderbolt", "Espial (US?)", GAME_NO_COCKTAIL )
GAMEX( 1983, espiale, espial, espial, espial, 0, ROT0, "[Orca] Thunderbolt", "Espial (Europe)", GAME_NO_COCKTAIL )
