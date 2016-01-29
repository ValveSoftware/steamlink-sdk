#include "../vidhrdw/cbuster.cpp"

/***************************************************************************

  Crude Buster (World version FX)		(c) 1990 Data East Corporation
  Crude Buster (World version FU)		(c) 1990 Data East Corporation
  Crude Buster (Japanese version)		(c) 1990 Data East Corporation
  Two Crude (USA version)	    		(c) 1990 Data East USA

  The 'FX' board is filled with 'FU' roms except for the 4 program roms,
  both boards have 'export' stickers which usually indicates a World version.
  Maybe one is a UK or European version.

  Emulation by Bryan McPhail, mish@tendril.co.uk

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/h6280/h6280.h"

int  twocrude_vh_start(void);
void twocrude_vh_stop(void);
void twocrude_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( twocrude_pf1_data_w );
WRITE_HANDLER( twocrude_pf2_data_w );
WRITE_HANDLER( twocrude_pf3_data_w );
WRITE_HANDLER( twocrude_pf4_data_w );
WRITE_HANDLER( twocrude_control_0_w );
WRITE_HANDLER( twocrude_control_1_w );
WRITE_HANDLER( twocrude_palette_24bit_rg_w );
WRITE_HANDLER( twocrude_palette_24bit_b_w );
READ_HANDLER( twocrude_palette_24bit_rg_r );
READ_HANDLER( twocrude_palette_24bit_b_r );

WRITE_HANDLER( twocrude_pf1_rowscroll_w );
WRITE_HANDLER( twocrude_pf2_rowscroll_w );
WRITE_HANDLER( twocrude_pf3_rowscroll_w );
WRITE_HANDLER( twocrude_pf4_rowscroll_w );

extern unsigned char *twocrude_pf1_rowscroll,*twocrude_pf2_rowscroll;
extern unsigned char *twocrude_pf3_rowscroll,*twocrude_pf4_rowscroll;
extern unsigned char *twocrude_pf1_data, *twocrude_pf2_data, *twocrude_pf3_data, *twocrude_pf4_data;
static unsigned char *twocrude_ram;
extern void twocrude_pri_w(int pri);
WRITE_HANDLER( twocrude_update_sprites_w );
static int prot;

/******************************************************************************/

static WRITE_HANDLER( twocrude_control_w )
{
	switch (offset) {
	case 0: /* DMA flag */
		twocrude_update_sprites_w(0,0);
		return;

	case 6: /* IRQ ack */
		return;

    case 2: /* Sound CPU write */
		soundlatch_w(0,data & 0xff);
		cpu_cause_interrupt(1,H6280_INT_IRQ1);
    	return;

	case 4: /* Protection, maybe this is a PAL on the board?

			80046 is level number
			stop at stage and enter.
			see also 8216..

				9a 00 = pf4 over pf3 (normal) (level 0)
				9a f1 =  (level 1 - water), pf3 over ALL sprites + pf4
				9a 80 = pf3 over pf4 (Level 2 - copter)
				9a 40 = pf3 over ALL sprites + pf4 (snow) level 3
				9a c0 = doesn't matter?
				9a ff = pf 3 over pf4

			I can't find a priority register, I assume it's tied to the
			protection?!

		*/
		if ((data&0xffff)==0x9a00) prot=0;
		if ((data&0xffff)==0xaa) prot=0x74;
		if ((data&0xffff)==0x0200) prot=0x63<<8;
		if ((data&0xffff)==0x9a) prot=0xe;
		if ((data&0xffff)==0x55) prot=0x1e;
		if ((data&0xffff)==0x0e) {prot=0x0e;twocrude_pri_w(0);} /* start */
		if ((data&0xffff)==0x00) {prot=0x0e;twocrude_pri_w(0);} /* level 0 */
		if ((data&0xffff)==0xf1) {prot=0x36;twocrude_pri_w(1);} /* level 1 */
		if ((data&0xffff)==0x80) {prot=0x2e;twocrude_pri_w(1);} /* level 2 */
		if ((data&0xffff)==0x40) {prot=0x1e;twocrude_pri_w(1);} /* level 3 */
		if ((data&0xffff)==0xc0) {prot=0x3e;twocrude_pri_w(0);} /* level 4 */
		if ((data&0xffff)==0xff) {prot=0x76;twocrude_pri_w(1);} /* level 5 */

		break;
	}
	//logerror("Warning %04x- %02x written to control %02x\n",cpu_get_pc(),data,offset);
}

