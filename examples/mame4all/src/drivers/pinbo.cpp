#include "../vidhrdw/pinbo.cpp"

/***************************************************************************

Pinbo
(c) 1985 Strike

Driver by Scott Kelley (wizard@tripoint.org)


  6502 Memory Map:

    Address Range:    R/W:    Function:
    --------------------------------------------------------------------------
    0000 - 00ff       R/W     Zero Page
    0100 - 01ff       R/W     Stack (0100 - 017f used for other porpoises)
    0200 - 02ff       R/W     Sprite Work Area
    0400 - 07ff       R/W     Screen Character Memory (low 8 bits)
    0800 - 0bff       R/W     Screen Attribute Memory
                                xxxxx111 - Character Palette Entry
                                11111xxx - High 5 bits of character
    1000 - 107f       W       Sprite Hardware Registers
    1800
    1801
    1802
    1803
    1804              R       Player 1 controls
    1805              R       Player 2 controls (cocktail)
    1806
    1807
    2000 - 3fff       R       Screen Building Instructions
    6000 - bfff       R       Program ROM
    e000 - ffff       (R)     (reload of a000-bfff for interrupt vectors)

  Z80 Memory Map:

    Address Range:    R/W:    Function:
    --------------------------------------------------------------------------
    0000 - 1fff       R       Program ROM
    f000 - f1ff       R/W     Memory

  Z80 Port Map:

    Port Range:       R/W:    Function:
    --------------------------------------------------------------------------
    00                W       AY8910 #1 Control Port
    01                W       AY8910 #1 Write Port
    02                R       AY8910 #1 Read Port
    04                W       AY8910 #2 Control Port
    05                W       AY8910 #2 Write Port
    06                R       AY8910 #2 Read Port
    08                R/W     SoundFX Communication Port ?
    14                W       AY8910 Reset ? / Sound Processor Ready ?

 Port 1804 & 1805 Bits     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Button 1                  |     |     |  X  |     |     |     |     |     |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Button 2                  |     |     |     |  X  |     |     |     |     |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Joystick Down             |     |     |     |     |  X  |     |     |     |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Joystick Up               |     |     |     |     |     |  X  |     |     |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Joystick Left             |     |     |     |     |     |     |  X  |     |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Joystick Right            |     |     |     |     |     |     |     |  X  |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Unknown                   |  X  |  X  |     |     |     |     |     |     |
 ---------------------------------------------------------------------------


 Port 1806 Bits            |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 2 Coins / 1 Credit        |     | ON  |     |     |     |     |     |     |
 1 Coin  / 1 Credit        |     | OFF |     |     |     |     |     |     |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 99 Lives                  |     |     | ON  | ON  |     |     |     |     |
  3 Lives                  |     |     | ON  | OFF |     |     |     |     |
  2 Lives                  |     |     | OFF | ON  |     |     |     |     |
  1 Life                   |     |     | OFF | OFF |     |     |     |     |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Two player controls       |     |     |     |     |     |     |     | ON  |
 One player control        |     |     |     |     |     |     |     | OFF |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Unknown                   |  X  |     |     |     |  X  |  X  |  X  |     |
 ---------------------------------------------------------------------------

 Port 1807 Bits            |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Start Player 1            |  ON |     |     |     |     |     |     |     |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Start Player 2            |     | ON  |     |     |     |     |     |     |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Coin Left Chute           |     |     | ON  |     |     |     |     |     |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Coin Right Chute          |     |     |     | ON  |     |     |     |     |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Controls Normal           |     |     |     |     |     |     | ON  |     |
 Controls Reversed         |     |     |     |     |     |     | OFF |     |
 --------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
 Unknown                   |     |     |     |     |  X  |  X  |     |  X  |
 ---------------------------------------------------------------------------

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"


void pinbo_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void pinbo_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);



static int pinbo_interrupt(void)
{
	if (cpu_getiloops() != 0)
	{
		/* user asks to insert coin: generate a NMI interrupt. */
		if (readinputport(3) & 0x30)
			return nmi_interrupt();
		else return ignore_interrupt();
	}
	else return interrupt();	/* one IRQ per frame */
}

WRITE_HANDLER( pinbo_sound_command_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt(1,Z80_IRQ_INT);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x0bff, MRA_RAM },
	{ 0x1000, 0x10ff, MRA_RAM },
	{ 0x1804, 0x1804, input_port_0_r },
	{ 0x1805, 0x1805, input_port_1_r },
	{ 0x1806, 0x1806, input_port_2_r },
	{ 0x1807, 0x1807, input_port_3_r },
	{ 0x2000, 0x3fff, MRA_ROM },
	{ 0x6000, 0xbfff, MRA_ROM },
	{ 0xe000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0400, 0x07ff, videoram_w, &videoram, &videoram_size },
	{ 0x0800, 0x0bff, colorram_w, &colorram },
	{ 0x0c00, 0x0c3f, MWA_RAM },	/* could be scroll RAM */
	{ 0x1000, 0x10ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x1800, 0x1800, pinbo_sound_command_w },
	{ 0x2000, 0x3fff, MWA_ROM },
	{ 0x6000, 0xbfff, MWA_ROM },
	{ 0xe000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0xf000, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0xf000, 0xffff, MWA_RAM },
	{ -1 }  /* end of table */
};

