#include "../vidhrdw/pcktgal.cpp"

/***************************************************************************

	Pocket Gal						(c) 1987 Data East Corporation
	Pocket Gal (Bootleg)			(c) 1989 Yada East Corporation(!!!)
	Super Pool III					(c) 1989 Data East Corporation
	Pocket Gal 2					(c) 1989 Data East Corporation
	Super Pool III (I-Vics Inc)		(c) 1990 Data East Corporation

	Pocket Gal (Bootleg) is often called 'Sexy Billiards'

	Emulation by Bryan McPhail, mish@tendril.co.uk

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6502/m6502.h"

/* From dec8.c - rename it to dec8_color prom later.. used by many games.. */
void ghostb_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void pcktgal_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( pcktgal_flipscreen_w );

/***************************************************************************/

static WRITE_HANDLER( pcktgal_bank_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	if (data & 1) { cpu_setbank(1,&RAM[0x4000]); }
	else { cpu_setbank(1,&RAM[0x10000]); }

	if (data & 2) { cpu_setbank(2,&RAM[0x6000]); }
	else { cpu_setbank(2,&RAM[0x12000]); }
}

static WRITE_HANDLER( pcktgal_sound_bank_w )
{
	unsigned char *RAM = memory_region(REGION_CPU2);

	if (data & 4) { cpu_setbank(3,&RAM[0x14000]); }
	else { cpu_setbank(3,&RAM[0x10000]); }
}

static WRITE_HANDLER( pcktgal_sound_w )
{
	soundlatch_w(0,data);
	cpu_cause_interrupt(1,M6502_INT_NMI);
}

static int msm5205next;

static void pcktgal_adpcm_int(int data)
{
	static int toggle;

	MSM5205_data_w(0,msm5205next >> 4);
	msm5205next<<=4;

	toggle = 1 - toggle;
	if (toggle)
		cpu_cause_interrupt(1,M6502_INT_IRQ);
}

static WRITE_HANDLER( pcktgal_adpcm_data_w )
{
	msm5205next=data;
}

static READ_HANDLER( pcktgal_adpcm_reset_r )
{
	MSM5205_reset_w(0,0);
	return 0;
}

/***************************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x1800, 0x1800, input_port_0_r },
	{ 0x1a00, 0x1a00, input_port_1_r },
	{ 0x1c00, 0x1c00, input_port_2_r },
	{ 0x4000, 0x5fff, MRA_BANK1 },
	{ 0x6000, 0x7fff, MRA_BANK2 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x0800, 0x0fff, videoram_w, &videoram, &videoram_size },
	{ 0x1000, 0x11ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x1801, 0x1801, pcktgal_flipscreen_w },
	/* 1800 - 0x181f are unused BAC-06 registers, see vidhrdw/dec0.c */
	{ 0x1a00, 0x1a00, pcktgal_sound_w },
	{ 0x1c00, 0x1c00, pcktgal_bank_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

/***************************************************************************/

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x3000, 0x3000, soundlatch_r },
	{ 0x3400, 0x3400, pcktgal_adpcm_reset_r },	/* ? not sure */
	{ 0x4000, 0x7fff, MRA_BANK3 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x0800, 0x0800, YM2203_control_port_0_w },
	{ 0x0801, 0x0801, YM2203_write_port_0_w },
	{ 0x1000, 0x1000, YM3812_control_port_0_w },
	{ 0x1001, 0x1001, YM3812_write_port_0_w },
	{ 0x1800, 0x1800, pcktgal_adpcm_data_w },	/* ADPCM data for the MSM5205 chip */
	{ 0x2000, 0x2000, pcktgal_sound_bank_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

/***************************************************************************/

INPUT_PORTS_START( pcktgal )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )

	PORT_START	/* Dip switch */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 1C_3C ) )
 	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
 	PORT_DIPNAME( 0x08, 0x08, "Allow 2 Players Game" )
	PORT_DIPSETTING(	0x00, DEF_STR( No ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Yes ) )
 	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
 	PORT_DIPNAME( 0x20, 0x20, "Time" )
	PORT_DIPSETTING(	0x00, "100" )
	PORT_DIPSETTING(	0x20, "120" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x00, "3" )
	PORT_DIPSETTING(	0x40, "4" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
INPUT_PORTS_END

/***************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	4096,
	4,
	{ 0x10000*8, 0, 0x18000*8, 0x8000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	 /* every char takes 8 consecutive bytes */
};

