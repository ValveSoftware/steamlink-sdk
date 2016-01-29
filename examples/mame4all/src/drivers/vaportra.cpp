#include "../vidhrdw/vaportra.cpp"

/***************************************************************************

  Vapor Trail (World version)  (c) 1989 Data East Corporation
  Vapor Trail (USA version)    (c) 1989 Data East USA
  Kuhga (Japanese version)     (c) 1989 Data East Corporation

  Emulation by Bryan McPhail, mish@tendril.co.uk

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/h6280/h6280.h"

int  vaportra_vh_start(void);
void vaportra_vh_stop (void);
void vaportra_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( vaportra_pf1_data_w );
WRITE_HANDLER( vaportra_pf2_data_w );
WRITE_HANDLER( vaportra_pf3_data_w );
WRITE_HANDLER( vaportra_pf4_data_w );
READ_HANDLER( vaportra_pf1_data_r );
READ_HANDLER( vaportra_pf2_data_r );
READ_HANDLER( vaportra_pf3_data_r );
READ_HANDLER( vaportra_pf4_data_r );

WRITE_HANDLER( vaportra_control_0_w );
WRITE_HANDLER( vaportra_control_1_w );
WRITE_HANDLER( vaportra_control_2_w );
WRITE_HANDLER( vaportra_palette_24bit_rg_w );
WRITE_HANDLER( vaportra_palette_24bit_b_w );

extern unsigned char *vaportra_pf1_data,*vaportra_pf2_data,*vaportra_pf3_data,*vaportra_pf4_data;
static unsigned char *vaportra_ram;

WRITE_HANDLER( vaportra_update_sprites_w );

/******************************************************************************/

static WRITE_HANDLER( vaportra_sound_w )
{
	soundlatch_w(0,data & 0xff);
	cpu_cause_interrupt(1,H6280_INT_IRQ1);
}

static READ_HANDLER( vaportra_control_r )
{
	switch (offset)
	{
		case 4: /* Dip Switches */
			return (readinputport(4) + (readinputport(3) << 8));
		case 2: /* Credits */
			return readinputport(2);
		case 0: /* Player 1 & Player 2 joysticks & fire buttons */
			return (readinputport(0) + (readinputport(1) << 8));
	}

	//logerror("Unknown control read at %d\n",offset);
	return 0xffff;
}

/******************************************************************************/

static struct MemoryReadAddress vaportra_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x100000, 0x10000f, vaportra_control_r },

	{ 0x200000, 0x201fff, vaportra_pf2_data_r },
	{ 0x202000, 0x203fff, vaportra_pf4_data_r },
	{ 0x280000, 0x281fff, vaportra_pf1_data_r },
	{ 0x282000, 0x283fff, vaportra_pf3_data_r },

	{ 0x300000, 0x300fff, paletteram_word_r },
	{ 0x304000, 0x304fff, paletteram_2_word_r },
	{ 0x308000, 0x308001, MRA_NOP },
	{ 0xff8000, 0xff87ff, MRA_BANK2 },
	{ 0xffc000, 0xffffff, MRA_BANK1 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress vaportra_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x100000, 0x100003, vaportra_control_2_w },
	{ 0x100006, 0x100007, vaportra_sound_w },

	{ 0x200000, 0x201fff, vaportra_pf2_data_w, &vaportra_pf2_data },
	{ 0x202000, 0x203fff, vaportra_pf4_data_w, &vaportra_pf4_data },
	{ 0x240000, 0x24000f, vaportra_control_0_w },

	{ 0x280000, 0x281fff, vaportra_pf1_data_w, &vaportra_pf1_data },
	{ 0x282000, 0x283fff, vaportra_pf3_data_w, &vaportra_pf3_data },
	{ 0x2c0000, 0x2c000f, vaportra_control_1_w },

	{ 0x300000, 0x3009ff, vaportra_palette_24bit_rg_w, &paletteram },
	{ 0x304000, 0x3049ff, vaportra_palette_24bit_b_w, &paletteram_2 },
	{ 0x308000, 0x308001, MWA_NOP },
	{ 0x30c000, 0x30c001, vaportra_update_sprites_w },
	{ 0xff8000, 0xff87ff, MWA_BANK2, &spriteram },
	{ 0xffc000, 0xffffff, MWA_BANK1, &vaportra_ram },
	{ -1 }  /* end of table */
};

/******************************************************************************/

static WRITE_HANDLER( YM2151_w )
{
	switch (offset) {
	case 0:
		YM2151_register_port_0_w(0,data);
		break;
	case 1:
		YM2151_data_port_0_w(0,data);
		break;
	}
}

