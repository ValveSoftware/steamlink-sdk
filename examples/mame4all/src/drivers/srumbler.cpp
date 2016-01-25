#include "../vidhrdw/srumbler.cpp"

/***************************************************************************

  Speed Rumbler

  Driver provided by Paul Leaman

  M6809 for game, Z80 and YM-2203 for sound.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"

extern unsigned char *srumbler_backgroundram,*srumbler_foregroundram;

WRITE_HANDLER( srumbler_background_w );
WRITE_HANDLER( srumbler_foreground_w );
WRITE_HANDLER( srumbler_scroll_w );
WRITE_HANDLER( srumbler_4009_w );

int  srumbler_vh_start(void);
void srumbler_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



static WRITE_HANDLER( srumbler_bankswitch_w )
{
	/*
	  banking is controlled by two PROMs. 0000-4fff is mapped to the same
	  address (RAM and I/O) for all banks, so we don't handle it here.
	  e000-ffff is all mapped to the same ROMs, however we do handle it
	  here anyway.
	  Note that 5000-8fff can be either ROM or RAM, so we should handle
	  that as well to be 100% accurate.
	 */
	int i;
	unsigned char *ROM = memory_region(REGION_USER1);
	unsigned char *prom1 = memory_region(REGION_PROMS) + (data & 0xf0);
	unsigned char *prom2 = memory_region(REGION_PROMS) + 0x100 + ((data & 0x0f) << 4);

	for (i = 0x05;i < 0x10;i++)
	{
		int bank = ((prom1[i] & 0x03) << 4) | (prom2[i] & 0x0f);
		/* bit 2 of prom1 selects ROM or RAM - not supported */

		cpu_setbank(i+1,&ROM[bank*0x1000]);
	}
}

static void srumbler_init_machine(void)
{
	/* initialize banked ROM pointers */
	srumbler_bankswitch_w(0,0);
}

static int srumbler_interrupt(void)
{
	if (cpu_getiloops()==0)
	{
		return interrupt();
	}
	else
	{
		return M6809_INT_FIRQ;
	}
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3fff, MRA_RAM },   /* RAM (of 1 sort or another) */
	{ 0x4008, 0x4008, input_port_0_r },
	{ 0x4009, 0x4009, input_port_1_r },
	{ 0x400a, 0x400a, input_port_2_r },
	{ 0x400b, 0x400b, input_port_3_r },
	{ 0x400c, 0x400c, input_port_4_r },
	{ 0x5000, 0x5fff, MRA_BANK6 },	/* Banked ROM */
	{ 0x6000, 0x6fff, MRA_BANK7 },	/* Banked ROM */
	{ 0x7000, 0x7fff, MRA_BANK8 },	/* Banked ROM */
	{ 0x8000, 0x8fff, MRA_BANK9 },	/* Banked ROM */
	{ 0x9000, 0x9fff, MRA_BANK10 },	/* Banked ROM */
	{ 0xa000, 0xafff, MRA_BANK11 },	/* Banked ROM */
	{ 0xb000, 0xbfff, MRA_BANK12 },	/* Banked ROM */
	{ 0xc000, 0xcfff, MRA_BANK13 },	/* Banked ROM */
	{ 0xd000, 0xdfff, MRA_BANK14 },	/* Banked ROM */
	{ 0xe000, 0xefff, MRA_BANK15 },	/* Banked ROM */
	{ 0xf000, 0xffff, MRA_BANK16 },	/* Banked ROM */
	{ -1 }  /* end of table */
};

/*
The "scroll test" routine on the test screen appears to overflow and write
over the control registers (0x4000-0x4080) when it clears the screen.

This doesn't affect anything since it happens to write the correct value
to the page register.

Ignore the warnings about writing to unmapped memory.
*/

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x1dff, MWA_RAM },
	{ 0x1e00, 0x1fff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x2000, 0x3fff, srumbler_background_w, &srumbler_backgroundram },
	{ 0x4008, 0x4008, srumbler_bankswitch_w },
	{ 0x4009, 0x4009, srumbler_4009_w },
	{ 0x400a, 0x400d, srumbler_scroll_w },
	{ 0x400e, 0x400e, soundlatch_w },
	{ 0x5000, 0x5fff, srumbler_foreground_w, &srumbler_foregroundram },
	{ 0x6000, 0x6fff, MWA_RAM }, /* Video RAM 2 ??? (not used) */
	{ 0x7000, 0x73ff, paletteram_RRRRGGGGBBBBxxxx_swap_w, &paletteram },
	{ 0x7400, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0xe000, 0xe000, soundlatch_r },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0x0000, 0x7fff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0x8000, 0x8000, YM2203_control_port_0_w },
	{ 0x8001, 0x8001, YM2203_write_port_0_w },
	{ 0xa000, 0xa000, YM2203_control_port_1_w },
	{ 0xa001, 0xa001, YM2203_write_port_1_w },
	{ 0x0000, 0x7fff, MWA_ROM },
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( srumbler )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_6C ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "20k 70k and every 70k" )
	PORT_DIPSETTING(    0x10, "30k 80k and every 80k" )
	PORT_DIPSETTING(    0x08, "20k 80k" )
	PORT_DIPSETTING(    0x00, "30k 80k" )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x80, 0x80, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	1024,   /* 1024 characters */
	2,      /* 2 bits per pixel */
	{ 4, 0 },
	{ 0, 1, 2, 3, 8+0,8+1, 8+2, 8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8
};

