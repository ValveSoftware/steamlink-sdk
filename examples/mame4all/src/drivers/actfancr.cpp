#include "../vidhrdw/actfancr.cpp"

/*******************************************************************************

	Act Fancer (Japan)				FD (c) 1989 Data East Corporation
	Act Fancer (World)				FE (c) 1989 Data East Corporation
	Trio The Punch (Japan)			FF (c) 1989 Data East Corporation

	The 'World' set has rom code FE, the 'Japan' set has rom code FD.

	Most Data East games give the Japanese version the earlier code, though
	there is no real difference between the sets.

	I believe the USA version of Act Fancer is called 'Out Fencer'

	Emulation by Bryan McPhail, mish@tendril.co.uk

*******************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6502/m6502.h"
#include "cpu/h6280/h6280.h"

void actfancr_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void triothep_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( actfancr_pf1_data_w );
READ_HANDLER( actfancr_pf1_data_r );
WRITE_HANDLER( actfancr_pf1_control_w );
WRITE_HANDLER( actfancr_pf2_data_w );
READ_HANDLER( actfancr_pf2_data_r );
WRITE_HANDLER( actfancr_pf2_control_w );
int actfancr_vh_start (void);
int triothep_vh_start (void);

extern unsigned char *actfancr_pf1_data,*actfancr_pf2_data,*actfancr_pf1_rowscroll_data;
static unsigned char *actfancr_ram;

/******************************************************************************/

static READ_HANDLER( actfan_control_0_r )
{
	return readinputport(2); /* VBL */
}

static READ_HANDLER( actfan_control_1_r )
{
	switch (offset) {
		case 0: return readinputport(0); /* Player 1 */
		case 1: return readinputport(1); /* Player 2 */
		case 2: return readinputport(3); /* Dip 1 */
		case 3: return readinputport(4); /* Dip 2 */
	}
	return 0xff;
}

static int trio_control_select;

static WRITE_HANDLER( triothep_control_select_w )
{
	trio_control_select=data;
}

static READ_HANDLER( triothep_control_r )
{
	switch (trio_control_select) {
		case 0: return readinputport(0); /* Player 1 */
		case 1: return readinputport(1); /* Player 2 */
		case 2: return readinputport(3); /* Dip 1 */
		case 3: return readinputport(4); /* Dip 2 */
		case 4: return readinputport(2); /* VBL */
	}

	return 0xff;
}

static WRITE_HANDLER( actfancr_sound_w )
{
	soundlatch_w(0,data & 0xff);
	cpu_cause_interrupt(1,M6502_INT_NMI);
}

/******************************************************************************/