static WRITE_HANDLER( YM2203_w )
{
	switch (offset) {
	case 0:
		YM2203_control_port_0_w(0,data);
		break;
	case 1:
		YM2203_write_port_0_w(0,data);
		break;
	}
}

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x000000, 0x00ffff, MRA_ROM },
	{ 0x100000, 0x100001, YM2203_status_port_0_r },
	{ 0x110000, 0x110001, YM2151_status_port_0_r },
	{ 0x120000, 0x120001, OKIM6295_status_0_r },
	{ 0x130000, 0x130001, OKIM6295_status_1_r },
	{ 0x140000, 0x140001, soundlatch_r },
	{ 0x1f0000, 0x1f1fff, MRA_BANK8 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x000000, 0x00ffff, MWA_ROM },
	{ 0x100000, 0x100001, YM2203_w },
	{ 0x110000, 0x110001, YM2151_w },
	{ 0x120000, 0x120001, OKIM6295_data_0_w },
	{ 0x130000, 0x130001, OKIM6295_data_1_w },
	{ 0x1f0000, 0x1f1fff, MWA_BANK8 },
	{ 0x1fec00, 0x1fec01, H6280_timer_w },
	{ 0x1ff402, 0x1ff403, H6280_irq_status_w },
	{ -1 }  /* end of table */
};

/******************************************************************************/