READ_HANDLER( twocrude_control_r )
{
	switch (offset)
	{
		case 0: /* Player 1 & Player 2 joysticks & fire buttons */
			return (readinputport(0) + (readinputport(1) << 8));

		case 2: /* Dip Switches */
			return (readinputport(3) + (readinputport(4) << 8));

		case 4: /* Protection */
			//logerror("%04x : protection control read at 30c000 %d\n",cpu_get_pc(),offset);
			return prot;

		case 6: /* Credits, VBL in byte 7 */
			return readinputport(2);
	}

	return 0xffff;
}

static READ_HANDLER( twocrude_pf1_data_r ) { return READ_WORD(&twocrude_pf1_data[offset]);}
static READ_HANDLER( twocrude_pf2_data_r ) { return READ_WORD(&twocrude_pf2_data[offset]);}
static READ_HANDLER( twocrude_pf3_data_r ) { return READ_WORD(&twocrude_pf3_data[offset]);}
static READ_HANDLER( twocrude_pf4_data_r ) { return READ_WORD(&twocrude_pf4_data[offset]);}
static READ_HANDLER( twocrude_pf1_rowscroll_r ) { return READ_WORD(&twocrude_pf1_rowscroll[offset]);}
static READ_HANDLER( twocrude_pf2_rowscroll_r ) { return READ_WORD(&twocrude_pf2_rowscroll[offset]);}
static READ_HANDLER( twocrude_pf3_rowscroll_r ) { return READ_WORD(&twocrude_pf3_rowscroll[offset]);}
static READ_HANDLER( twocrude_pf4_rowscroll_r ) { return READ_WORD(&twocrude_pf4_rowscroll[offset]);}

/******************************************************************************/

static struct MemoryReadAddress twocrude_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x080000, 0x083fff, MRA_BANK1 },

	{ 0x0a0000, 0x0a1fff, twocrude_pf1_data_r },
	{ 0x0a2000, 0x0a2fff, twocrude_pf4_data_r },
	{ 0x0a4000, 0x0a47ff, twocrude_pf1_rowscroll_r },
	{ 0x0a6000, 0x0a67ff, twocrude_pf4_rowscroll_r },

	{ 0x0a8000, 0x0a8fff, twocrude_pf3_data_r },
	{ 0x0aa000, 0x0aafff, twocrude_pf2_data_r },
	{ 0x0ac000, 0x0ac7ff, twocrude_pf3_rowscroll_r },
	{ 0x0ae000, 0x0ae7ff, twocrude_pf2_rowscroll_r },

	{ 0x0b0000, 0x0b07ff, MRA_BANK2 },
	{ 0x0b8000, 0x0b8fff, twocrude_palette_24bit_rg_r },
	{ 0x0b9000, 0x0b9fff, twocrude_palette_24bit_b_r },
	{ 0x0bc000, 0x0bc00f, twocrude_control_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress twocrude_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x080000, 0x083fff, MWA_BANK1, &twocrude_ram },

	{ 0x0a0000, 0x0a1fff, twocrude_pf1_data_w, &twocrude_pf1_data },
	{ 0x0a2000, 0x0a2fff, twocrude_pf4_data_w, &twocrude_pf4_data },
	{ 0x0a4000, 0x0a47ff, twocrude_pf1_rowscroll_w, &twocrude_pf1_rowscroll },
	{ 0x0a6000, 0x0a67ff, twocrude_pf4_rowscroll_w, &twocrude_pf4_rowscroll },

	{ 0x0a8000, 0x0a8fff, twocrude_pf3_data_w, &twocrude_pf3_data },
	{ 0x0aa000, 0x0aafff, twocrude_pf2_data_w, &twocrude_pf2_data },
	{ 0x0ac000, 0x0ac7ff, twocrude_pf3_rowscroll_w, &twocrude_pf3_rowscroll },
	{ 0x0ae000, 0x0ae7ff, twocrude_pf2_rowscroll_w, &twocrude_pf2_rowscroll },

	{ 0x0b0000, 0x0b07ff, MWA_BANK2, &spriteram },
	{ 0x0b4000, 0x0b4001, MWA_NOP },
	{ 0x0b5000, 0x0b500f, twocrude_control_1_w },
	{ 0x0b6000, 0x0b600f, twocrude_control_0_w },
	{ 0x0b8000, 0x0b8fff, twocrude_palette_24bit_rg_w, &paletteram },
	{ 0x0b9000, 0x0b9fff, twocrude_palette_24bit_b_w, &paletteram_2 },
	{ 0x0bc000, 0x0bc00f, twocrude_control_w },
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

