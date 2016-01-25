#include "../vidhrdw/rockrage.cpp"

/***************************************************************************

Rock'n'Rage(GX620) (c) 1986 Konami

Driver by Manuel Abadia <manu@teleline.es>

***************************************************************************/

#include "driver.h"
#include "cpu/m6809/m6809.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"

extern int rockrage_irq_enable;

/* from vidhrdw */
int rockrage_vh_start(void);
void rockrage_vh_stop(void);
void rockrage_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( rockrage_vreg_w );
void rockrage_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

static int rockrage_interrupt( void )
{
	if (K007342_is_INT_enabled())
        return HD6309_INT_IRQ;
    else
		return ignore_interrupt();
}

static WRITE_HANDLER( rockrage_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);

	/* bits 4-6 = bank number */
	bankaddress = 0x10000 + ((data & 0x70) >> 4) * 0x2000;
	cpu_setbank(1,&RAM[bankaddress]);

	/* bits 0 & 1 = coin counters */
	coin_counter_w(0,data & 0x01);
	coin_counter_w(1,data & 0x02);

	/* other bits unknown */
}

static WRITE_HANDLER( rockrage_sh_irqtrigger_w )
{
	soundlatch_w(offset, data);
	cpu_cause_interrupt(1,M6809_INT_IRQ);
}

static READ_HANDLER( rockrage_VLM5030_busy_r ) {
	return ( VLM5030_BSY() ? 1 : 0 );
}

static WRITE_HANDLER( rockrage_speech_w ) {
	VLM5030_RST( ( data >> 2 ) & 0x01 );
	VLM5030_ST(  ( data >> 0 ) & 0x01 );
	VLM5030_VCU( ( data >> 1 ) & 0x01 );
}

static struct MemoryReadAddress rockrage_readmem[] =
{
	{ 0x0000, 0x1fff, K007342_r },			/* Color RAM + Video RAM */
	{ 0x2000, 0x21ff, K007420_r },			/* Sprite RAM */
	{ 0x2200, 0x23ff, K007342_scroll_r },	/* Scroll RAM */
	{ 0x2400, 0x247f, paletteram_r },		/* Palette */
	{ 0x2e01, 0x2e01, input_port_3_r },		/* 1P controls */
	{ 0x2e02, 0x2e02, input_port_4_r },		/* 2P controls */
	{ 0x2e03, 0x2e03, input_port_1_r },		/* DISPW #2 */
	{ 0x2e40, 0x2e40, input_port_0_r },		/* DIPSW #1 */
	{ 0x2e00, 0x2e00, input_port_2_r },		/* coinsw, testsw, startsw */
	{ 0x4000, 0x5fff, MRA_RAM },			/* RAM */
	{ 0x6000, 0x7fff, MRA_BANK1 },			/* banked ROM */
	{ 0x8000, 0xffff, MRA_ROM },			/* ROM */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress rockrage_writemem[] =
{
	{ 0x0000, 0x1fff, K007342_w },				/* Color RAM + Video RAM */
	{ 0x2000, 0x21ff, K007420_w },				/* Sprite RAM */
	{ 0x2200, 0x23ff, K007342_scroll_w },		/* Scroll RAM */
	{ 0x2400, 0x247f, paletteram_xBBBBBGGGGGRRRRR_w, &paletteram },/* palette */
	{ 0x2600, 0x2607, K007342_vreg_w },			/* Video Registers */
	{ 0x2e80, 0x2e80, rockrage_sh_irqtrigger_w },/* cause interrupt on audio CPU */
	{ 0x2ec0, 0x2ec0, watchdog_reset_w },		/* watchdog reset */
	{ 0x2f00, 0x2f00, rockrage_vreg_w },		/* ??? */
	{ 0x2f40, 0x2f40, rockrage_bankswitch_w },	/* bankswitch control */
	{ 0x4000, 0x5fff, MWA_RAM },				/* RAM */
	{ 0x6000, 0x7fff, MWA_RAM },				/* banked ROM */
	{ 0x8000, 0xffff, MWA_ROM },				/* ROM */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress rockrage_readmem_sound[] =
{
	{ 0x3000, 0x3000, rockrage_VLM5030_busy_r },/* VLM5030 */
	{ 0x5000, 0x5000, soundlatch_r },			/* soundlatch_r */
	{ 0x6001, 0x6001, YM2151_status_port_0_r },	/* YM 2151 */
	{ 0x7000, 0x77ff, MRA_RAM },				/* RAM */
	{ 0x8000, 0xffff, MRA_ROM },				/* ROM */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress rockrage_writemem_sound[] =
{
	{ 0x2000, 0x2000, VLM5030_data_w }, 			/* VLM5030 */
	{ 0x4000, 0x4000, rockrage_speech_w },			/* VLM5030 */
	{ 0x6000, 0x6000, YM2151_register_port_0_w },	/* YM 2151 */
	{ 0x6001, 0x6001, YM2151_data_port_0_w },		/* YM 2151 */
	{ 0x7000, 0x77ff, MWA_RAM },					/* RAM */
	{ 0x8000, 0xffff, MWA_ROM },					/* ROM */
	{ -1 }	/* end of table */
};

/***************************************************************************

	Input Ports

***************************************************************************/

INPUT_PORTS_START( rockrage )
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
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "30k and every 70k" )
	PORT_DIPSETTING(    0x00, "40k and every 80k" )
	PORT_DIPNAME( 0x10, 0x10, "Freeze Screen" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* COINSW */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* PLAYER 1 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* PLAYER 2 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,			/* 8*8 characters */
	0x20000/16,		/* 8192 characters */
	4,				/* 4 bpp */
	{ 0, 1, 2, 3 },	/* the four bitplanes are packed in one nibble */
	{ 0*4, 1*4, 0x20000*8+0*4, 0x20000*8+1*4, 2*4, 3*4, 0x20000*8+2*4, 0x20000*8+3*4 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8		/* every char takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	8,8,			/* 8*8 sprites */
	0x40000/32,	/* 8192 sprites */
	4,				/* 4 bpp */
	{ 0, 1, 2, 3 },	/* the four bitplanes are packed in one nibble */
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8			/* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   64, 32 },	/* colors 00..31, but using 2 lookup tables */
	{ REGION_GFX2, 0, &spritelayout, 32,  1 },	/* colors 32..63 */
	{ -1 } /* end of array */
};

/***************************************************************************

	Machine Driver

***************************************************************************/

static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3579545,	/* 3.579545 MHz */
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) },
	{ 0 },
	{ 0 }
};