static struct MemoryReadAddress actfan_readmem[] =
{
	{ 0x000000, 0x02ffff, MRA_ROM },
	{ 0x062000, 0x063fff, actfancr_pf1_data_r },
	{ 0x072000, 0x0727ff, actfancr_pf2_data_r },
	{ 0x100000, 0x1007ff, MRA_RAM },
	{ 0x130000, 0x130003, actfan_control_1_r },
	{ 0x140000, 0x140001, actfan_control_0_r },
	{ 0x120000, 0x1205ff, paletteram_r },
	{ 0x1f0000, 0x1f3fff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress actfan_writemem[] =
{
	{ 0x000000, 0x02ffff, MWA_ROM },
	{ 0x060000, 0x06001f, actfancr_pf1_control_w },
	{ 0x062000, 0x063fff, actfancr_pf1_data_w, &actfancr_pf1_data },
	{ 0x070000, 0x07001f, actfancr_pf2_control_w },
	{ 0x072000, 0x0727ff, actfancr_pf2_data_w, &actfancr_pf2_data },
	{ 0x100000, 0x1007ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x110000, 0x110001, buffer_spriteram_w },
	{ 0x120000, 0x1205ff, paletteram_xxxxBBBBGGGGRRRR_w, &paletteram },
	{ 0x150000, 0x150001, actfancr_sound_w },
	{ 0x1f0000, 0x1f3fff, MWA_RAM, &actfancr_ram }, /* Main ram */
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress triothep_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x044000, 0x045fff, actfancr_pf2_data_r },
	{ 0x064000, 0x0647ff, actfancr_pf1_data_r },
	{ 0x120000, 0x1207ff, MRA_RAM },
	{ 0x130000, 0x1305ff, paletteram_r },
	{ 0x140000, 0x140001, MRA_NOP }, /* Value doesn't matter */
	{ 0x1f0000, 0x1f3fff, MRA_RAM },
	{ 0x1ff000, 0x1ff001, triothep_control_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress triothep_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x040000, 0x04001f, actfancr_pf2_control_w },
	{ 0x044000, 0x045fff, actfancr_pf2_data_w, &actfancr_pf2_data },
	{ 0x046400, 0x0467ff, MWA_NOP }, /* Pf2 rowscroll - is it used? */
	{ 0x060000, 0x06001f, actfancr_pf1_control_w },
	{ 0x064000, 0x0647ff, actfancr_pf1_data_w, &actfancr_pf1_data },
	{ 0x066400, 0x0667ff, MWA_RAM, &actfancr_pf1_rowscroll_data },
	{ 0x100000, 0x100001, actfancr_sound_w },
	{ 0x110000, 0x110001, buffer_spriteram_w },
	{ 0x120000, 0x1207ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x130000, 0x1305ff, paletteram_xxxxBBBBGGGGRRRR_w, &paletteram },
	{ 0x1f0000, 0x1f3fff, MWA_RAM, &actfancr_ram }, /* Main ram */
	{ 0x1ff000, 0x1ff001, triothep_control_select_w },
	{ 0x1ff402, 0x1ff403, H6280_irq_status_w },
	{ -1 }  /* end of table */
};

/******************************************************************************/

static struct MemoryReadAddress dec0_s_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x3000, 0x3000, soundlatch_r },
	{ 0x3800, 0x3800, OKIM6295_status_0_r },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress dec0_s_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x0800, 0x0800, YM2203_control_port_0_w },
	{ 0x0801, 0x0801, YM2203_write_port_0_w },
	{ 0x1000, 0x1000, YM3812_control_port_0_w },
	{ 0x1001, 0x1001, YM3812_write_port_0_w },
	{ 0x3800, 0x3800, OKIM6295_data_0_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

/******************************************************************************/

INPUT_PORTS_START( actfancr )
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
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* start buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_BITX(0,  0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "100", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x20, "800000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( triothep )
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
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* start buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_START /* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "8" )
	PORT_DIPSETTING(    0x01, "10" )
	PORT_DIPSETTING(    0x03, "12" )
	PORT_DIPSETTING(    0x02, "14" )
	PORT_DIPNAME( 0x0c, 0x0c, "Difficulty (Time)" )
	PORT_DIPSETTING(    0x08, "Easy (130)" )
	PORT_DIPSETTING(    0x0c, "Normal (100)" )
	PORT_DIPSETTING(    0x04, "Hard (70)" )
	PORT_DIPSETTING(    0x00, "Hardest (60)" )
	PORT_DIPNAME( 0x10, 0x10, "Bonus Lives" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout chars =
{
	8,8,	/* 8*8 chars */
	4096,
	4,		/* 4 bits per pixel  */
	{ 0x08000*8, 0x18000*8, 0x00000*8, 0x10000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout tiles =
{
	16,16,	/* 16*16 sprites */
	2048,
	4,
	{ 0, 0x10000*8, 0x20000*8,0x30000*8 },	/* plane offset */
	{ 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
			0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};

static struct GfxLayout sprites =
{
	16,16,	/* 16*16 sprites */
	2048+1024,
	4,
	{ 0, 0x18000*8, 0x30000*8, 0x48000*8 },	/* plane offset */
	{ 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
			0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo actfan_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &chars,       0, 16 },
	{ REGION_GFX2, 0, &sprites,   512, 16 },
	{ REGION_GFX3, 0, &tiles,     256, 16 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo triothep_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &chars,       0, 16 },
	{ REGION_GFX2, 0, &sprites,   256, 16 },
	{ REGION_GFX3, 0, &tiles,     512, 16 },
	{ -1 } /* end of array */
};

/******************************************************************************/

static void sound_irq(int linestate)
{
	cpu_set_irq_line(1,0,linestate); /* IRQ */
}

static struct YM2203interface ym2203_interface =
{
	1,
	1500000, /* Should be accurate */
	{ YM2203_VOL(50,90) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct YM3812interface ym3812_interface =
{
	1,			/* 1 chip */
	3000000,	/* 3.000000 MHz (Should be accurate) */
	{ 45 },
	{ sound_irq },
};

static struct OKIM6295interface okim6295_interface =
{
	1,              /* 1 chip */
	{ 7759 },       /* frequency */
	{ REGION_SOUND1 },
	{ 85 }
};

/******************************************************************************/

static int actfan_interrupt(void)
{
	return H6280_INT_IRQ1;
}

static struct MachineDriver machine_driver_actfancr =
{
	/* basic machine hardware */
	{
		{
			CPU_H6280,
			21477200/3, /* Should be accurate */
			actfan_readmem,actfan_writemem,0,0,
			actfan_interrupt,1 /* VBL */
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000, /* Should be accurate */
			dec0_s_readmem,dec0_s_writemem,0,0,
			ignore_interrupt,0	/* Interrupts from OPL chip */
		}
	},
	60, 529,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written*/
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	actfan_gfxdecodeinfo,
	768, 768,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_BUFFERS_SPRITERAM | VIDEO_UPDATE_AFTER_VBLANK,
	0,
	actfancr_vh_start,
	0,
	actfancr_vh_screenrefresh,

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
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver machine_driver_triothep =
{
	/* basic machine hardware */
	{
		{
			CPU_H6280,
			21477200/3, /* Should be accurate */
			triothep_readmem,triothep_writemem,0,0,
			actfan_interrupt,1 /* VBL */
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000, /* Should be accurate */
			dec0_s_readmem,dec0_s_writemem,0,0,
			ignore_interrupt,0	/* Interrupts from OPL chip */
		}
	},
	60, 529,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written*/
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	triothep_gfxdecodeinfo,
	768, 768,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_BUFFERS_SPRITERAM | VIDEO_UPDATE_AFTER_VBLANK,
	0,
	triothep_vh_start,
	0,
	triothep_vh_screenrefresh,

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
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

/******************************************************************************/

ROM_START( actfancr )
	ROM_REGION( 0x200000, REGION_CPU1 ) /* Need to allow full RAM allocation for now */
	ROM_LOAD( "fe08-2.bin", 0x00000, 0x10000, 0x0d36fbfa )
	ROM_LOAD( "fe09-2.bin", 0x10000, 0x10000, 0x27ce2bb1 )
	ROM_LOAD( "10",   0x20000, 0x10000, 0xcabad137 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 6502 Sound CPU */
	ROM_LOAD( "17-1", 0x08000, 0x8000, 0x289ad106 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15", 0x00000, 0x10000, 0xa1baf21e ) /* Chars */
	ROM_LOAD( "16", 0x10000, 0x10000, 0x22e64730 )

	ROM_REGION( 0x60000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "02", 0x00000, 0x10000, 0xb1db0efc ) /* Sprites */
	ROM_LOAD( "03", 0x10000, 0x08000, 0xf313e04f )
	ROM_LOAD( "06", 0x18000, 0x10000, 0x8cb6dd87 )
	ROM_LOAD( "07", 0x28000, 0x08000, 0xdd345def )
	ROM_LOAD( "00", 0x30000, 0x10000, 0xd50a9550 )
	ROM_LOAD( "01", 0x40000, 0x08000, 0x34935e93 )
	ROM_LOAD( "04", 0x48000, 0x10000, 0xbcf41795 )
	ROM_LOAD( "05", 0x58000, 0x08000, 0xd38b94aa )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "14", 0x00000, 0x10000, 0xd6457420 ) /* Tiles */
	ROM_LOAD( "12", 0x10000, 0x10000, 0x08787b7a )
	ROM_LOAD( "13", 0x20000, 0x10000, 0xc30c37dc )
	ROM_LOAD( "11", 0x30000, 0x10000, 0x1f006d9f )

	ROM_REGION( 0x10000, REGION_SOUND1 ) /* ADPCM sounds */
	ROM_LOAD( "18",   0x00000, 0x10000, 0x5c55b242 )
ROM_END

ROM_START( actfanc1 )
	ROM_REGION( 0x200000, REGION_CPU1 ) /* Need to allow full RAM allocation for now */
	ROM_LOAD( "08-1", 0x00000, 0x10000, 0x3bf214a4 )
	ROM_LOAD( "09-1", 0x10000, 0x10000, 0x13ae78d5 )
	ROM_LOAD( "10",   0x20000, 0x10000, 0xcabad137 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 6502 Sound CPU */
	ROM_LOAD( "17-1", 0x08000, 0x8000, 0x289ad106 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15", 0x00000, 0x10000, 0xa1baf21e ) /* Chars */
	ROM_LOAD( "16", 0x10000, 0x10000, 0x22e64730 )

	ROM_REGION( 0x60000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "02", 0x00000, 0x10000, 0xb1db0efc ) /* Sprites */
	ROM_LOAD( "03", 0x10000, 0x08000, 0xf313e04f )
	ROM_LOAD( "06", 0x18000, 0x10000, 0x8cb6dd87 )
	ROM_LOAD( "07", 0x28000, 0x08000, 0xdd345def )
	ROM_LOAD( "00", 0x30000, 0x10000, 0xd50a9550 )
	ROM_LOAD( "01", 0x40000, 0x08000, 0x34935e93 )
	ROM_LOAD( "04", 0x48000, 0x10000, 0xbcf41795 )
	ROM_LOAD( "05", 0x58000, 0x08000, 0xd38b94aa )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "14", 0x00000, 0x10000, 0xd6457420 ) /* Tiles */
	ROM_LOAD( "12", 0x10000, 0x10000, 0x08787b7a )
	ROM_LOAD( "13", 0x20000, 0x10000, 0xc30c37dc )
	ROM_LOAD( "11", 0x30000, 0x10000, 0x1f006d9f )

	ROM_REGION( 0x10000, REGION_SOUND1 ) /* ADPCM sounds */
	ROM_LOAD( "18",   0x00000, 0x10000, 0x5c55b242 )
ROM_END

ROM_START( actfancj )
	ROM_REGION( 0x200000, REGION_CPU1 ) /* Need to allow full RAM allocation for now */
	ROM_LOAD( "fd08-1.bin", 0x00000, 0x10000, 0x69004b60 )
	ROM_LOAD( "fd09-1.bin", 0x10000, 0x10000, 0xa455ae3e )
	ROM_LOAD( "10",   0x20000, 0x10000, 0xcabad137 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 6502 Sound CPU */
	ROM_LOAD( "17-1", 0x08000, 0x8000, 0x289ad106 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "15", 0x00000, 0x10000, 0xa1baf21e ) /* Chars */
	ROM_LOAD( "16", 0x10000, 0x10000, 0x22e64730 )

	ROM_REGION( 0x60000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "02", 0x00000, 0x10000, 0xb1db0efc ) /* Sprites */
	ROM_LOAD( "03", 0x10000, 0x08000, 0xf313e04f )
	ROM_LOAD( "06", 0x18000, 0x10000, 0x8cb6dd87 )
	ROM_LOAD( "07", 0x28000, 0x08000, 0xdd345def )
	ROM_LOAD( "00", 0x30000, 0x10000, 0xd50a9550 )
	ROM_LOAD( "01", 0x40000, 0x08000, 0x34935e93 )
	ROM_LOAD( "04", 0x48000, 0x10000, 0xbcf41795 )
	ROM_LOAD( "05", 0x58000, 0x08000, 0xd38b94aa )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "14", 0x00000, 0x10000, 0xd6457420 ) /* Tiles */
	ROM_LOAD( "12", 0x10000, 0x10000, 0x08787b7a )
	ROM_LOAD( "13", 0x20000, 0x10000, 0xc30c37dc )
	ROM_LOAD( "11", 0x30000, 0x10000, 0x1f006d9f )

	ROM_REGION( 0x10000, REGION_SOUND1 ) /* ADPCM sounds */
	ROM_LOAD( "18",   0x00000, 0x10000, 0x5c55b242 )
ROM_END

ROM_START( triothep )
	ROM_REGION( 0x200000, REGION_CPU1 ) /* Need to allow full RAM allocation for now */
	ROM_LOAD( "ff16",     0x00000, 0x20000, 0x84d7e1b6 )
	ROM_LOAD( "ff15.bin", 0x20000, 0x10000, 0x6eada47c )
	ROM_LOAD( "ff14.bin", 0x30000, 0x10000, 0x4ba7de4a )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 6502 Sound CPU */
	ROM_LOAD( "ff18.bin", 0x00000, 0x10000, 0x9de9ee63 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ff12.bin", 0x00000, 0x10000, 0x15fb49f2 ) /* Chars */
	ROM_LOAD( "ff13.bin", 0x10000, 0x10000, 0xe20c9623 )

	ROM_REGION( 0x60000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ff11.bin", 0x00000, 0x10000, 0x19e885c7 ) /* Sprites */
	ROM_LOAD( "ff10.bin", 0x10000, 0x08000, 0x4b6b477a )
	ROM_LOAD( "ff09.bin", 0x18000, 0x10000, 0x79c6bc0e )
	ROM_LOAD( "ff08.bin", 0x28000, 0x08000, 0x1391e445 )
	ROM_LOAD( "ff03.bin", 0x30000, 0x10000, 0xb36ad42d )
	ROM_LOAD( "ff02.bin", 0x40000, 0x08000, 0x6b9d24ce )
	ROM_LOAD( "ff01.bin", 0x48000, 0x10000, 0x68d80a66 )
	ROM_LOAD( "ff00.bin", 0x58000, 0x08000, 0x41232442 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ff04.bin", 0x00000, 0x10000, 0x7cea3c87 ) /* Tiles */
	ROM_LOAD( "ff06.bin", 0x10000, 0x10000, 0x5e7f3e8f )
	ROM_LOAD( "ff05.bin", 0x20000, 0x10000, 0x8bb13f05 )
	ROM_LOAD( "ff07.bin", 0x30000, 0x10000, 0x0d7affc3 )

	ROM_REGION( 0x10000, REGION_SOUND1 ) /* ADPCM sounds */
	ROM_LOAD( "ff17.bin", 0x00000, 0x10000, 0xf0ab0d05 )
ROM_END

/******************************************************************************/

static READ_HANDLER( cycle_r )
{
	int pc=cpu_get_pc();
	int ret=actfancr_ram[0x26];

	if (offset==1) return actfancr_ram[0x27];

	if (pc==0xe29a && ret==0) {
		cpu_spinuntil_int();
		return 1;
	}

	return ret;
}

static READ_HANDLER( cyclej_r )
{
	int pc=cpu_get_pc();
	int ret=actfancr_ram[0x26];

	if (offset==1) return actfancr_ram[0x27];

	if (pc==0xe2b1 && ret==0) {
		cpu_spinuntil_int();
		return 1;
	}

	return ret;
}

static void init_actfancr(void)
{
	install_mem_read_handler(0, 0x1f0026, 0x1f0027, cycle_r);
}

static void init_actfancj(void)
{
	install_mem_read_handler(0, 0x1f0026, 0x1f0027, cyclej_r);
}



GAME( 1989, actfancr, 0,        actfancr, actfancr, actfancr, ROT0, "Data East Corporation", "Act-Fancer Cybernetick Hyper Weapon (World revision 2)" )
GAME( 1989, actfanc1, actfancr, actfancr, actfancr, actfancr, ROT0, "Data East Corporation", "Act-Fancer Cybernetick Hyper Weapon (World revision 1)" )
GAME( 1989, actfancj, actfancr, actfancr, actfancr, actfancj, ROT0, "Data East Corporation", "Act-Fancer Cybernetick Hyper Weapon (Japan revision 1)" )
GAME( 1989, triothep, 0,        triothep, triothep, 0,        ROT0, "Data East Corporation", "Trio The Punch - Never Forget Me... (Japan)" )