static struct GfxLayout bootleg_charlayout =
{
	8,8,	/* 8*8 characters */
	4096,
	4,
	{ 0x18000*8, 0x8000*8, 0x10000*8, 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	 /* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	1024,   /* 1024 sprites */
	2,	  /* 2 bits per pixel */
	{ 0x8000*8, 0 },
	{ 128+0, 128+1, 128+2, 128+3, 128+4, 128+5, 128+6, 128+7, 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout bootleg_spritelayout =
{
	16,16,  /* 16*16 sprites */
	1024,   /* 1024 sprites */
	2,	  /* 2 bits per pixel */
	{ 0x8000*8, 0 },
	{ 128+7, 128+6, 128+5, 128+4, 128+3, 128+2, 128+1, 128+0, 7, 6, 5, 4, 3, 2, 1, 0,  },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every char takes 8 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &charlayout,   256, 16 }, /* chars */
	{ REGION_GFX2, 0x00000, &spritelayout,   0,  8 }, /* sprites */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo bootleg_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &bootleg_charlayout,   256, 16 }, /* chars */
	{ REGION_GFX2, 0x00000, &bootleg_spritelayout,   0,  8 }, /* sprites */
	{ -1 } /* end of array */
};

/***************************************************************************/

static struct YM2203interface ym2203_interface =
{
	1,	  /* 1 chip */
	1500000,		/* 1.5 MHz */
	{ YM2203_VOL(60,60) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct YM3812interface ym3812_interface =
{
	1,			/* 1 chip (no more supported) */
	3000000,	/* 3 MHz */
	{ 50 }
};

static struct MSM5205interface msm5205_interface =
{
	1,					/* 1 chip			 */
	384000,				/* 384KHz			 */
	{ pcktgal_adpcm_int },/* interrupt function */
	{ MSM5205_S48_4B},	/* 8KHz			   */
	{ 70 }
};

/***************************************************************************/

static struct MachineDriver machine_driver_pcktgal =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			2000000,
			readmem,writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
							/* IRQs are caused by the ADPCM chip */
							/* NMIs are caused by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,  /* frames per second, vblank duration */
	1, /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	512, 512,
	ghostb_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	pcktgal_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812_interface
		},
		{
			SOUND_MSM5205,
			&msm5205_interface
		}
	}
};

static struct MachineDriver machine_driver_bootleg =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			2000000,
			readmem,writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
							/* IRQs are caused by the ADPCM chip */
							/* NMIs are caused by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,  /* frames per second, vblank duration */
	1, /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	bootleg_gfxdecodeinfo,
	512, 512,
	ghostb_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	pcktgal_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812_interface
		},
		{
			SOUND_MSM5205,
			&msm5205_interface
		}
	}
};

/***************************************************************************/

