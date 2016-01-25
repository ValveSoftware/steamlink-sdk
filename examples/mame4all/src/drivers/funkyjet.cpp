#include "../vidhrdw/funkyjet.cpp"

/***************************************************************************

  Funky Jet                               (c) 1992 Mitchell Corporation

  But actually a Data East game...  Hardware is pretty close to Super Burger
  Time but with a different graphics chip.  And I can't work out the graphics
  format of this chip - it's used in all Data East games from 1991 onwards.
  If you want to help decode it, look at Rohga for an easy example as the fix
  char roms (64k) are in this format.

  The game also uses a protection chip which isn't fully worked out yet.

  Emulation by Bryan McPhail, mish@tendril.co.uk

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/h6280/h6280.h"

int  funkyjet_vh_start(void);
void funkyjet_vh_stop(void);
void funkyjet_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( funkyjet_pf2_data_w );
WRITE_HANDLER( funkyjet_pf1_data_w );
READ_HANDLER( funkyjet_pf1_data_r );
READ_HANDLER( funkyjet_pf2_data_r );

WRITE_HANDLER( funkyjet_control_0_w );

extern unsigned char *funkyjet_pf2_data,*funkyjet_pf1_data,*funkyjet_pf1_row;
static unsigned char *funkyjet_ram;

/******************************************************************************/

static int loopback[0x800];

static WRITE_HANDLER( funkyjet_protection_w )
{
	WRITE_WORD(&loopback[offset],data);

	//if (offset!=0x502 && offset!=0x700 && offset!=0x70e && offset!=0x78e)
	//	logerror("CPU #0 PC %06x: warning - write unmapped control address %06x %04x\n",cpu_get_pc(),offset,data);

	if (offset==0x10a) {
		soundlatch_w(0,data&0xff);
		cpu_cause_interrupt(1,H6280_INT_IRQ1);
	}

	/*

		offset==0x502 - written after joystick read
		offset==0x700 - mirror of flipscreen byte
		offset==0x70e - written after credits read
		offset==0x78e - written after credits read
	*/
}

static READ_HANDLER( funkyjet_protection_r )
{
 	switch (offset)
	{
		case 0x148: /* EOR mask for joysticks */
			return 0;
		case 0xc: /* Player 1 & Player 2 joysticks & fire buttons */
		case 0x24c:
			return ~(readinputport(0) + (readinputport(1) << 8));

		case 0x2d8: /* EOR mask for credits */
			return 0;
		case 0x778: /* Credits */
			return readinputport(2);

		case 0x382: /* DIPS */
			return (readinputport(3) + (readinputport(4) << 8));

		case 0x56c:
			return 0;

	}

	//if (offset!=0x778) logerror("CPU #0 PC %06x: warning - read unmapped control address %06x\n",cpu_get_pc(),offset);

/*

Protection device:

	382 is the flag for whether controls are read (bne 0x800)

	778 is read and written back to 502


*/

	return 0xffff;
}

/******************************************************************************/

static struct MemoryReadAddress funkyjet_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x120000, 0x1207ff, paletteram_word_r },
	{ 0x140000, 0x143fff, MRA_BANK1 },
	{ 0x160000, 0x1607ff, MRA_BANK2 },
	{ 0x180000, 0x1807ff, funkyjet_protection_r },

	{ 0x320000, 0x321fff, funkyjet_pf1_data_r },
	{ 0x322000, 0x323fff, funkyjet_pf2_data_r },
	{ 0x340000, 0x340bff, MRA_BANK3 },
	{ 0x342000, 0x342bff, MRA_BANK4 }, /* pf2 rowscroll */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress funkyjet_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x120000, 0x1207ff, paletteram_xxxxBBBBGGGGRRRR_word_w, &paletteram },
	{ 0x140000, 0x143fff, MWA_BANK1, &funkyjet_ram },
	{ 0x160000, 0x1607ff, MWA_BANK2, &spriteram },
	{ 0x180000, 0x1807ff, funkyjet_protection_w },

	{ 0x184000, 0x184001, MWA_NOP },
	{ 0x188000, 0x188001, MWA_NOP },

	{ 0x300000, 0x30000f, funkyjet_control_0_w },
	{ 0x320000, 0x321fff, funkyjet_pf1_data_w, &funkyjet_pf1_data },
	{ 0x322000, 0x323fff, funkyjet_pf2_data_w, &funkyjet_pf2_data },
	{ 0x340000, 0x340bff, MWA_BANK3, &funkyjet_pf1_row },
	{ 0x342000, 0x342bff, MWA_BANK4 }, /* pf2 rowscroll */
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

