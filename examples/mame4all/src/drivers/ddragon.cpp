#include "../vidhrdw/ddragon.cpp"

/***************************************************************************

Double Dragon     (c) 1987 Technos Japan
Double Dragon II  (c) 1988 Technos Japan

Driver by Carlos A. Lozano, Rob Rosenbrock, Phil Stroffolino, Ernesto Corvi

***************************************************************************/

#include "driver.h"
#include "cpu/m6800/m6800.h"
#include "cpu/m6809/m6809.h"
#include "cpu/z80/z80.h"

/* from vidhrdw */
extern unsigned char *ddragon_bgvideoram,*ddragon_fgvideoram;
extern int ddragon_scrollx_hi, ddragon_scrolly_hi;
extern unsigned char *ddragon_scrollx_lo;
extern unsigned char *ddragon_scrolly_lo;
int ddragon_vh_start(void);
void ddragon_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( ddragon_bgvideoram_w );
WRITE_HANDLER( ddragon_fgvideoram_w );
extern unsigned char *ddragon_spriteram;
extern int dd2_video;
/* end of extern code & data */

/* private globals */
static int dd_sub_cpu_busy;
static int sprite_irq, sound_irq, ym_irq;
static int adpcm_pos[2],adpcm_end[2],adpcm_idle[2];
/* end of private globals */

static void ddragon_init_machine( void )
{
	sprite_irq = HD63701_INT_NMI;
	sound_irq = M6809_INT_IRQ;
	ym_irq = M6809_FIRQ_LINE;
	dd2_video = 0;
	dd_sub_cpu_busy = 0x10;
	adpcm_idle[0] = adpcm_idle[1] = 1;
}

static void ddragonb_init_machine( void )
{
	sprite_irq = M6809_INT_NMI;
	sound_irq = M6809_INT_IRQ;
	ym_irq = M6809_FIRQ_LINE;
	dd2_video = 0;
	dd_sub_cpu_busy = 0x10;
	adpcm_idle[0] = adpcm_idle[1] = 1;
}

static void ddragon2_init_machine( void )
{
	sprite_irq = Z80_NMI_INT;
	sound_irq = Z80_NMI_INT;
	ym_irq = 0;
	dd2_video = 1;
	dd_sub_cpu_busy = 0x10;
}

static WRITE_HANDLER( ddragon_bankswitch_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	ddragon_scrolly_hi = ( ( data & 0x02 ) << 7 );
	ddragon_scrollx_hi = ( ( data & 0x01 ) << 8 );

	flip_screen_w(0,~data & 0x04);

	/* bit 3 unknown */

	if (data & 0x10)
		dd_sub_cpu_busy = 0x00;
	else if (dd_sub_cpu_busy == 0x00)
		cpu_cause_interrupt( 1, sprite_irq );

	cpu_setbank( 1,&RAM[ 0x10000 + ( 0x4000 * ( ( data & 0xe0) >> 5 ) ) ] );
}

static WRITE_HANDLER( ddragon_forcedIRQ_w )
{
	cpu_cause_interrupt( 0, M6809_INT_IRQ );
}

static READ_HANDLER( port4_r )
{
	int port = readinputport( 4 );

	return port | dd_sub_cpu_busy;
}

static READ_HANDLER( ddragon_spriteram_r )
{
	return ddragon_spriteram[offset];
}

static WRITE_HANDLER( ddragon_spriteram_w )
{
	if ( cpu_getactivecpu() == 1 && offset == 0 )
		dd_sub_cpu_busy = 0x10;

	ddragon_spriteram[offset] = data;
}

static WRITE_HANDLER( cpu_sound_command_w )
{
	soundlatch_w( offset, data );
	cpu_cause_interrupt( 2, sound_irq );
}

static WRITE_HANDLER( dd_adpcm_w )
{
	int chip = offset & 1;

	switch (offset/2)
	{
		case 3:
			adpcm_idle[chip] = 1;
			MSM5205_reset_w(chip,1);
			break;

		case 2:
			adpcm_pos[chip] = (data & 0x7f) * 0x200;
			break;

		case 1:
			adpcm_end[chip] = (data & 0x7f) * 0x200;
			break;

		case 0:
			adpcm_idle[chip] = 0;
			MSM5205_reset_w(chip,0);
			break;
	}
}