static struct IOReadPort sound_readport[] =
{
	{ 0x02, 0x02, AY8910_read_port_0_r },
	{ 0x06, 0x06, AY8910_read_port_1_r },
	{ 0x08, 0x08, soundlatch_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ 0x04, 0x04, AY8910_control_port_1_w },
	{ 0x05, 0x05, AY8910_write_port_1_w },
	{ 0x08, 0x08, MWA_NOP },	/* ??? */
	{ 0x14, 0x14, MWA_NOP },	/* ??? */
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( pinbo )
	PORT_START  /* 1804 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START  /* 1805 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START /* 1806 */
	PORT_DIPNAME( 0x01, 0x00, "Two controls?" )
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
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_BITX( 0,       0x30, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "99", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* 1807 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Controls" )
	PORT_DIPSETTING(    0x02, "Normal" )
	PORT_DIPSETTING(    0x00, "Reversed" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START1 )
INPUT_PORTS_END


INPUT_PORTS_START( pinbos )
	PORT_START  /* 1804 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START  /* 1805 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START /* 1806 */
	PORT_DIPNAME( 0x01, 0x00, "Two controls?" )
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
	PORT_DIPNAME( 0x30, 0x20, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_BITX( 0,       0x30, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "99", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* 1807 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Controls" )
	PORT_DIPSETTING(    0x02, "Normal" )
	PORT_DIPSETTING(    0x00, "Reversed" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_BIT_IMPULSE( 0x10, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START1 )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,
	2048,
	3,
	{ 0*2048*8*8, 1*2048*8*8, 2*2048*8*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static struct GfxLayout spritelayout =
{
	16,16,
	512,
	3,
	{ 0*512*32*8, 1*512*32*8, 2*512*32*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,   0, 16 },
	{ REGION_GFX1, 0x0000, &spritelayout, 0, 16 },
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	2,		/* 2 chips */
	1250000,	/* 1.25 MHz? */
	{ 25, 25 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 }
};



static struct MachineDriver machine_driver_pinbo =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			3072000,	/* ??? */
			readmem,writemem,0,0,
			pinbo_interrupt,2,	/* IRQ = vblank, NMI = coin */
		},
		{
			CPU_Z80,
			3000000,	/* ??? */
			sound_readmem, sound_writemem,
			sound_readport,sound_writeport,
			ignore_interrupt,0	/* triggered by main cpu */
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256, 256,
	pinbo_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	pinbo_vh_screenrefresh,

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

ROM_START( pinbo )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "rom2.b7",     0x2000, 0x2000, 0x9a185338 )
	ROM_LOAD( "rom3.e7",     0x6000, 0x2000, 0x1cd1b3bd )
	ROM_LOAD( "rom4.h7",     0x8000, 0x2000, 0xba043fa7 )
	ROM_LOAD( "rom5.j7",     0xa000, 0x2000, 0xe71046c4 )
	ROM_RELOAD(              0xe000, 0x2000 )

	ROM_REGION( 0x10000, REGION_CPU2 )  /* 64K for sound */
	ROM_LOAD( "rom1.s8",     0x0000, 0x2000, 0xca45a1be )

	ROM_REGION( 0x10000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rom6.a1",     0x0000, 0x4000, 0x74fe8e98 )
	ROM_LOAD( "rom8.c1",     0x4000, 0x4000, 0x5a800fe7 )
	ROM_LOAD( "rom7.d1",     0x8000, 0x4000, 0x327a3c21 )

	ROM_REGION( 0x00300, REGION_PROMS ) /* Color PROMs */
	ROM_LOAD( "red.l10",     0x0000, 0x0100, 0xe6c9ba52 )
	ROM_LOAD( "green.k10",   0x0100, 0x0100, 0x1bf2d335 )
	ROM_LOAD( "blue.n10",    0x0200, 0x0100, 0xe41250ad )
ROM_END

ROM_START( pinbos )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "b4.bin",     0x2000, 0x2000, 0xd9452d4f )
	ROM_LOAD( "b5.bin",     0x6000, 0x2000, 0xf80b204c )
	ROM_LOAD( "b6.bin",     0x8000, 0x2000, 0xae967d83 )
	ROM_LOAD( "b7.bin",     0xa000, 0x2000, 0x7a584b4e )
	ROM_RELOAD(             0xe000, 0x2000 )

	ROM_REGION( 0x10000, REGION_CPU2 )  /* 64K for sound */
	ROM_LOAD( "b8.bin",     0x0000, 0x2000, 0x32d1df14 )

	ROM_REGION( 0x10000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rom6.a1",     0x0000, 0x4000, 0x74fe8e98 )
	ROM_LOAD( "rom8.c1",     0x4000, 0x4000, 0x5a800fe7 )
	ROM_LOAD( "rom7.d1",     0x8000, 0x4000, 0x327a3c21 )

	ROM_REGION( 0x00300, REGION_PROMS ) /* Color PROMs */
	ROM_LOAD( "red.l10",     0x0000, 0x0100, 0xe6c9ba52 )
	ROM_LOAD( "green.k10",   0x0100, 0x0100, 0x1bf2d335 )
	ROM_LOAD( "blue.n10",    0x0200, 0x0100, 0xe41250ad )
ROM_END



GAME( 1984, pinbo,  0,     pinbo, pinbo,  0, ROT90, "Jaleco", "Pinbo" )
GAME( 1984, pinbos, pinbo, pinbo, pinbos, 0, ROT90, "bootleg?", "Pinbo (Strike)" )
