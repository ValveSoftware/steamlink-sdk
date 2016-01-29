#include "../machine/exctsccr.cpp"
#include "../vidhrdw/exctsccr.cpp"

/***************************************************************************

Exciting Soccer - (c) 1983 Alpha Denshi Co.

Supported sets:
Exciting Soccer - Alpha Denshi
Exciting Soccer (bootleg) - Kazutomi


Preliminary driver by:
Ernesto Corvi
ernesto@imagina.com

Jarek Parchanski
jpdev@friko6.onet.pl


NOTES:
The game supports Coin 2, but the dip switches used for it are the same
as Coin 1. Basically, this allowed to select an alternative coin table
based on wich Coin input was connected.

KNOWN ISSUES/TODO:
- Cocktail mode is unsupported.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* from vidhrdw */
extern void exctsccr_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
extern void exctsccr_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( exctsccr_gfx_bank_w );
extern int exctsccr_vh_start( void );
extern void exctsccr_vh_stop( void );

/* from machine */
extern unsigned char *exctsccr_mcu_ram;
WRITE_HANDLER( exctsccr_mcu_w );
WRITE_HANDLER( exctsccr_mcu_control_w );


WRITE_HANDLER( exctsccr_DAC_data_w )
{
	DAC_signed_data_w(offset,data << 2);
}


/***************************************************************************

	Memory definition(s)

***************************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6000, 0x63ff, MRA_RAM }, /* Alpha mcu (protection) */
	{ 0x7c00, 0x7fff, MRA_RAM }, /* work ram */
	{ 0x8000, 0x83ff, videoram_r },
	{ 0x8400, 0x87ff, colorram_r },
	{ 0x8800, 0x8bff, MRA_RAM }, /* ??? */
	{ 0xa000, 0xa000, input_port_0_r },
	{ 0xa040, 0xa040, input_port_1_r },
	{ 0xa080, 0xa080, input_port_3_r },
	{ 0xa0c0, 0xa0c0, input_port_2_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x63ff, exctsccr_mcu_w, &exctsccr_mcu_ram }, /* Alpha mcu (protection) */
	{ 0x7c00, 0x7fff, MWA_RAM }, /* work ram */
	{ 0x8000, 0x83ff, videoram_w, &videoram, &videoram_size },
	{ 0x8400, 0x87ff, colorram_w, &colorram },
	{ 0x8800, 0x8bff, MWA_RAM }, /* ??? */
	{ 0xa000, 0xa000, MWA_NOP }, /* ??? */
	{ 0xa001, 0xa001, MWA_NOP }, /* ??? */
	{ 0xa002, 0xa002, exctsccr_gfx_bank_w },
	{ 0xa003, 0xa003, MWA_NOP }, /* Cocktail mode ( 0xff = flip screen, 0x00 = normal ) */
	{ 0xa006, 0xa006, exctsccr_mcu_control_w }, /* MCU control */
	{ 0xa007, 0xa007, MWA_NOP }, /* This is also MCU control, but i dont need it */
	{ 0xa040, 0xa06f, MWA_RAM, &spriteram }, /* Sprite pos */
	{ 0xa080, 0xa080, soundlatch_w },
	{ 0xa0c0, 0xa0c0, watchdog_reset_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x8fff, MRA_ROM },
	{ 0xa000, 0xa7ff, MRA_RAM },
	{ 0xc00d, 0xc00d, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x8fff, MWA_ROM },
	{ 0xa000, 0xa7ff, MWA_RAM },
	{ 0xc008, 0xc009, exctsccr_DAC_data_w },
	{ 0xc00c, 0xc00c, soundlatch_w }, /* used to clear the latch */
	{ 0xc00f, 0xc00f, MWA_NOP }, /* ??? */
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x82, 0x82, AY8910_write_port_0_w },
	{ 0x83, 0x83, AY8910_control_port_0_w },
	{ 0x86, 0x86, AY8910_write_port_1_w },
	{ 0x87, 0x87, AY8910_control_port_1_w },
	{ 0x8a, 0x8a, AY8910_write_port_2_w },
	{ 0x8b, 0x8b, AY8910_control_port_2_w },
	{ 0x8e, 0x8e, AY8910_write_port_3_w },
	{ 0x8f, 0x8f, AY8910_control_port_3_w },
	{ -1 }	/* end of table */
};