static void dd_adpcm_int(int chip)
{
	static int adpcm_data[2] = { -1, -1 };

	if (adpcm_pos[chip] >= adpcm_end[chip] || adpcm_pos[chip] >= 0x10000)
	{
		adpcm_idle[chip] = 1;
		MSM5205_reset_w(chip,1);
	}
	else if (adpcm_data[chip] != -1)
	{
		MSM5205_data_w(chip,adpcm_data[chip] & 0x0f);
		adpcm_data[chip] = -1;
	}
	else
	{
		unsigned char *ROM = memory_region(REGION_SOUND1) + 0x10000 * chip;

		adpcm_data[chip] = ROM[adpcm_pos[chip]++];
		MSM5205_data_w(chip,adpcm_data[chip] >> 4);
	}
}

static READ_HANDLER( dd_adpcm_status_r )
{
	return adpcm_idle[0] + (adpcm_idle[1] << 1);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x2000, 0x2fff, ddragon_spriteram_r },
	{ 0x3000, 0x37ff, MRA_RAM },
	{ 0x3800, 0x3800, input_port_0_r },
	{ 0x3801, 0x3801, input_port_1_r },
	{ 0x3802, 0x3802, port4_r },
	{ 0x3803, 0x3803, input_port_2_r },
	{ 0x3804, 0x3804, input_port_3_r },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x1000, 0x11ff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x1200, 0x13ff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x1400, 0x17ff, MWA_RAM },
	{ 0x1800, 0x1fff, ddragon_fgvideoram_w, &ddragon_fgvideoram },
	{ 0x2000, 0x2fff, ddragon_spriteram_w, &ddragon_spriteram },
	{ 0x3000, 0x37ff, ddragon_bgvideoram_w, &ddragon_bgvideoram },
	{ 0x3808, 0x3808, ddragon_bankswitch_w },
	{ 0x3809, 0x3809, MWA_RAM, &ddragon_scrollx_lo },
	{ 0x380a, 0x380a, MWA_RAM, &ddragon_scrolly_lo },
	{ 0x380b, 0x380d, MWA_RAM },	/* ??? */
	{ 0x380e, 0x380e, cpu_sound_command_w },
	{ 0x380f, 0x380f, ddragon_forcedIRQ_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress dd2_writemem[] =
{
	{ 0x0000, 0x17ff, MWA_RAM },
	{ 0x1800, 0x1fff, ddragon_fgvideoram_w, &ddragon_fgvideoram },
	{ 0x2000, 0x2fff, ddragon_spriteram_w, &ddragon_spriteram },
	{ 0x3000, 0x37ff, ddragon_bgvideoram_w, &ddragon_bgvideoram },
	{ 0x3808, 0x3808, ddragon_bankswitch_w },
	{ 0x3809, 0x3809, MWA_RAM, &ddragon_scrollx_lo },
	{ 0x380a, 0x380a, MWA_RAM, &ddragon_scrolly_lo },
	{ 0x380b, 0x380d, MWA_RAM },	/* ??? */
	{ 0x380e, 0x380e, cpu_sound_command_w },
	{ 0x380f, 0x380f, ddragon_forcedIRQ_w },
	{ 0x3c00, 0x3dff, paletteram_xxxxBBBBGGGGRRRR_split1_w, &paletteram },
	{ 0x3e00, 0x3fff, paletteram_xxxxBBBBGGGGRRRR_split2_w, &paletteram_2 },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sub_readmem[] =
{
	{ 0x0000, 0x0fff, MRA_RAM },
	{ 0x8000, 0x8fff, ddragon_spriteram_r },
	{ 0xc000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sub_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x8000, 0x8fff, ddragon_spriteram_w },
	{ 0xc000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x0fff, MRA_RAM },
	{ 0x1000, 0x1000, soundlatch_r },
	{ 0x1800, 0x1800, dd_adpcm_status_r },
	{ 0x2800, 0x2801, YM2151_status_port_0_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x2800, 0x2800, YM2151_register_port_0_w },
	{ 0x2801, 0x2801, YM2151_data_port_0_w },
	{ 0x3800, 0x3807, dd_adpcm_w },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress dd2_sub_readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xcfff, ddragon_spriteram_r },
	{ 0xd000, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress dd2_sub_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, ddragon_spriteram_w },
	{ 0xd000, 0xffff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress dd2_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x8801, 0x8801, YM2151_status_port_0_r },
	{ 0x9800, 0x9800, OKIM6295_status_0_r },
	{ 0xA000, 0xA000, soundlatch_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress dd2_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8800, 0x8800, YM2151_register_port_0_w },
	{ 0x8801, 0x8801, YM2151_data_port_0_w },
	{ 0x9800, 0x9800, OKIM6295_data_0_w },
	{ -1 }	/* end of table */
};



#define COMMON_PORT4	PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON3 ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_VBLANK ) \
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL ) /* sub cpu busy */ \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define COMMON_INPUT_PORTS PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 ) \
	PORT_START \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_START \
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) ) \
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) ) \
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_5C ) ) \
	PORT_DIPNAME( 0x40, 0x40, "Screen Orientation?" ) \
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) ) \
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) ) \
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )


INPUT_PORTS_START( ddragon )
	COMMON_INPUT_PORTS

    PORT_START      /* DSW1 */
    PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
    PORT_DIPSETTING(    0x02, "Easy")
    PORT_DIPSETTING(    0x03, "Normal")
    PORT_DIPSETTING(    0x01, "Hard")
    PORT_DIPSETTING(    0x00, "Very Hard")
    PORT_DIPNAME( 0x04, 0x04, DEF_STR( Demo_Sounds ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
    PORT_DIPSETTING(    0x10, "20k")
    PORT_DIPSETTING(    0x00, "40k" )
    PORT_DIPSETTING(    0x30, "30k and every 60k")
    PORT_DIPSETTING(    0x20, "40k and every 80k" )
    PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
    PORT_DIPSETTING(    0xc0, "2")
    PORT_DIPSETTING(    0x80, "3" )
    PORT_DIPSETTING(    0x40, "4")
    PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )

    COMMON_PORT4
INPUT_PORTS_END

INPUT_PORTS_START( ddragon2 )
	COMMON_INPUT_PORTS

	PORT_START      /* DSW1 */
    PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
    PORT_DIPSETTING(    0x02, "Easy")
    PORT_DIPSETTING(    0x03, "Normal")
    PORT_DIPSETTING(    0x01, "Medium")
    PORT_DIPSETTING(    0x00, "Hard")
    PORT_DIPNAME( 0x04, 0x04, DEF_STR( Demo_Sounds ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x04, DEF_STR( On ) )
    PORT_DIPNAME( 0x08, 0x08, "Hurricane Kick" )
    PORT_DIPSETTING(    0x00, "Easy" )
    PORT_DIPSETTING(    0x08, "Normal" )
    PORT_DIPNAME( 0x30, 0x30, "Timer" )
    PORT_DIPSETTING(    0x00, "60" )
    PORT_DIPSETTING(    0x10, "65" )
    PORT_DIPSETTING(    0x30, "70" )
    PORT_DIPSETTING(    0x20, "80" )
    PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
    PORT_DIPSETTING(    0xc0, "1" )
    PORT_DIPSETTING(    0x80, "2" )
    PORT_DIPSETTING(    0x40, "3" )
    PORT_DIPSETTING(    0x00, "4" )

	COMMON_PORT4
INPUT_PORTS_END

#undef COMMON_INPUT_PORTS
#undef COMMON_PORT4



static struct GfxLayout char_layout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 2, 4, 6 },
	{ 1, 0, 8*8+1, 8*8+0, 16*8+1, 16*8+0, 24*8+1, 24*8+0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	32*8
};

static struct GfxLayout tile_layout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+4, 0, 4 },
	{ 3, 2, 1, 0, 16*8+3, 16*8+2, 16*8+1, 16*8+0,
		  32*8+3, 32*8+2, 32*8+1, 32*8+0, 48*8+3, 48*8+2, 48*8+1, 48*8+0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		  8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	64*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &char_layout,   0, 8 },	/* colors   0-127 */
	{ REGION_GFX2, 0, &tile_layout, 128, 8 },	/* colors 128-255 */
	{ REGION_GFX3, 0, &tile_layout, 256, 8 },	/* colors 256-383 */
	{ -1 }
};



static void irq_handler(int irq)
{
	cpu_set_irq_line( 2, ym_irq , irq ? ASSERT_LINE : CLEAR_LINE );
}

static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3579545,	/* ??? */
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) },
	{ irq_handler }
};

static struct MSM5205interface msm5205_interface =
{
	2,					/* 2 chips             */
	384000,				/* 384KHz             */
	{ dd_adpcm_int, dd_adpcm_int },/* interrupt function */
	{ MSM5205_S48_4B, MSM5205_S64_4B },	/* 8kHz and 6kHz      */
	{ 40, 40 }				/* volume */
};