static struct GfxLayout tilelayout =
{
	16,16,  /* 16*16 tiles */
	2048,   /* 2048  tiles */
	4,      /* 4 bits per pixel */
	{ 0x20000*8+4, 0x20000*8+0, 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			32*8+0, 32*8+1, 32*8+2, 32*8+3, 33*8+0, 33*8+1, 33*8+2, 33*8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	2048,   /* 2048 sprites */
	4,      /* 4 bits per pixel */
	{ 0x30000*8, 0x20000*8, 0x10000*8, 0   },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			2*64+0, 2*64+1, 2*64+2, 2*64+3, 2*64+4, 2*64+5, 2*64+6, 2*64+7, 2*64+8 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8    /* every sprite takes 32*8 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   448, 16 }, /* colors 448 - 511 */
	{ REGION_GFX2, 0, &tilelayout,   128,  8 }, /* colors 128 - 255 */
	{ REGION_GFX3, 0, &spritelayout, 256,  8 }, /* colors 256 - 383 */
	{ -1 } /* end of array */
};


static struct YM2203interface ym2203_interface =
{
	2,                      /* 2 chips */
	4000000,        /* 4.0 MHz (? hand tuned to match the real board) */
	{ YM2203_VOL(60,20), YM2203_VOL(60,20) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_srumbler =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			1500000,        /* 1.5 Mhz (?) */
			readmem,writemem,0,0,
			srumbler_interrupt,2
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,        /* 3 Mhz ??? */
			sound_readmem,sound_writemem,0,0,
			interrupt,4
		}
	},
	60, 2500,       /* frames per second, vblank duration */
				/* hand tuned to get rid of sprite lag */
	1,      /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	srumbler_init_machine,

	/* video hardware */
	64*8, 32*8, { 10*8, (64-10)*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	512, 512,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_AFTER_VBLANK,
	0,
	srumbler_vh_start,
	0,
	srumbler_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( srumbler )
	ROM_REGION( 0x10000, REGION_CPU1 )  /* 64k for code */
	/* empty, will be filled later */

	ROM_REGION( 0x40000, REGION_USER1 ) /* Paged ROMs */
	ROM_LOAD( "14e_sr04.bin", 0x00000, 0x08000, 0xa68ce89c )  /* RC4 */
	ROM_LOAD( "13e_sr03.bin", 0x08000, 0x08000, 0x87bda812 )  /* RC3 */
	ROM_LOAD( "12e_sr02.bin", 0x10000, 0x08000, 0xd8609cca )  /* RC2 */
	ROM_LOAD( "11e_sr01.bin", 0x18000, 0x08000, 0x27ec4776 )  /* RC1 */
	ROM_LOAD( "14f_sr09.bin", 0x20000, 0x08000, 0x2146101d )  /* RC9 */
	ROM_LOAD( "13f_sr08.bin", 0x28000, 0x08000, 0x838369a6 )  /* RC8 */
	ROM_LOAD( "12f_sr07.bin", 0x30000, 0x08000, 0xde785076 )  /* RC7 */
	ROM_LOAD( "11f_sr06.bin", 0x38000, 0x08000, 0xa70f4fd4 )  /* RC6 */

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "2f_sr05.bin",  0x0000, 0x8000, 0x0177cebe )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "6g_sr10.bin",  0x00000, 0x4000, 0xadabe271 ) /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11a_sr11.bin", 0x00000, 0x8000, 0x5fa042ba ) /* tiles */
	ROM_LOAD( "13a_sr12.bin", 0x08000, 0x8000, 0xa2db64af )
	ROM_LOAD( "14a_sr13.bin", 0x10000, 0x8000, 0xf1df5499 )
	ROM_LOAD( "15a_sr14.bin", 0x18000, 0x8000, 0xb22b31b3 )
	ROM_LOAD( "11c_sr15.bin", 0x20000, 0x8000, 0xca3a3af3 )
	ROM_LOAD( "13c_sr16.bin", 0x28000, 0x8000, 0xc49a4a11 )
	ROM_LOAD( "14c_sr17.bin", 0x30000, 0x8000, 0xaa80aaab )
	ROM_LOAD( "15c_sr18.bin", 0x38000, 0x8000, 0xce67868e )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15e_sr20.bin", 0x00000, 0x8000, 0x3924c861 ) /* sprites */
	ROM_LOAD( "14e_sr19.bin", 0x08000, 0x8000, 0xff8f9129 )
	ROM_LOAD( "15f_sr22.bin", 0x10000, 0x8000, 0xab64161c )
	ROM_LOAD( "14f_sr21.bin", 0x18000, 0x8000, 0xfd64bcd1 )
	ROM_LOAD( "15h_sr24.bin", 0x20000, 0x8000, 0xc972af3e )
	ROM_LOAD( "14h_sr23.bin", 0x28000, 0x8000, 0x8c9abf57 )
	ROM_LOAD( "15j_sr26.bin", 0x30000, 0x8000, 0xd4f1732f )
	ROM_LOAD( "14j_sr25.bin", 0x38000, 0x8000, 0xd2a4ea4f )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "63s141.12a",   0x0000, 0x0100, 0x8421786f )	/* ROM banking */
	ROM_LOAD( "63s141.13a",   0x0100, 0x0100, 0x6048583f )	/* ROM banking */
	ROM_LOAD( "63s141.8j",    0x0200, 0x0100, 0x1a89a7ff )	/* priority (not used) */