/* Bootleg */
static struct MemoryReadAddress bl_readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x8000, 0x83ff, videoram_r },
	{ 0x8400, 0x87ff, colorram_r },
	{ 0x8800, 0x8fff, MRA_RAM }, /* ??? */
	{ 0xa000, 0xa000, input_port_0_r },
	{ 0xa040, 0xa040, input_port_1_r },
	{ 0xa080, 0xa080, input_port_3_r },
	{ 0xa0c0, 0xa0c0, input_port_2_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress bl_writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x7000, 0x7000, AY8910_write_port_0_w },
	{ 0x7001, 0x7001, AY8910_control_port_0_w },
	{ 0x8000, 0x83ff, videoram_w, &videoram, &videoram_size },
	{ 0x8400, 0x87ff, colorram_w, &colorram },
	{ 0x8800, 0x8fff, MWA_RAM }, /* ??? */
	{ 0xa000, 0xa000, MWA_NOP }, /* ??? */
	{ 0xa001, 0xa001, MWA_NOP }, /* ??? */
	{ 0xa002, 0xa002, exctsccr_gfx_bank_w }, /* ??? */
	{ 0xa003, 0xa003, MWA_NOP }, /* Cocktail mode ( 0xff = flip screen, 0x00 = normal ) */
	{ 0xa006, 0xa006, MWA_NOP }, /* no MCU, but some leftover code still writes here */
	{ 0xa007, 0xa007, MWA_NOP }, /* no MCU, but some leftover code still writes here */
	{ 0xa040, 0xa06f, MWA_RAM, &spriteram }, /* Sprite Pos */
	{ 0xa080, 0xa080, soundlatch_w },
	{ 0xa0c0, 0xa0c0, watchdog_reset_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress bl_sound_readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6000, 0x6000, soundlatch_r },
	{ 0xe000, 0xe3ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress bl_sound_writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x8000, 0x8000, MWA_NOP }, /* 0 = DAC sound off, 1 = DAC sound on */
	{ 0xa000, 0xa000, soundlatch_w }, /* used to clear the latch */
	{ 0xc000, 0xc000, exctsccr_DAC_data_w },
	{ 0xe000, 0xe3ff, MWA_RAM },
	{ -1 }	/* end of table */
};

/***************************************************************************

	Input port(s)

***************************************************************************/

INPUT_PORTS_START( exctsccr )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP   | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT| IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, "A 1C/1C B 3C/1C" )
	PORT_DIPSETTING(    0x01, "A 1C/2C B 1C/4C" )
	PORT_DIPSETTING(    0x00, "A 1C/3C B 1C/6C" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x60, 0x00, "Game Time" )
	PORT_DIPSETTING(    0x20, "1 Min." )
	PORT_DIPSETTING(    0x00, "2 Min." )
	PORT_DIPSETTING(    0x60, "3 Min." )
	PORT_DIPSETTING(    0x40, "4 Min." )
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* Has to be 0 */
INPUT_PORTS_END

/***************************************************************************

	Graphic(s) decoding

***************************************************************************/