static struct OKIM6295interface okim6295_interface =
{
	1,              /* 1 chip */
	{ 8000 },           /* frequency (Hz) */
	{ REGION_SOUND1 },  /* memory region */
	{ 15 }
};

static int ddragon_interrupt(void)
{
    cpu_set_irq_line(0, 1, HOLD_LINE); /* hold the FIRQ line */
    cpu_set_nmi_line(0, PULSE_LINE); /* pulse the NMI line */
    return M6809_INT_NONE;
}



static struct MachineDriver machine_driver_ddragon =
{
	/* basic machine hardware */
	{
		{
 			CPU_HD6309,
			3579545,	/* 3.579545 Mhz */
			readmem,writemem,0,0,
			ddragon_interrupt,1
		},
		{
 			CPU_HD63701,
			2000000, /* 2 Mhz ???*/
			sub_readmem,sub_writemem,0,0,
			ignore_interrupt,0
		},
		{
 			CPU_HD6309 | CPU_AUDIO_CPU,	/* ? */
			3579545,	/* 3.579545 Mhz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0 /* irq on command */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION, /* frames per second, vblank duration */
	100, /* heavy interleaving to sync up sprite<->main cpu's */
	ddragon_init_machine,

	/* video hardware */
	32*8, 32*8,{ 1*8, 31*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	384, 384,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	ddragon_vh_start,
	0,
	ddragon_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_MSM5205,
			&msm5205_interface
		}
	}
};

static struct MachineDriver machine_driver_ddragonb =
{
	/* basic machine hardware */
	{
		{
 			CPU_HD6309,
			3579545,	/* 3.579545 Mhz */
			readmem,writemem,0,0,
			ddragon_interrupt,1
		},
		{
 			CPU_HD6309,	/* ? */
			12000000 / 3, /* 4 Mhz */
			sub_readmem,sub_writemem,0,0,
			ignore_interrupt,0
		},
		{
 			CPU_HD6309 | CPU_AUDIO_CPU,	/* ? */
			3579545,	/* 3.579545 Mhz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0 /* irq on command */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION, /* frames per second, vblank duration */
	100, /* heavy interleaving to sync up sprite<->main cpu's */
	ddragonb_init_machine,

	/* video hardware */
	32*8, 32*8,{ 1*8, 31*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	384, 384,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	ddragon_vh_start,
	0,
	ddragon_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_MSM5205,
			&msm5205_interface
		}
	}
};

static struct MachineDriver machine_driver_ddragon2 =
{
	/* basic machine hardware */
	{
		{
 			CPU_HD6309,
			3579545,	/* 3.579545 Mhz */
			readmem,dd2_writemem,0,0,
			ddragon_interrupt,1
		},
		{
			CPU_Z80,
			12000000 / 3, /* 4 Mhz */
			dd2_sub_readmem,dd2_sub_writemem,0,0,
			ignore_interrupt,0
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* 3.579545 Mhz */
			dd2_sound_readmem,dd2_sound_writemem,0,0,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION, /* frames per second, vblank duration */
	100, /* heavy interleaving to sync up sprite<->main cpu's */
	ddragon2_init_machine,

	/* video hardware */
	32*8, 32*8,{ 1*8, 31*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	384, 384,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	ddragon_vh_start,
	0,
	ddragon_vh_screenrefresh,

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


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( ddragon )
	ROM_REGION( 0x28000, REGION_CPU1 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "21j-1-5",      0x08000, 0x08000, 0x42045dfd )
	ROM_LOAD( "21j-2-3",      0x10000, 0x08000, 0x5779705e ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21j-3",        0x18000, 0x08000, 0x3bdea613 ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21j-4-1",      0x20000, 0x08000, 0x728f87b9 ) /* banked at 0x4000-0x8000 */

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sprite cpu */
	ROM_LOAD( "63701.bin",    0xc000, 0x4000, 0xf5232d03 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* audio cpu */
	ROM_LOAD( "21j-0-1",      0x08000, 0x08000, 0x9efa95bb )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "21j-5",        0x00000, 0x08000, 0x7a8b8db4 )	/* chars */

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "21j-a",        0x00000, 0x10000, 0x574face3 )	/* sprites */
	ROM_LOAD( "21j-b",        0x10000, 0x10000, 0x40507a76 )
	ROM_LOAD( "21j-c",        0x20000, 0x10000, 0xbb0bc76f )
	ROM_LOAD( "21j-d",        0x30000, 0x10000, 0xcb4f231b )
	ROM_LOAD( "21j-e",        0x40000, 0x10000, 0xa0a0c261 )
	ROM_LOAD( "21j-f",        0x50000, 0x10000, 0x6ba152f6 )
	ROM_LOAD( "21j-g",        0x60000, 0x10000, 0x3220a0b6 )
	ROM_LOAD( "21j-h",        0x70000, 0x10000, 0x65c7517d )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "21j-8",        0x00000, 0x10000, 0x7c435887 )	/* tiles */
	ROM_LOAD( "21j-9",        0x10000, 0x10000, 0xc6640aed )
	ROM_LOAD( "21j-i",        0x20000, 0x10000, 0x5effb0a0 )
	ROM_LOAD( "21j-j",        0x30000, 0x10000, 0x5fb42e7c )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* adpcm samples */
	ROM_LOAD( "21j-6",        0x00000, 0x10000, 0x34755de3 )
	ROM_LOAD( "21j-7",        0x10000, 0x10000, 0x904de6f8 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "21j-k-0",      0x0000, 0x0100, 0xfdb130a9 )	/* unknown */
	ROM_LOAD( "21j-l-0",      0x0100, 0x0200, 0x46339529 )	/* unknown */
ROM_END

ROM_START( ddragonu )
	ROM_REGION( 0x28000, REGION_CPU1 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "21a-1-5",      0x08000, 0x08000, 0xe24a6e11 )
	ROM_LOAD( "21j-2-3",      0x10000, 0x08000, 0x5779705e ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21a-3",        0x18000, 0x08000, 0xdbf24897 ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21a-4",        0x20000, 0x08000, 0x6ea16072 ) /* banked at 0x4000-0x8000 */

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sprite cpu */
	ROM_LOAD( "63701.bin",    0xc000, 0x4000, 0xf5232d03 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* audio cpu */
	ROM_LOAD( "21j-0-1",      0x08000, 0x08000, 0x9efa95bb )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "21j-5",        0x00000, 0x08000, 0x7a8b8db4 )	/* chars */

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "21j-a",        0x00000, 0x10000, 0x574face3 )	/* sprites */
	ROM_LOAD( "21j-b",        0x10000, 0x10000, 0x40507a76 )
	ROM_LOAD( "21j-c",        0x20000, 0x10000, 0xbb0bc76f )
	ROM_LOAD( "21j-d",        0x30000, 0x10000, 0xcb4f231b )
	ROM_LOAD( "21j-e",        0x40000, 0x10000, 0xa0a0c261 )
	ROM_LOAD( "21j-f",        0x50000, 0x10000, 0x6ba152f6 )
	ROM_LOAD( "21j-g",        0x60000, 0x10000, 0x3220a0b6 )
	ROM_LOAD( "21j-h",        0x70000, 0x10000, 0x65c7517d )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "21j-8",        0x00000, 0x10000, 0x7c435887 )	/* tiles */
	ROM_LOAD( "21j-9",        0x10000, 0x10000, 0xc6640aed )
	ROM_LOAD( "21j-i",        0x20000, 0x10000, 0x5effb0a0 )
	ROM_LOAD( "21j-j",        0x30000, 0x10000, 0x5fb42e7c )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* adpcm samples */
	ROM_LOAD( "21j-6",        0x00000, 0x10000, 0x34755de3 )
	ROM_LOAD( "21j-7",        0x10000, 0x10000, 0x904de6f8 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "21j-k-0",      0x0000, 0x0100, 0xfdb130a9 )	/* unknown */
	ROM_LOAD( "21j-l-0",      0x0100, 0x0200, 0x46339529 )	/* unknown */
ROM_END

ROM_START( ddragonb )
	ROM_REGION( 0x28000, REGION_CPU1 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "ic26",         0x08000, 0x08000, 0xae714964 )
	ROM_LOAD( "21j-2-3",      0x10000, 0x08000, 0x5779705e ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21a-3",        0x18000, 0x08000, 0xdbf24897 ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "ic23",         0x20000, 0x08000, 0x6c9f46fa ) /* banked at 0x4000-0x8000 */

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sprite cpu */
	ROM_LOAD( "ic38",         0x0c000, 0x04000, 0x6a6a0325 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* audio cpu */
	ROM_LOAD( "21j-0-1",      0x08000, 0x08000, 0x9efa95bb )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "21j-5",        0x00000, 0x08000, 0x7a8b8db4 )	/* chars */

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "21j-a",        0x00000, 0x10000, 0x574face3 )	/* sprites */
	ROM_LOAD( "21j-b",        0x10000, 0x10000, 0x40507a76 )
	ROM_LOAD( "21j-c",        0x20000, 0x10000, 0xbb0bc76f )
	ROM_LOAD( "21j-d",        0x30000, 0x10000, 0xcb4f231b )
	ROM_LOAD( "21j-e",        0x40000, 0x10000, 0xa0a0c261 )
	ROM_LOAD( "21j-f",        0x50000, 0x10000, 0x6ba152f6 )
	ROM_LOAD( "21j-g",        0x60000, 0x10000, 0x3220a0b6 )
	ROM_LOAD( "21j-h",        0x70000, 0x10000, 0x65c7517d )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "21j-8",        0x00000, 0x10000, 0x7c435887 )	/* tiles */
	ROM_LOAD( "21j-9",        0x10000, 0x10000, 0xc6640aed )
	ROM_LOAD( "21j-i",        0x20000, 0x10000, 0x5effb0a0 )
	ROM_LOAD( "21j-j",        0x30000, 0x10000, 0x5fb42e7c )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* adpcm samples */
	ROM_LOAD( "21j-6",        0x00000, 0x10000, 0x34755de3 )
	ROM_LOAD( "21j-7",        0x10000, 0x10000, 0x904de6f8 )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "21j-k-0",      0x0000, 0x0100, 0xfdb130a9 )	/* unknown */
	ROM_LOAD( "21j-l-0",      0x0100, 0x0200, 0x46339529 )	/* unknown */