INPUT_PORTS_START( twocrude )
	PORT_START	/* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
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
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
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
	8,8,
	4096,
	4,
	{ 0x10000*8+8, 8, 0x10000*8, 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			 },
	16*8
};

static struct GfxLayout tilelayout =
{
	16,16,
	4096,
	4,
	{ 24, 16, 8, 0 },
	{ 64*8+0, 64*8+1, 64*8+2, 64*8+3, 64*8+4, 64*8+5, 64*8+6, 64*8+7,
		0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	128*8
};

static struct GfxLayout spritelayout =
{
	16,16,
	(4096*2)+2048,  /* Main bank + 4 extra roms */
	4,
	{ 0xa0000*8+8, 0xa0000*8, 8, 0 },
	{ 32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
		0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout, 	   0, 16 },	/* Characters 8x8 */
	{ REGION_GFX2, 0, &tilelayout,  1024, 16 },	/* Tiles 16x16 */
	{ REGION_GFX2, 0, &tilelayout,   768, 16 },	/* Tiles 16x16 */
	{ REGION_GFX3, 0, &tilelayout,   512, 16 },	/* Tiles 16x16 */
	{ REGION_GFX4, 0, &spritelayout, 256, 80 },	/* Sprites 16x16 */
	{ -1 } /* end of array */
};

/******************************************************************************/