static struct GfxLayout charlayout1 =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	3,		/* 3 bits per pixel */
	{ 0x4000*8+4, 0, 4 },	/* plane offset */
	{ 8*8+0, 8*8+1, 8*8+2, 8*8+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout charlayout2 =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	3,		/* 3 bits per pixel */
	{ 0x2000*8, 0, 4 },	/* plane offset */
	{ 8*8+0, 8*8+1, 8*8+2, 8*8+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout1 =
{
	16,16,	    /* 16*16 sprites */
	64,	        /* 64 sprites */
	3,	        /* 3 bits per pixel */
	{ 0x4000*8+4, 0, 4 },	/* plane offset */
	{ 8*8, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 0, 1, 2, 3  },
	{ 0 * 8, 1 * 8, 2 * 8, 3 * 8, 4 * 8, 5 * 8, 6 * 8, 7 * 8,
			32 * 8, 33 * 8, 34 * 8, 35 * 8, 36 * 8, 37 * 8, 38 * 8, 39 * 8 },
	64*8	/* every sprite takes 64 bytes */
};

static struct GfxLayout spritelayout2 =
{
	16,16,	    /* 16*16 sprites */
	64,         /* 64 sprites */
	3,	        /* 3 bits per pixel */
	{ 0x2000*8, 0, 4 },	/* plane offset */
	{ 8*8, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 0, 1, 2, 3  },
	{ 0 * 8, 1 * 8, 2 * 8, 3 * 8, 4 * 8, 5 * 8, 6 * 8, 7 * 8,
			32 * 8, 33 * 8, 34 * 8, 35 * 8, 36 * 8, 37 * 8, 38 * 8, 39 * 8 },
	64*8	/* every sprite takes 64 bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,		/* 16*16 sprites */
	64,	    	/* 64 sprites */
	3,	    	/* 2 bits per pixel */
	{ 0x1000*8+4, 0, 4 },	/* plane offset */
	{ 8*8, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 0, 1, 2, 3  },
	{ 0 * 8, 1 * 8, 2 * 8, 3 * 8, 4 * 8, 5 * 8, 6 * 8, 7 * 8,
			32 * 8, 33 * 8, 34 * 8, 35 * 8, 36 * 8, 37 * 8, 38 * 8, 39 * 8 },
	64*8	/* every sprite takes 64 bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout1,      0, 32 }, /* chars */
	{ REGION_GFX1, 0x2000, &charlayout2,      0, 32 }, /* chars */
	{ REGION_GFX1, 0x1000, &spritelayout1, 16*8, 32 }, /* sprites */
	{ REGION_GFX1, 0x3000, &spritelayout2, 16*8, 32 }, /* sprites */
	{ REGION_GFX2, 0x0000, &spritelayout,  16*8, 32 }, /* sprites */
	{ -1 } /* end of array */
};

/***************************************************************************

	Sound interface(s)

***************************************************************************/

static struct AY8910interface ay8910_interface =
{
	4,	/* 4 chips */
	1500000,	/* 1.5 MHz ? */
	{ 15, 15, 15, 15 }, /* volume */
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 }, /* it writes 0s thru port A, no clue what for */
	{ 0, 0, 0, 0 }
};

static struct DACinterface dac_interface =
{
	2,
	{ 50, 50 }
};

/* Bootleg */
static struct AY8910interface bl_ay8910_interface =
{
	1,	/* 1 chip */
	1500000,	/* 1.5 MHz ? */
	{ 50 }, /* volume */
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct DACinterface bl_dac_interface =
{
	1,
	{ 100 }
};

/***************************************************************************

	Machine driver(s)

***************************************************************************/

static struct MachineDriver machine_driver_exctsccr =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4.0 Mhz (?) */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80,
			4123456,	/* ??? with 4 MHz, nested NMIs might happen */
			sound_readmem,sound_writemem,0,sound_writeport,
			ignore_interrupt,0,
			nmi_interrupt, 4000 /* 4 khz, updates the dac */
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION, /* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32, 64*8,
	exctsccr_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	exctsccr_vh_start,
	exctsccr_vh_stop,
	exctsccr_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

/* Bootleg */
static struct MachineDriver machine_driver_exctsccb =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4.0 Mhz (?) */
			bl_readmem,bl_writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz ? */
			bl_sound_readmem,bl_sound_writemem,0,0,
			ignore_interrupt,0
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION, /* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32, 64*8,
	exctsccr_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	exctsccr_vh_stop,
	exctsccr_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0,
	{
		{
			SOUND_AY8910,
			&bl_ay8910_interface
		},
		{
			SOUND_DAC,
			&bl_dac_interface
		}
	}
};

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( exctsccr )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "1_g10.bin",    0x0000, 0x2000, 0xaa68df66 )
	ROM_LOAD( "2_h10.bin",    0x2000, 0x2000, 0x2d8f8326 )
	ROM_LOAD( "3_j10.bin",    0x4000, 0x2000, 0xdce4a04d )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for code */
	ROM_LOAD( "0_h6.bin",     0x0000, 0x2000, 0x3babbd6b )
	ROM_LOAD( "9_f6.bin",     0x2000, 0x2000, 0x639998f5 )
	ROM_LOAD( "8_d6.bin",     0x4000, 0x2000, 0x88651ee1 )
	ROM_LOAD( "7_c6.bin",     0x6000, 0x2000, 0x6d51521e )
	ROM_LOAD( "1_a6.bin",     0x8000, 0x1000, 0x20f2207e )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "4_a5.bin",     0x0000, 0x2000, 0xc342229b )
	ROM_LOAD( "5_b5.bin",     0x2000, 0x2000, 0x35f4f8c9 )
	ROM_LOAD( "6_c5.bin",     0x4000, 0x2000, 0xeda40e32 )

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "2_k5.bin",     0x0000, 0x1000, 0x7f9cace2 )
	ROM_LOAD( "3_l5.bin",     0x1000, 0x1000, 0xdb2d9e0d )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "prom1.e1",     0x0000, 0x0020, 0xd9b10bf0 ) /* palette */
	ROM_LOAD( "prom2.8r",     0x0020, 0x0100, 0x8a9c0edf ) /* lookup table */
	ROM_LOAD( "prom3.k5",     0x0120, 0x0100, 0xb5db1c2c ) /* lookup table */