/* Physical memory map (21 bits) */
static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x000000, 0x00ffff, MRA_ROM },
	{ 0x100000, 0x100001, MRA_NOP },
	{ 0x110000, 0x110001, YM2151_status_port_0_r },
	{ 0x120000, 0x120001, OKIM6295_status_0_r },
	{ 0x130000, 0x130001, MRA_NOP }, /* This board only has 1 oki chip */
	{ 0x140000, 0x140001, soundlatch_r },
	{ 0x1f0000, 0x1f1fff, MRA_BANK8 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x000000, 0x00ffff, MWA_ROM },
	{ 0x100000, 0x100001, MWA_NOP }, /* YM2203 - this board doesn't have one */
	{ 0x110000, 0x110001, YM2151_w },
	{ 0x120000, 0x120001, OKIM6295_data_0_w },
	{ 0x130000, 0x130001, MWA_NOP },
	{ 0x1f0000, 0x1f1fff, MWA_BANK8 },
	{ 0x1fec00, 0x1fec01, H6280_timer_w },
	{ 0x1ff402, 0x1ff403, H6280_irq_status_w },
	{ -1 }  /* end of table */
};

/******************************************************************************/

INPUT_PORTS_START( funkyjet )
	PORT_START	/* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 - unused? */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 - unused? */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* Credits */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switch bank 1 */

	/* Dips seem inverted with respect to other Deco games */

	/* Some of these coinage options may not be correct.. */
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x80, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
  	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 chars */
	4096,
	4,		/* 4 bits per pixel  */
	{ 0x40000*8+8, 0x40000*8, 8, 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout tile_layout =
{
	16,16,
	4096,
	4,
	{ 0x40000*8+8, 0x40000*8, 8, 0 },
	{ 32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
		0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

static struct GfxLayout sprite_layout =
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
	{ REGION_GFX1, 0, &charlayout,   256, 16 },	/* Characters 8x8 */
	{ REGION_GFX1, 0, &tile_layout,  512, 16 }, 	/* Tiles 16x16 */
	{ REGION_GFX2, 0, &sprite_layout,  0, 16 },	/* Sprites 16x16 */
	{ -1 } /* end of array */
};

/******************************************************************************/

static struct OKIM6295interface okim6295_interface =
{
	1,          /* 1 chip */
	{ 7757 },	/* Frequency */
	{ REGION_SOUND1 },      /* memory region */
	{ 50 }
};

static void sound_irq(int state)
{
	cpu_set_irq_line(1,1,state); /* IRQ 2 */
}

static struct YM2151interface ym2151_interface =
{
	1,
	32220000/9,
	{ YM3012_VOL(45,MIXER_PAN_LEFT,45,MIXER_PAN_RIGHT) },
	{ sound_irq }
};

static struct MachineDriver machine_driver_funkyjet =
{
	/* basic machine hardware */
	{
	 	{
			CPU_M68000,
			14000000, /* 28 MHz crystal */
			funkyjet_readmem,funkyjet_writemem,0,0,
			m68_level6_irq,1
		},
		{
			CPU_H6280 | CPU_AUDIO_CPU, /* Custom chip 45 */
			32220000/8, /* Audio section crystal is 32.220 MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
	},
	58, 529,
	1,
	0,

	/* video hardware */
	40*8, 32*8, { 0*8, 40*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	funkyjet_vh_start,
	funkyjet_vh_stop,
	funkyjet_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
  	{
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

ROM_START( funkyjet )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "jk00.12f", 0x00000, 0x40000, 0x712089c1 )
	ROM_LOAD_ODD ( "jk01.13f", 0x00000, 0x40000, 0xbe3920d7 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "jk02.16f",    0x00000, 0x10000, 0x748c0bd8 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mat02", 0x000000, 0x80000, 0xe4b94c7e ) /* chars */

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
  	ROM_LOAD( "mat00", 0x000000, 0x80000, 0xfbda0228 ) /* sprites */
	ROM_LOAD( "mat01", 0x080000, 0x80000, 0x24093a8d )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
  	ROM_LOAD( "jk03.15h",    0x00000, 0x20000, 0x69a0eaf7 )
ROM_END

/******************************************************************************/

GAMEX( 1992, funkyjet, 0, funkyjet, funkyjet, 0, ROT0, "[Data East] (Mitchell license)", "Funky Jet", GAME_NOT_WORKING )