ROM_START( pcktgal )
	ROM_REGION( 0x14000, REGION_CPU1 )	 /* 64k for code + 16k for banks */
	ROM_LOAD( "eb04.rom",	   0x10000, 0x4000, 0x8215d60d )
	ROM_CONTINUE(			   0x04000, 0xc000)
	/* 4000-7fff is banked but code falls through from 7fff to 8000, so */
	/* I have to load the bank directly at 4000. */

	ROM_REGION( 2*0x18000, REGION_CPU2 )	 /* 96k for code + 96k for decrypted opcodes */
	ROM_LOAD( "eb03.rom",	   0x10000, 0x8000, 0xcb029b02 )
	ROM_CONTINUE(			   0x08000, 0x8000 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "eb01.rom",	   0x00000, 0x10000, 0x63542c3d )
	ROM_LOAD( "eb02.rom",	   0x10000, 0x10000, 0xa9dcd339 )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "eb00.rom",	   0x00000, 0x10000, 0x6c1a14a8 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "eb05.rom",     0x0000, 0x0200, 0x3b6198cb ) /* 82s147.084 */
	ROM_LOAD( "eb06.rom",     0x0200, 0x0200, 0x1fbd4b59 ) /* 82s131.101 */
ROM_END

ROM_START( pcktgalb )
	ROM_REGION( 0x14000, REGION_CPU1 )	 /* 64k for code + 16k for banks */
	ROM_LOAD( "sexybill.001", 0x10000, 0x4000, 0x4acb3e84 )
	ROM_CONTINUE(			  0x04000, 0xc000)
	/* 4000-7fff is banked but code falls through from 7fff to 8000, so */
	/* I have to load the bank directly at 4000. */

	ROM_REGION( 2*0x18000, REGION_CPU2 )	 /* 96k for code + 96k for decrypted opcodes */
	ROM_LOAD( "eb03.rom",	  0x10000, 0x8000, 0xcb029b02 )
	ROM_CONTINUE(			  0x08000, 0x8000 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sexybill.005", 0x00000, 0x10000, 0x3128dc7b )
	ROM_LOAD( "sexybill.006", 0x10000, 0x10000, 0x0fc91eeb )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sexybill.003", 0x00000, 0x08000, 0x58182daa )
	ROM_LOAD( "sexybill.004", 0x08000, 0x08000, 0x33a67af6 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "eb05.rom",     0x0000, 0x0200, 0x3b6198cb ) /* 82s147.084 */
	ROM_LOAD( "eb06.rom",     0x0200, 0x0200, 0x1fbd4b59 ) /* 82s131.101 */
ROM_END

ROM_START( pcktgal2 )
	ROM_REGION( 0x14000, REGION_CPU1 )	 /* 64k for code + 16k for banks */
	ROM_LOAD( "eb04-2.rom",   0x10000, 0x4000, 0x0c7f2905 )
	ROM_CONTINUE(			  0x04000, 0xc000)
	/* 4000-7fff is banked but code falls through from 7fff to 8000, so */
	/* I have to load the bank directly at 4000. */

	ROM_REGION( 0x18000, REGION_CPU2 )	 /* audio cpu */
	ROM_LOAD( "eb03-2.rom",   0x10000, 0x8000, 0x9408ffb4 )
	ROM_CONTINUE(			  0x08000, 0x8000)

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "eb01-2.rom",   0x00000, 0x10000, 0xe52b1f97 )
	ROM_LOAD( "eb02-2.rom",   0x10000, 0x10000, 0xf30d965d )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "eb00.rom",	  0x00000, 0x10000, 0x6c1a14a8 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "eb05.rom",     0x0000, 0x0200, 0x3b6198cb ) /* 82s147.084 */
	ROM_LOAD( "eb06.rom",     0x0200, 0x0200, 0x1fbd4b59 ) /* 82s131.101 */
ROM_END

