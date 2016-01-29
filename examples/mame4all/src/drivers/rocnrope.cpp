#include "../vidhrdw/rocnrope.cpp"

/***************************************************************************

Based on drivers from Juno First emulator by Chris Hardy (chrish@kcbbs.gen.nz)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"


void konami1_decode(void);

WRITE_HANDLER( rocnrope_flipscreen_w );
void rocnrope_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void rocnrope_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/* defined in sndhrdw/timeplt.c */
extern struct MemoryReadAddress timeplt_sound_readmem[];
extern struct MemoryWriteAddress timeplt_sound_writemem[];
extern struct AY8910interface timeplt_ay8910_interface;
WRITE_HANDLER( timeplt_sh_irqtrigger_w );


/* Roc'n'Rope has the IRQ vectors in RAM. The rom contains $FFFF at this address! */
WRITE_HANDLER( rocnrope_interrupt_vector_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);


	RAM[0xFFF2+offset] = data;
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x3080, 0x3080, input_port_0_r }, /* IO Coin */
	{ 0x3081, 0x3081, input_port_1_r }, /* P1 IO */
	{ 0x3082, 0x3082, input_port_2_r }, /* P2 IO */
	{ 0x3083, 0x3083, input_port_3_r }, /* DSW 0 */
	{ 0x3000, 0x3000, input_port_4_r }, /* DSW 1 */
	{ 0x3100, 0x3100, input_port_5_r }, /* DSW 2 */
	{ 0x4000, 0x5fff, MRA_RAM },
	{ 0x6000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x4000, 0x403f, MWA_RAM, &spriteram_2 },
	{ 0x4040, 0x43ff, MWA_RAM },
	{ 0x4400, 0x443f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x4440, 0x47ff, MWA_RAM },
	{ 0x4800, 0x4bff, colorram_w, &colorram },
	{ 0x4c00, 0x4fff, videoram_w, &videoram, &videoram_size },
	{ 0x5000, 0x5fff, MWA_RAM },
	{ 0x8000, 0x8000, watchdog_reset_w },
	{ 0x8080, 0x8080, rocnrope_flipscreen_w },
	{ 0x8081, 0x8081, timeplt_sh_irqtrigger_w },  /* cause interrupt on audio CPU */
	{ 0x8082, 0x8082, MWA_NOP },	/* interrupt acknowledge??? */
	{ 0x8083, 0x8083, MWA_NOP },	/* Coin counter 1 */
	{ 0x8084, 0x8084, MWA_NOP },	/* Coin counter 2 */
	{ 0x8087, 0x8087, interrupt_enable_w },
	{ 0x8100, 0x8100, soundlatch_w },
	{ 0x8182, 0x818d, rocnrope_interrupt_vector_w },
	{ 0x6000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( rocnrope )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
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
	PORT_DIPSETTING(    0x00, "Disabled" )
/* 0x00 disables Coin 2. It still accepts coins and makes the sound, but
   it doesn't give you any credit */

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "255", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x78, 0x78, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x78, "Easy 1" )
	PORT_DIPSETTING(    0x70, "Easy 2" )
	PORT_DIPSETTING(    0x68, "Easy 3" )
	PORT_DIPSETTING(    0x60, "Easy 4" )
	PORT_DIPSETTING(    0x58, "Normal 1" )
	PORT_DIPSETTING(    0x50, "Normal 2" )
	PORT_DIPSETTING(    0x48, "Normal 3" )
	PORT_DIPSETTING(    0x40, "Normal 4" )
	PORT_DIPSETTING(    0x38, "Normal 5" )
	PORT_DIPSETTING(    0x30, "Normal 6" )
	PORT_DIPSETTING(    0x28, "Normal 7" )
	PORT_DIPSETTING(    0x20, "Normal 8" )
	PORT_DIPSETTING(    0x18, "Difficult 1" )
	PORT_DIPSETTING(    0x10, "Difficult 2" )
	PORT_DIPSETTING(    0x08, "Difficult 3" )
	PORT_DIPSETTING(    0x00, "Difficult 4" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x07, 0x07, "First Bonus" )
	PORT_DIPSETTING(    0x07, "20000" )
	PORT_DIPSETTING(    0x05, "30000" )
	PORT_DIPSETTING(    0x04, "40000" )
	PORT_DIPSETTING(    0x03, "50000" )
	PORT_DIPSETTING(    0x02, "60000" )
	PORT_DIPSETTING(    0x01, "70000" )
	PORT_DIPSETTING(    0x00, "80000" )
	/* 0x06 gives 20000 */
	PORT_DIPNAME( 0x38, 0x38, "Repeated Bonus" )
	PORT_DIPSETTING(    0x38, "40000" )
	PORT_DIPSETTING(    0x18, "50000" )
	PORT_DIPSETTING(    0x10, "60000" )
	PORT_DIPSETTING(    0x08, "70000" )
	PORT_DIPSETTING(    0x00, "80000" )
	/* 0x20, 0x28 and 0x30 all gives 40000 */
	PORT_DIPNAME( 0x40, 0x00, "Grant Repeated Bonus" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x00, "Unknown DSW 8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 sprites */
	512,	/* 512 characters */
	4,	/* 4 bits per pixel */
	{ 0x2000*8+4, 0x2000*8+0, 4, 0 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	256,	/* 256 sprites */
	4,	/* 4 bits per pixel */
	{ 256*64*8+4, 256*64*8+0, 4, 0 },
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,       0, 16 },
	{ REGION_GFX2, 0, &spritelayout, 16*16, 16 },
	{ -1 } /* end of array */
};