ROM_END

ROM_START( exctscca )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "1_g10.bin",    0x0000, 0x2000, 0xaa68df66 )
	ROM_LOAD( "2_h10.bin",    0x2000, 0x2000, 0x2d8f8326 )
	ROM_LOAD( "3_j10.bin",    0x4000, 0x2000, 0xdce4a04d )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for code */
	ROM_LOAD( "exctsccc.000", 0x0000, 0x2000, 0x642fc42f )
	ROM_LOAD( "exctsccc.009", 0x2000, 0x2000, 0xd88b3236 )
	ROM_LOAD( "8_d6.bin",     0x4000, 0x2000, 0x88651ee1 )
	ROM_LOAD( "7_c6.bin",     0x6000, 0x2000, 0x6d51521e )
	ROM_LOAD( "1_a6.bin",     0x8000, 0x1000, 0x20f2207e )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "4_a5.bin",     0x0000, 0x2000, 0xc342229b )
	ROM_LOAD( "5_b5.bin",     0x2000, 0x2000, 0x35f4f8c9 )
	ROM_LOAD( "6_c5.bin",     0x4000, 0x2000, 0xeda40e32 )

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "2_k5.bin",     0x0000, 0x1000, 0x7f9cace2 )
	ROM_LOAD( "3_l5.bin",     0x1000, 0x1000, 0xdb2d9e0d )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "prom1.e1",     0x0000, 0x0020, 0xd9b10bf0 ) /* palette */
	ROM_LOAD( "prom2.8r",     0x0020, 0x0100, 0x8a9c0edf ) /* lookup table */
	ROM_LOAD( "prom3.k5",     0x0120, 0x0100, 0xb5db1c2c ) /* lookup table */
ROM_END