ROM_START( spool3 )
	ROM_REGION( 0x14000, REGION_CPU1 )	 /* 64k for code + 16k for banks */
	ROM_LOAD( "eb04-2.rom",   0x10000, 0x4000, 0x0c7f2905 )
	ROM_CONTINUE(			  0x04000, 0xc000)
	/* 4000-7fff is banked but code falls through from 7fff to 8000, so */
	/* I have to load the bank directly at 4000. */

	ROM_REGION( 0x18000, REGION_CPU2 )	 /* audio cpu */
	ROM_LOAD( "eb03-2.rom",   0x10000, 0x8000, 0x9408ffb4 )
	ROM_CONTINUE(			  0x08000, 0x8000)

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "deco2.bin",	  0x00000, 0x10000, 0x0a23f0cf )
	ROM_LOAD( "deco3.bin",	  0x10000, 0x10000, 0x55ea7c45 )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "eb00.rom",	  0x00000, 0x10000, 0x6c1a14a8 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "eb05.rom",     0x0000, 0x0200, 0x3b6198cb ) /* 82s147.084 */
	ROM_LOAD( "eb06.rom",     0x0200, 0x0200, 0x1fbd4b59 ) /* 82s131.101 */
ROM_END

ROM_START( spool3i )
	ROM_REGION( 0x14000, REGION_CPU1 )	 /* 64k for code + 16k for banks */
	ROM_LOAD( "de1.bin",	  0x10000, 0x4000, 0xa59980fe )
	ROM_CONTINUE(			  0x04000, 0xc000)
	/* 4000-7fff is banked but code falls through from 7fff to 8000, so */
	/* I have to load the bank directly at 4000. */

	ROM_REGION( 0x18000, REGION_CPU2 )	 /* audio cpu */
	ROM_LOAD( "eb03-2.rom",   0x10000, 0x8000, 0x9408ffb4 )
	ROM_CONTINUE(			  0x08000, 0x8000)

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "deco2.bin",	  0x00000, 0x10000, 0x0a23f0cf )
	ROM_LOAD( "deco3.bin",	  0x10000, 0x10000, 0x55ea7c45 )

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "eb00.rom",	  0x00000, 0x10000, 0x6c1a14a8 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "eb05.rom",     0x0000, 0x0200, 0x3b6198cb ) /* 82s147.084 */
	ROM_LOAD( "eb06.rom",     0x0200, 0x0200, 0x1fbd4b59 ) /* 82s131.101 */
ROM_END

/***************************************************************************/

static void init_deco222(void)
{
	int A;
	unsigned char *rom = memory_region(REGION_CPU2);
	int diff = memory_region_length(REGION_CPU2) / 2;


	memory_set_opcode_base(1,rom+diff);

	/* bits 5 and 6 of the opcodes are swapped */
	for (A = 0;A < diff;A++)
		rom[A + diff] = (rom[A] & 0x9f) | ((rom[A] & 0x20) << 1) | ((rom[A] & 0x40) >> 1);
}

static void init_graphics(void)
{
	unsigned char *rom = memory_region(REGION_GFX1);
	int len = memory_region_length(REGION_GFX1);
	int i,j,temp[16];

	/* Tile graphics roms have some swapped lines, original version only */
	for (i = 0x00000;i < len;i += 32)
	{
		for (j=0; j<16; j++)
		{
			temp[j] = rom[i+j+16];
			rom[i+j+16] = rom[i+j];
			rom[i+j] = temp[j];
		}
	}
}

static void init_pcktgal(void)
{
	init_deco222();
	init_graphics();
}

/***************************************************************************/

GAME( 1987, pcktgal,  0,       pcktgal, pcktgal, pcktgal,  ROT0, "Data East Corporation", "Pocket Gal (Japan)" )
GAME( 1989, pcktgalb, pcktgal, bootleg, pcktgal, deco222,  ROT0, "bootleg", "Pocket Gal (bootleg)" )
GAME( 1989, pcktgal2, pcktgal, pcktgal, pcktgal, graphics, ROT0, "Data East Corporation", "Pocket Gal 2 (World?)" )
GAME( 1989, spool3,   pcktgal, pcktgal, pcktgal, graphics, ROT0, "Data East Corporation", "Super Pool III (World?)" )
GAME( 1990, spool3i,  pcktgal, pcktgal, pcktgal, graphics, ROT0, "Data East Corporation (I-Vics license)", "Super Pool III (I-Vics)" )