static struct VLM5030interface vlm5030_interface =
{
	3579545,	/* 3.579545 MHz */
	60,			/* volume */
	REGION_SOUND1,	/* memory region of speech rom */
	0,
	0
};

static struct MachineDriver machine_driver_rockrage =
{
	/* basic machine hardware */
	{
		{
			CPU_HD6309,
			3000000,		/* 24MHz/8 (?) */
			rockrage_readmem,rockrage_writemem,0,0,
            rockrage_interrupt,1
        },
		{
			CPU_M6809 | CPU_AUDIO_CPU,
			2000000,		/* 24MHz/12 (?) */
			rockrage_readmem_sound, rockrage_writemem_sound,0,0,
			ignore_interrupt,0	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	64, 64 + 2*16*16,
	rockrage_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	rockrage_vh_start,
	rockrage_vh_stop,
	rockrage_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_VLM5030,
			&vlm5030_interface
		}
	}
};


/***************************************************************************

  Game ROMs

***************************************************************************/

ROM_START( rockrage )
	ROM_REGION( 0x20000, REGION_CPU1 ) /* code + banked roms */
	ROM_LOAD( "rr-q01.rom", 0x08000, 0x08000, 0x0ddb5ef5 )	/* fixed ROM */
	ROM_LOAD( "rr-q02.rom", 0x10000, 0x10000, 0xb4f6e346 )	/* banked ROM */

	ROM_REGION(  0x10000 , REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "620k03.11c", 0x08000, 0x08000, 0x9fbefe82 )

	ROM_REGION( 0x040000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "620k06.15g",	0x000000, 0x20000, BADCRC( 0xc0e2b35c ) )	/* tiles */
	ROM_LOAD( "620k05.16g",	0x020000, 0x20000, BADCRC( 0xca9d9346 ) )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rr-k11.rom",	0x000000, 0x20000, 0x70449239 )	/* sprites */
	ROM_LOAD( "rr-l10.rom",	0x020000, 0x20000, 0x06d108e0 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "620k09.11g", 0x00000, 0x00100, 0x9f0e0608 )	/* layer 0 lookup table */
	ROM_LOAD( "620k08.12g", 0x00100, 0x00100, 0xb499800c )	/* layer 1 lookup table */
	ROM_LOAD( "620k07.13g", 0x00200, 0x00100, 0xb6135ee0 )	/* sprite lookup table, but its not used */
															/* because it's always 0 1 2 ... f */
	ROM_REGION( 0x08000, REGION_SOUND1 ) /* VLM3050 data */
	ROM_LOAD( "620k04.6e", 0x00000, 0x08000, 0x8be969f3 )
ROM_END

ROM_START( rockragj )
	ROM_REGION( 0x20000, REGION_CPU1 ) /* code + banked roms */
	ROM_LOAD( "620k01.16c", 0x08000, 0x08000, 0x4f5171f7 )	/* fixed ROM */
	ROM_LOAD( "620k02.15c", 0x10000, 0x10000, 0x04c4d8f7 )	/* banked ROM */

	ROM_REGION(  0x10000 , REGION_CPU2 ) /* 64k for the sound CPU */
	ROM_LOAD( "620k03.11c", 0x08000, 0x08000, 0x9fbefe82 )

	ROM_REGION( 0x040000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "620k06.15g",	0x000000, 0x20000, 0xc0e2b35c )	/* tiles */
	ROM_LOAD( "620k05.16g",	0x020000, 0x20000, 0xca9d9346 )

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "620k11.7g",	0x000000, 0x20000, 0x7430f6e9 )	/* sprites */
	ROM_LOAD( "620k10.8g",	0x020000, 0x20000, 0x0d1a95ab )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "620k09.11g", 0x00000, 0x00100, 0x9f0e0608 )	/* layer 0 lookup table */
	ROM_LOAD( "620k08.12g", 0x00100, 0x00100, 0xb499800c )	/* layer 1 lookup table */
	ROM_LOAD( "620k07.13g", 0x00200, 0x00100, 0xb6135ee0 )	/* sprite lookup table, but its not used */
															/* because it's always 0 1 2 ... f */
	ROM_REGION( 0x08000, REGION_SOUND1 ) /* VLM3050 data */
	ROM_LOAD( "620k04.6e", 0x00000, 0x08000, 0x8be969f3 )
ROM_END

/***************************************************************************

  Game driver(s)

***************************************************************************/

GAME( 1986, rockrage, 0,        rockrage, rockrage, 0, ROT0, "Konami", "Rock 'n Rage (World?)" )
GAME( 1986, rockragj, rockrage, rockrage, rockrage, 0, ROT0, "Konami", "Koi no Hotrock (Japan)" )