/* Bootleg */
ROM_START( exctsccb )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "es-1.e2",      0x0000, 0x2000, 0x997c6a82 )
	ROM_LOAD( "es-2.g2",      0x2000, 0x2000, 0x5c66e792 )
	ROM_LOAD( "es-3.h2",      0x4000, 0x2000, 0xe0d504c0 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* sound */
	ROM_LOAD( "es-a.k2",      0x0000, 0x2000, 0x99e87b78 )
	ROM_LOAD( "es-b.l2",      0x2000, 0x2000, 0x8b3db794 )
	ROM_LOAD( "es-c.m2",      0x4000, 0x2000, 0x7bed2f81 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	/* I'm using the ROMs from exctscc2, national flags would be wrong otherwise */
	ROM_LOAD( "vr.5a",        0x0000, 0x2000, BADCRC( 0x4ff1783d ) )
	ROM_LOAD( "vr.5b",        0x2000, 0x2000, BADCRC( 0x5605b60b ) )
	ROM_LOAD( "vr.5c",        0x4000, 0x2000, BADCRC( 0x1fb84ee6 ) )

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vr.5k",        0x0000, 0x1000, BADCRC( 0x1d37edfa ) )
	ROM_LOAD( "vr.5l",        0x1000, 0x1000, BADCRC( 0xb97f396c ) )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "prom1.e1",     0x0000, 0x0020, 0xd9b10bf0 ) /* palette */
	ROM_LOAD( "prom2.8r",     0x0020, 0x0100, 0x8a9c0edf ) /* lookup table */
	ROM_LOAD( "prom3.k5",     0x0120, 0x0100, 0xb5db1c2c ) /* lookup table */
ROM_END

ROM_START( exctscc2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "vr.3j",        0x0000, 0x2000, 0xc6115362 )
	ROM_LOAD( "vr.3k",        0x2000, 0x2000, 0xde36ba00 )
	ROM_LOAD( "vr.3l",        0x4000, 0x2000, 0x1ddfdf65 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for code */
	ROM_LOAD( "vr.7d",        0x0000, 0x2000, 0x2c675a43 )
	ROM_LOAD( "vr.7e",        0x2000, 0x2000, 0xe571873d )
	ROM_LOAD( "8_d6.bin",     0x4000, 0x2000, 0x88651ee1 )	/* vr.7f */
	ROM_LOAD( "7_c6.bin",     0x6000, 0x2000, 0x6d51521e )	/* vr.7h */
	ROM_LOAD( "1_a6.bin",     0x8000, 0x1000, 0x20f2207e )	/* vr.7k */

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vr.5a",        0x0000, 0x2000, 0x4ff1783d )
	ROM_LOAD( "vr.5b",        0x2000, 0x2000, 0x5605b60b )
	ROM_LOAD( "vr.5c",        0x4000, 0x2000, 0x1fb84ee6 )

	ROM_REGION( 0x02000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vr.5k",        0x0000, 0x1000, 0x1d37edfa )
	ROM_LOAD( "vr.5l",        0x1000, 0x1000, 0xb97f396c )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "prom1.e1",     0x0000, 0x0020, 0xd9b10bf0 ) /* palette */
	ROM_LOAD( "prom2.8r",     0x0020, 0x0100, 0x8a9c0edf ) /* lookup table */
	ROM_LOAD( "prom3.k5",     0x0120, 0x0100, 0xb5db1c2c ) /* lookup table */
ROM_END



GAMEX( 1983, exctsccr, 0,        exctsccr, exctsccr, 0, ROT90, "Alpha Denshi Co.", "Exciting Soccer", GAME_NO_COCKTAIL )
GAMEX( 1983, exctscca, exctsccr, exctsccr, exctsccr, 0, ROT90, "Alpha Denshi Co.", "Exciting Soccer (alternate music)", GAME_NO_COCKTAIL )
GAMEX( 1984, exctsccb, exctsccr, exctsccb, exctsccr, 0, ROT90, "bootleg", "Exciting Soccer (bootleg)", GAME_NO_COCKTAIL )
GAMEX( 1984, exctscc2, exctsccr, exctsccr, exctsccr, 0, ROT90, "Alpha Denshi Co.", "Exciting Soccer II", GAME_NOT_WORKING | GAME_NO_COCKTAIL )