static struct OKIM6295interface okim6295_interface =
{
	2,              /* 2 chips */
	{ 7757, 15514 },/* Frequency */
	{ REGION_SOUND1, REGION_SOUND2 },  /* memory regions 3 & 4 */
	{ 50, 25 }		/* Note!  Keep chip 1 (voices) louder than chip 2 */
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

static struct MachineDriver machine_driver_twocrude =
{
	/* basic machine hardware */
	{
	 	{
			CPU_M68000,
			12000000, /* Accurate */
			twocrude_readmem,twocrude_writemem,0,0,
			m68_level4_irq,1 /* VBL */
		},
		{
			CPU_H6280 | CPU_AUDIO_CPU,
			32220000/8,	/* Accurate */
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
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	twocrude_vh_start,
	twocrude_vh_stop,
	twocrude_vh_screenrefresh,

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

ROM_START( cbuster )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
  	ROM_LOAD_EVEN( "fx01.rom", 0x00000, 0x20000, 0xddae6d83 )
	ROM_LOAD_ODD ( "fx00.rom", 0x00000, 0x20000, 0x5bc2c0de )
  	ROM_LOAD_EVEN( "fx03.rom", 0x40000, 0x20000, 0xc3d65bf9 )
 	ROM_LOAD_ODD ( "fx02.rom", 0x40000, 0x20000, 0xb875266b )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fu11-.rom",     0x00000, 0x10000, 0x65f20f10 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fu05-.rom",     0x00000, 0x10000, 0x8134d412 ) /* Chars */
	ROM_LOAD( "fu06-.rom",     0x10000, 0x10000, 0x2f914a45 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mab-01",        0x00000, 0x80000, 0x1080d619 ) /* Tiles */

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mab-00",        0x00000, 0x80000, 0x660eaabd ) /* Tiles */

	ROM_REGION( 0x180000,REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mab-02",        0x000000, 0x80000, 0x58b7231d ) /* Sprites */
	/* Space for extra sprites to be copied to (0x20000) */
	ROM_LOAD( "mab-03",        0x0a0000, 0x80000, 0x76053b9d )
 	/* Space for extra sprites to be copied to (0x20000) */
	ROM_LOAD( "fu07-.rom",     0x140000, 0x10000, 0xca8d0bb3 ) /* Extra sprites */
	ROM_LOAD( "fu08-.rom",     0x150000, 0x10000, 0xc6afc5c8 )
	ROM_LOAD( "fu09-.rom",     0x160000, 0x10000, 0x526809ca )
	ROM_LOAD( "fu10-.rom",     0x170000, 0x10000, 0x6be6d50e )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "fu12-.rom",     0x00000, 0x20000, 0x2d1d65f2 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* ADPCM samples */
	ROM_LOAD( "fu13-.rom",     0x00000, 0x20000, 0xb8525622 )
ROM_END

ROM_START( cbusterw )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
  	ROM_LOAD_EVEN( "fu01-.rom", 0x00000, 0x20000, 0x0203e0f8 )
	ROM_LOAD_ODD ( "fu00-.rom", 0x00000, 0x20000, 0x9c58626d )
  	ROM_LOAD_EVEN( "fu03-.rom", 0x40000, 0x20000, 0xdef46956 )
 	ROM_LOAD_ODD ( "fu02-.rom", 0x40000, 0x20000, 0x649c3338 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fu11-.rom",     0x00000, 0x10000, 0x65f20f10 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fu05-.rom",     0x00000, 0x10000, 0x8134d412 ) /* Chars */
	ROM_LOAD( "fu06-.rom",     0x10000, 0x10000, 0x2f914a45 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mab-01",        0x00000, 0x80000, 0x1080d619 ) /* Tiles */

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mab-00",        0x00000, 0x80000, 0x660eaabd ) /* Tiles */

	ROM_REGION( 0x180000,REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mab-02",        0x000000, 0x80000, 0x58b7231d ) /* Sprites */
	/* Space for extra sprites to be copied to (0x20000) */
	ROM_LOAD( "mab-03",        0x0a0000, 0x80000, 0x76053b9d )
 	/* Space for extra sprites to be copied to (0x20000) */
	ROM_LOAD( "fu07-.rom",     0x140000, 0x10000, 0xca8d0bb3 ) /* Extra sprites */
	ROM_LOAD( "fu08-.rom",     0x150000, 0x10000, 0xc6afc5c8 )
	ROM_LOAD( "fu09-.rom",     0x160000, 0x10000, 0x526809ca )
	ROM_LOAD( "fu10-.rom",     0x170000, 0x10000, 0x6be6d50e )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "fu12-.rom",     0x00000, 0x20000, 0x2d1d65f2 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* ADPCM samples */
	ROM_LOAD( "fu13-.rom",     0x00000, 0x20000, 0xb8525622 )
ROM_END

ROM_START( cbusterj )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
  	ROM_LOAD_EVEN( "fr01-1",   0x00000, 0x20000, 0xaf3c014f )
	ROM_LOAD_ODD ( "fr00-1",   0x00000, 0x20000, 0xf666ad52 )
  	ROM_LOAD_EVEN( "fr03",     0x40000, 0x20000, 0x02c06118 )
 	ROM_LOAD_ODD ( "fr02",     0x40000, 0x20000, 0xb6c34332 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fu11-.rom",     0x00000, 0x10000, 0x65f20f10 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fu05-.rom",     0x00000, 0x10000, 0x8134d412 ) /* Chars */
	ROM_LOAD( "fu06-.rom",     0x10000, 0x10000, 0x2f914a45 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mab-01",        0x00000, 0x80000, 0x1080d619 ) /* Tiles */

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mab-00",        0x00000, 0x80000, 0x660eaabd ) /* Tiles */

	ROM_REGION( 0x180000,REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mab-02",        0x000000, 0x80000, 0x58b7231d ) /* Sprites */
	/* Space for extra sprites to be copied to (0x20000) */
	ROM_LOAD( "mab-03",        0x0a0000, 0x80000, 0x76053b9d )
 	/* Space for extra sprites to be copied to (0x20000) */
	ROM_LOAD( "fr07",          0x140000, 0x10000, 0x52c85318 ) /* Extra sprites */
	ROM_LOAD( "fr08",          0x150000, 0x10000, 0xea25fbac )
	ROM_LOAD( "fr09",          0x160000, 0x10000, 0xf8363424 )
	ROM_LOAD( "fr10",          0x170000, 0x10000, 0x241d5760 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "fu12-.rom",     0x00000, 0x20000, 0x2d1d65f2 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* ADPCM samples */
	ROM_LOAD( "fu13-.rom",     0x00000, 0x20000, 0xb8525622 )
ROM_END

ROM_START( twocrude )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "ft01",     0x00000, 0x20000, 0x08e96489 )
	ROM_LOAD_ODD ( "ft00",     0x00000, 0x20000, 0x6765c445 )
	ROM_LOAD_EVEN( "ft03",     0x40000, 0x20000, 0x28002c99 )
	ROM_LOAD_ODD ( "ft02",     0x40000, 0x20000, 0x37ea0626 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fu11-.rom",     0x00000, 0x10000, 0x65f20f10 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fu05-.rom",     0x00000, 0x10000, 0x8134d412 ) /* Chars */
	ROM_LOAD( "fu06-.rom",     0x10000, 0x10000, 0x2f914a45 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mab-01",        0x00000, 0x80000, 0x1080d619 ) /* Tiles */

	ROM_REGION( 0x80000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mab-00",        0x00000, 0x80000, 0x660eaabd ) /* Tiles */

	ROM_REGION( 0x180000,REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mab-02",        0x000000, 0x80000, 0x58b7231d ) /* Sprites */
	/* Space for extra sprites to be copied to (0x20000) */
	ROM_LOAD( "mab-03",        0x0a0000, 0x80000, 0x76053b9d )
 	/* Space for extra sprites to be copied to (0x20000) */
	ROM_LOAD( "ft07",          0x140000, 0x10000, 0xe3465c25 )
	ROM_LOAD( "ft08",          0x150000, 0x10000, 0xc7f1d565 )
	ROM_LOAD( "ft09",          0x160000, 0x10000, 0x6e3657b9 )
	ROM_LOAD( "ft10",          0x170000, 0x10000, 0xcdb83560 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "fu12-.rom",     0x00000, 0x20000, 0x2d1d65f2 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* ADPCM samples */
	ROM_LOAD( "fu13-.rom",     0x00000, 0x20000, 0xb8525622 )
ROM_END

/******************************************************************************/

static void init_twocrude(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	unsigned char *PTR;
	int i,j;

	/* Main cpu decrypt */
	for (i=0x00000; i<0x80000; i+=2) {
#ifdef LSB_FIRST
		RAM[i+1]=(RAM[i+1] & 0xcf) | ((RAM[i+1] & 0x10) << 1) | ((RAM[i+1] & 0x20) >> 1);
		RAM[i+1]=(RAM[i+1] & 0x5f) | ((RAM[i+1] & 0x20) << 2) | ((RAM[i+1] & 0x80) >> 2);

		RAM[i]=(RAM[i] & 0xbd) | ((RAM[i] & 0x2) << 5) | ((RAM[i] & 0x40) >> 5);
		RAM[i]=(RAM[i] & 0xf5) | ((RAM[i] & 0x2) << 2) | ((RAM[i] & 0x8) >> 2);
#else
		RAM[i]=(RAM[i] & 0xcf) | ((RAM[i] & 0x10) << 1) | ((RAM[i] & 0x20) >> 1);
		RAM[i]=(RAM[i] & 0x5f) | ((RAM[i] & 0x20) << 2) | ((RAM[i] & 0x80) >> 2);

		RAM[i+1]=(RAM[i+1] & 0xbd) | ((RAM[i+1] & 0x2) << 5) | ((RAM[i+1] & 0x40) >> 5);
		RAM[i+1]=(RAM[i+1] & 0xf5) | ((RAM[i+1] & 0x2) << 2) | ((RAM[i+1] & 0x8) >> 2);
#endif
	}

	/* Rearrange the 'extra' sprite bank to be in the same format as main sprites */
	RAM = memory_region(REGION_GFX4) + 0x080000;
	PTR = memory_region(REGION_GFX4) + 0x140000;
	for (i=0; i<0x20000; i+=64) {
		for (j=0; j<16; j+=1) { /* Copy 16 lines down */
			RAM[i+      0+j*2]=PTR[i/2+      0+j]; /* Pixels 0-7 for each plane */
			RAM[i+      1+j*2]=PTR[i/2+0x10000+j];
			RAM[i+0xa0000+j*2]=PTR[i/2+0x20000+j];
			RAM[i+0xa0001+j*2]=PTR[i/2+0x30000+j];
		}

		for (j=0; j<16; j+=1) { /* Copy 16 lines down */
			RAM[i+   0x20+j*2]=PTR[i/2+   0x10+j]; /* Pixels 8-15 for each plane */
			RAM[i+   0x21+j*2]=PTR[i/2+0x10010+j];
			RAM[i+0xa0020+j*2]=PTR[i/2+0x20010+j];
			RAM[i+0xa0021+j*2]=PTR[i/2+0x30010+j];
		}
	}
}

/******************************************************************************/

GAME( 1990, cbuster,  0,       twocrude, twocrude, twocrude, ROT0, "Data East Corporation", "Crude Buster (World FX version)" )
GAME( 1990, cbusterw, cbuster, twocrude, twocrude, twocrude, ROT0, "Data East Corporation", "Crude Buster (World FU version)" )
GAME( 1990, cbusterj, cbuster, twocrude, twocrude, twocrude, ROT0, "Data East Corporation", "Crude Buster (Japan)" )
GAME( 1990, twocrude, cbuster, twocrude, twocrude, twocrude, ROT0, "Data East USA", "Two Crude (US)" )