static struct MachineDriver machine_driver_rocnrope =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			2048000,        /* 2 Mhz */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318180/8,	/* 1.789772727 MHz */						\
			timeplt_sound_readmem,timeplt_sound_writemem,0,0,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32,16*16+16*16,
	rocnrope_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	rocnrope_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&timeplt_ay8910_interface
		}
	}
};

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( rocnrope )
	ROM_REGION( 2*0x10000, REGION_CPU1 )     /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "rr1.1h",       0x6000, 0x2000, 0x83093134 )
	ROM_LOAD( "rr2.2h",       0x8000, 0x2000, 0x75af8697 )
	ROM_LOAD( "rr3.3h",       0xa000, 0x2000, 0xb21372b1 )
	ROM_LOAD( "rr4.4h",       0xc000, 0x2000, 0x7acb2a05 )
	ROM_LOAD( "rnr_h5.vid",   0xe000, 0x2000, 0x150a6264 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "rnr_7a.snd",   0x0000, 0x1000, 0x75d2c4e2 )
	ROM_LOAD( "rnr_8a.snd",   0x1000, 0x1000, 0xca4325ae )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rnr_h12.vid",  0x0000, 0x2000, 0xe2114539 )
	ROM_LOAD( "rnr_h11.vid",  0x2000, 0x2000, 0x169a8f3f )

	ROM_REGION( 0x8000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rnr_a11.vid",  0x0000, 0x2000, 0xafdaba5e )
	ROM_LOAD( "rnr_a12.vid",  0x2000, 0x2000, 0x054cafeb )
	ROM_LOAD( "rnr_a9.vid",   0x4000, 0x2000, 0x9d2166b2 )
	ROM_LOAD( "rnr_a10.vid",  0x6000, 0x2000, 0xaff6e22f )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "a17_prom.bin", 0x0000, 0x0020, 0x22ad2c3e )
	ROM_LOAD( "b16_prom.bin", 0x0020, 0x0100, 0x750a9677 )
	ROM_LOAD( "rocnrope.pr3", 0x0120, 0x0100, 0xb5c75a27 )
ROM_END

ROM_START( rocnropk )
	ROM_REGION( 2*0x10000, REGION_CPU1 )     /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "rnr_h1.vid",   0x6000, 0x2000, 0x0fddc1f6 )
	ROM_LOAD( "rnr_h2.vid",   0x8000, 0x2000, 0xce9db49a )
	ROM_LOAD( "rnr_h3.vid",   0xa000, 0x2000, 0x6d278459 )
	ROM_LOAD( "rnr_h4.vid",   0xc000, 0x2000, 0x9b2e5f2a )
	ROM_LOAD( "rnr_h5.vid",   0xe000, 0x2000, 0x150a6264 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "rnr_7a.snd",   0x0000, 0x1000, 0x75d2c4e2 )
	ROM_LOAD( "rnr_8a.snd",   0x1000, 0x1000, 0xca4325ae )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rnr_h12.vid",  0x0000, 0x2000, 0xe2114539 )
	ROM_LOAD( "rnr_h11.vid",  0x2000, 0x2000, 0x169a8f3f )

	ROM_REGION( 0x8000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rnr_a11.vid",  0x0000, 0x2000, 0xafdaba5e )
	ROM_LOAD( "rnr_a12.vid",  0x2000, 0x2000, 0x054cafeb )
	ROM_LOAD( "rnr_a9.vid",   0x4000, 0x2000, 0x9d2166b2 )
	ROM_LOAD( "rnr_a10.vid",  0x6000, 0x2000, 0xaff6e22f )

	ROM_REGION( 0x0220, REGION_PROMS )
	ROM_LOAD( "a17_prom.bin", 0x0000, 0x0020, 0x22ad2c3e )
	ROM_LOAD( "b16_prom.bin", 0x0020, 0x0100, 0x750a9677 )
	ROM_LOAD( "rocnrope.pr3", 0x0120, 0x0100, 0xb5c75a27 )
ROM_END



static void init_rocnrope(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;


	konami1_decode();
	rom[0x703d + diff] = 0x98;	/* fix one instruction */
}

static void init_rocnropk(void)
{
	konami1_decode();
}



GAME( 1983, rocnrope, 0,        rocnrope, rocnrope, rocnrope, ROT270, "Konami", "Roc'n Rope" )
GAME( 1983, rocnropk, rocnrope, rocnrope, rocnrope, rocnropk, ROT270, "Konami + Kosuka", "Roc'n Rope (Kosuka)" )