ROM_END

ROM_START( srumblr2 )
	ROM_REGION( 0x10000, REGION_CPU1 )  /* 64k for code */
	/* empty, will be filled later */

	ROM_REGION( 0x40000, REGION_USER1 ) /* Paged ROMs */
	ROM_LOAD( "14e_sr04.bin", 0x00000, 0x08000, 0xa68ce89c )  /* RC4 */
	ROM_LOAD( "rc03.13e",     0x08000, 0x08000, 0xe82f78d4 )  /* RC3 (different) */
	ROM_LOAD( "rc02.12e",     0x10000, 0x08000, 0x009a62d8 )  /* RC2 (different) */
	ROM_LOAD( "rc01.11e",     0x18000, 0x08000, 0x2ac48d1d )  /* RC1 (different) */
	ROM_LOAD( "rc09.14f",     0x20000, 0x08000, 0x64f23e72 )  /* RC9 (different) */
	ROM_LOAD( "rc08.13f",     0x28000, 0x08000, 0x74c71007 )  /* RC8 (different) */
	ROM_LOAD( "12f_sr07.bin", 0x30000, 0x08000, 0xde785076 )  /* RC7 */
	ROM_LOAD( "11f_sr06.bin", 0x38000, 0x08000, 0xa70f4fd4 )  /* RC6 */

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "rc05.2f",      0x0000, 0x8000, 0xea04fa07 )  /* AUDIO (different) */

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "6g_sr10.bin",  0x00000, 0x4000, 0xadabe271 ) /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11a_sr11.bin", 0x00000, 0x8000, 0x5fa042ba ) /* tiles */
	ROM_LOAD( "13a_sr12.bin", 0x08000, 0x8000, 0xa2db64af )
	ROM_LOAD( "14a_sr13.bin", 0x10000, 0x8000, 0xf1df5499 )
	ROM_LOAD( "15a_sr14.bin", 0x18000, 0x8000, 0xb22b31b3 )
	ROM_LOAD( "11c_sr15.bin", 0x20000, 0x8000, 0xca3a3af3 )
	ROM_LOAD( "13c_sr16.bin", 0x28000, 0x8000, 0xc49a4a11 )
	ROM_LOAD( "14c_sr17.bin", 0x30000, 0x8000, 0xaa80aaab )
	ROM_LOAD( "15c_sr18.bin", 0x38000, 0x8000, 0xce67868e )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15e_sr20.bin", 0x00000, 0x8000, 0x3924c861 ) /* sprites */
	ROM_LOAD( "14e_sr19.bin", 0x08000, 0x8000, 0xff8f9129 )
	ROM_LOAD( "15f_sr22.bin", 0x10000, 0x8000, 0xab64161c )
	ROM_LOAD( "14f_sr21.bin", 0x18000, 0x8000, 0xfd64bcd1 )
	ROM_LOAD( "15h_sr24.bin", 0x20000, 0x8000, 0xc972af3e )
	ROM_LOAD( "14h_sr23.bin", 0x28000, 0x8000, 0x8c9abf57 )
	ROM_LOAD( "15j_sr26.bin", 0x30000, 0x8000, 0xd4f1732f )
	ROM_LOAD( "14j_sr25.bin", 0x38000, 0x8000, 0xd2a4ea4f )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "63s141.12a",   0x0000, 0x0100, 0x8421786f )	/* ROM banking */
	ROM_LOAD( "63s141.13a",   0x0100, 0x0100, 0x6048583f )	/* ROM banking */
	ROM_LOAD( "63s141.8j",    0x0200, 0x0100, 0x1a89a7ff )	/* priority (not used) */