INPUT_PORTS_START( vaportra )
	PORT_START	/* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* Credits */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
  	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 chars */
	4096,
	4,		/* 4 bits per pixel  */
	{ 8, 0,  0x40000*8+8, 0x40000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout seallayout =
{
	16,16,
	4096,
	4,
	{ 0x80000*8+8, 0x80000*8, 8, 0 },
	{ 32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
		0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

static struct GfxLayout seallayout3 =
{
	16,16,
	4096,
	4,
	{ 8, 0, 0x40000*8+8, 0x40000*8 },
	{ 32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
		0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

static struct GfxLayout seallayout2 =
{
	16,16,
	4096*2,
	4,
	{ 8, 0, 0x80000*8+8, 0x80000*8 },
	{ 32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
		0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x000000, &charlayout,    0, 16 },	/* Characters 8x8 */
	{ REGION_GFX2, 0x000000, &seallayout,  768, 16 },	/* Tiles 16x16 */
	{ REGION_GFX1, 0x000000, &seallayout3, 512, 16 },	/* Tiles 16x16 */
	{ REGION_GFX2, 0x040000, &seallayout, 1024, 16 },	/* Tiles 16x16 */
	{ REGION_GFX3, 0x000000, &seallayout2, 256, 16 },	/* Sprites 16x16 */
	{ -1 } /* end of array */
};

/******************************************************************************/

static struct OKIM6295interface okim6295_interface =
{
	2,              /* 2 chips */
	{ 7757, 15514 },/* Frequency */
	{ REGION_SOUND1, REGION_SOUND2 },	/* memory regions */
	{ 75, 50 }		/* Note!  Keep chip 1 (voices) louder than chip 2 */
};

static struct YM2203interface ym2203_interface =
{
	1,
	32220000/8,	/* Accurate, audio section crystal is 32.220 MHz */
	{ YM2203_VOL(40,40) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static void sound_irq(int state)
{
	cpu_set_irq_line(1,1,state); /* IRQ 2 */
}

static struct YM2151interface ym2151_interface =
{
	1,
	32220000/9, /* Accurate, audio section crystal is 32.220 MHz */
	{ YM3012_VOL(45,MIXER_PAN_LEFT,45,MIXER_PAN_RIGHT) },
	{ sound_irq }
};

static struct MachineDriver machine_driver_vaportra =
{
	/* basic machine hardware */
	{
	 	{
			CPU_M68000, /* Custom chip 59 */
			12000000,
			vaportra_readmem,vaportra_writemem,0,0,
			m68_level6_irq,1 /* VBL */
		},
		{
			CPU_H6280 | CPU_AUDIO_CPU, /* Custom chip 45 */
			32220000/8, /* Audio section crystal is 32.220 MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
	},
	58, 529, /* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	1280, 1280,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	vaportra_vh_start,
	vaportra_vh_stop,
	vaportra_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
  	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

/******************************************************************************/

ROM_START( vaportra )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
  	ROM_LOAD_EVEN( "fl_02-1.bin", 0x00000, 0x20000, 0x9ae36095 )
  	ROM_LOAD_ODD ( "fl_00-1.bin", 0x00000, 0x20000, 0xc08cc048 )
	ROM_LOAD_EVEN( "fl_03.bin",   0x40000, 0x20000, 0x80bd2844 )
 	ROM_LOAD_ODD ( "fl_01.bin",   0x40000, 0x20000, 0x9474b085 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fj04",    0x00000, 0x10000, 0xe9aedf9b )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vtmaa00.bin",   0x000000, 0x80000, 0x0330e13b ) /* chars & tiles */

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
  	ROM_LOAD( "vtmaa01.bin",   0x000000, 0x80000, 0xc217a31b ) /* tiles 2 */
	ROM_LOAD( "vtmaa02.bin",   0x080000, 0x80000, 0x091ff98e ) /* tiles 3 */

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
  	ROM_LOAD( "vtmaa03.bin",   0x000000, 0x80000, 0x1a30bf81 ) /* sprites */
  	ROM_LOAD( "vtmaa04.bin",   0x080000, 0x80000, 0xb713e9cc )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "fj06",    0x00000, 0x20000, 0x6e98a235 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* ADPCM samples */
	ROM_LOAD( "fj05",    0x00000, 0x20000, 0x39cda2b5 )
ROM_END

ROM_START( vaportru )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
  	ROM_LOAD_EVEN( "fj02",   0x00000, 0x20000, 0xa2affb73 )
  	ROM_LOAD_ODD ( "fj00",   0x00000, 0x20000, 0xef05e07b )
	ROM_LOAD_EVEN( "fj03",   0x40000, 0x20000, 0x44893379 )
 	ROM_LOAD_ODD ( "fj01",   0x40000, 0x20000, 0x97fbc107 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fj04",    0x00000, 0x10000, 0xe9aedf9b )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vtmaa00.bin",   0x000000, 0x80000, 0x0330e13b ) /* chars & tiles */

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
  	ROM_LOAD( "vtmaa01.bin",   0x000000, 0x80000, 0xc217a31b ) /* tiles 2 */
	ROM_LOAD( "vtmaa02.bin",   0x080000, 0x80000, 0x091ff98e ) /* tiles 3 */

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
  	ROM_LOAD( "vtmaa03.bin",   0x000000, 0x80000, 0x1a30bf81 ) /* sprites */
  	ROM_LOAD( "vtmaa04.bin",   0x080000, 0x80000, 0xb713e9cc )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "fj06",    0x00000, 0x20000, 0x6e98a235 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* ADPCM samples */
	ROM_LOAD( "fj05",    0x00000, 0x20000, 0x39cda2b5 )
ROM_END

ROM_START( kuhga )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
  	ROM_LOAD_EVEN( "fp02-3.bin", 0x00000, 0x20000, 0xd0705ef4 )
  	ROM_LOAD_ODD ( "fp00-3.bin", 0x00000, 0x20000, 0x1da92e48 )
	ROM_LOAD_EVEN( "fp03.bin",   0x40000, 0x20000, 0xea0da0f1 )
 	ROM_LOAD_ODD ( "fp01.bin",   0x40000, 0x20000, 0xe3ecbe86 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fj04",    0x00000, 0x10000, 0xe9aedf9b )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vtmaa00.bin",   0x000000, 0x80000, 0x0330e13b ) /* chars & tiles */

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
  	ROM_LOAD( "vtmaa01.bin",   0x000000, 0x80000, 0xc217a31b ) /* tiles 2 */
	ROM_LOAD( "vtmaa02.bin",   0x080000, 0x80000, 0x091ff98e ) /* tiles 3 */

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
  	ROM_LOAD( "vtmaa03.bin",   0x000000, 0x80000, 0x1a30bf81 ) /* sprites */
  	ROM_LOAD( "vtmaa04.bin",   0x080000, 0x80000, 0xb713e9cc )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "fj06",    0x00000, 0x20000, 0x6e98a235 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* ADPCM samples */
	ROM_LOAD( "fj05",    0x00000, 0x20000, 0x39cda2b5 )
ROM_END

/******************************************************************************/

static void vaportra_decrypt(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int i;

	for (i=0x00000; i<0x80000; i++)
		RAM[i]=(RAM[i] & 0x7e) | ((RAM[i] & 0x01) << 7) | ((RAM[i] & 0x80) >> 7);
}

static READ_HANDLER( cycle_r )
{
	int pc=cpu_get_pc();
	int ret=READ_WORD(&vaportra_ram[0x6]);

	if (ret==0 && (pc==0x3dea || pc==0x3de8)) {
		cpu_spinuntil_int();
		return 1;
	}
	return ret;
}

static void init_vaportra(void)
{
	install_mem_read_handler(0, 0xffc006, 0xffc007, cycle_r);
	vaportra_decrypt();
}

/******************************************************************************/

GAME( 1989, vaportra, 0,        vaportra, vaportra, vaportra, ROT270, "Data East Corporation", "Vapor Trail - Hyper Offence Formation (World revision 1)" )
GAME( 1989, vaportru, vaportra, vaportra, vaportra, vaportra, ROT270, "Data East USA", "Vapor Trail - Hyper Offence Formation (US)" )
GAME( 1989, kuhga,    vaportra, vaportra, vaportra, vaportra, ROT270, "Data East Corporation", "Kuhga - Operation Code 'Vapor Trail' (Japan revision 3)" )