ROM_END

ROM_START( ddragon2 )
	ROM_REGION( 0x28000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "26a9-04.bin",  0x08000, 0x8000, 0xf2cfc649 )
	ROM_LOAD( "26aa-03.bin",  0x10000, 0x8000, 0x44dd5d4b )
	ROM_LOAD( "26ab-0.bin",   0x18000, 0x8000, 0x49ddddcd )
	ROM_LOAD( "26ac-02.bin",  0x20000, 0x8000, 0x097eaf26 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* sprite CPU 64kb (Upper 16kb = 0) */
	ROM_LOAD( "26ae-0.bin",   0x00000, 0x10000, 0xea437867 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* music CPU, 64kb */
	ROM_LOAD( "26ad-0.bin",   0x00000, 0x8000, 0x75e36cd6 )

	ROM_REGION( 0x10000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "26a8-0.bin",   0x00000, 0x10000, 0x3ad1049c )	/* chars */

	ROM_REGION( 0xc0000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "26j0-0.bin",   0x00000, 0x20000, 0xdb309c84 )	/* sprites */
	ROM_LOAD( "26j1-0.bin",   0x20000, 0x20000, 0xc3081e0c )
	ROM_LOAD( "26af-0.bin",   0x40000, 0x20000, 0x3a615aad )
	ROM_LOAD( "26j2-0.bin",   0x60000, 0x20000, 0x589564ae )
	ROM_LOAD( "26j3-0.bin",   0x80000, 0x20000, 0xdaf040d6 )
	ROM_LOAD( "26a10-0.bin",  0xa0000, 0x20000, 0x6d16d889 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "26j4-0.bin",   0x00000, 0x20000, 0xa8c93e76 )	/* tiles */
	ROM_LOAD( "26j5-0.bin",   0x20000, 0x20000, 0xee555237 )

	ROM_REGION( 0x40000, REGION_SOUND1 ) /* adpcm samples */
	ROM_LOAD( "26j6-0.bin",   0x00000, 0x20000, 0xa84b2a29 )
	ROM_LOAD( "26j7-0.bin",   0x20000, 0x20000, 0xbc6a48d5 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "prom.16",      0x0000, 0x0200, 0x46339529 )	/* unknown (same as ddragon) */
ROM_END



GAME( 1987, ddragon,  0,       ddragon,  ddragon,  0, ROT0, "Technos", "Double Dragon (Japan)" )
GAME( 1987, ddragonu, ddragon, ddragon,  ddragon,  0, ROT0, "[Technos] (Taito America license)", "Double Dragon (US)" )
GAME( 1987, ddragonb, ddragon, ddragonb, ddragon,  0, ROT0, "bootleg", "Double Dragon (bootleg)" )
GAME( 1988, ddragon2, 0,       ddragon2, ddragon2, 0, ROT0, "Technos", "Double Dragon II - The Revenge" )