ROM_END

ROM_START( rushcrsh )
	ROM_REGION( 0x10000, REGION_CPU1 )  /* 64k for code */
	/* empty, will be filled later */

	ROM_REGION( 0x40000, REGION_USER1 ) /* Paged ROMs */
	ROM_LOAD( "14e_sr04.bin", 0x00000, 0x08000, 0xa68ce89c )  /* RC4 */
	ROM_LOAD( "rc03.bin",     0x08000, 0x08000, 0xa49c9be0 )  /* RC3 (different) */
	ROM_LOAD( "rc02.12e",     0x10000, 0x08000, 0x009a62d8 )  /* RC2 (different) */
	ROM_LOAD( "rc01.11e",     0x18000, 0x08000, 0x2ac48d1d )  /* RC1 (different) */
	ROM_LOAD( "rc09.14f",     0x20000, 0x08000, 0x64f23e72 )  /* RC9 (different) */
	ROM_LOAD( "rc08.bin",     0x28000, 0x08000, 0x2c25874b )  /* RC8 (different) */
	ROM_LOAD( "12f_sr07.bin", 0x30000, 0x08000, 0xde785076 )  /* RC7 */
	ROM_LOAD( "11f_sr06.bin", 0x38000, 0x08000, 0xa70f4fd4 )  /* RC6 */

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "rc05.2f",      0x0000, 0x8000, 0xea04fa07 )  /* AUDIO (different) */

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rc10.bin",     0x00000, 0x4000, 0x0a3c0b0d ) /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "11a_sr11.bin", 0x00000, 0x8000, 0x5fa042ba ) /* tiles */
	ROM_LOAD( "13a_sr12.bin", 0x08000, 0x8000, 0xa2db64af )
	ROM_LOAD( "14a_sr13.bin", 0x10000, 0x8000, 0xf1df5499 )
	ROM_LOAD( "15a_sr14.bin", 0x18000, 0x8000, 0xb22b31b3 )
	ROM_LOAD( "11c_sr15.bin", 0x20000, 0x8000, 0xca3a3af3 )
	ROM_LOAD( "13c_sr16.bin", 0x28000, 0x8000, 0xc49a4a11 )
	ROM_LOAD( "14c_sr17.bin", 0x30000, 0x8000, 0xaa80aaab )
	ROM_LOAD( "15c_sr18.bin", 0x38000, 0x8000, 0xce67868e )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15e_sr20.bin", 0x00000, 0x8000, 0x3924c861 ) /* sprites */
	ROM_LOAD( "14e_sr19.bin", 0x08000, 0x8000, 0xff8f9129 )
	ROM_LOAD( "15f_sr22.bin", 0x10000, 0x8000, 0xab64161c )
	ROM_LOAD( "14f_sr21.bin", 0x18000, 0x8000, 0xfd64bcd1 )
	ROM_LOAD( "15h_sr24.bin", 0x20000, 0x8000, 0xc972af3e )
	ROM_LOAD( "14h_sr23.bin", 0x28000, 0x8000, 0x8c9abf57 )
	ROM_LOAD( "15j_sr26.bin", 0x30000, 0x8000, 0xd4f1732f )
	ROM_LOAD( "14j_sr25.bin", 0x38000, 0x8000, 0xd2a4ea4f )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "63s141.12a",   0x0000, 0x0100, 0x8421786f )	/* ROM banking */
	ROM_LOAD( "63s141.13a",   0x0100, 0x0100, 0x6048583f )	/* ROM banking */
	ROM_LOAD( "63s141.8j",    0x0200, 0x0100, 0x1a89a7ff )	/* priority (not used) */
ROM_END



GAME( 1986, srumbler, 0,        srumbler, srumbler, 0, ROT270, "Capcom", "The Speed Rumbler (set 1)" )
GAME( 1986, srumblr2, srumbler, srumbler, srumbler, 0, ROT270, "Capcom", "The Speed Rumbler (set 2)" )
GAME( 1986, rushcrsh, srumbler, srumbler, srumbler, 0, ROT270, "Capcom", "Rush & Crash (Japan)" )
