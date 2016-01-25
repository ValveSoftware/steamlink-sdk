#include "../vidhrdw/brkthru.cpp"

/***************************************************************************
Break Thru Doc. Data East (1986)

driver by Phil Stroffolino

UNK-1.1    (16Kb)  Code (4000-7FFF)
UNK-1.2    (32Kb)  Main 6809 (8000-FFFF)
UNK-1.3    (32Kb)  Mapped (2000-3FFF)
UNK-1.4    (32Kb)  Mapped (2000-3FFF)

UNK-1.5    (32Kb)  Sound 6809 (8000-FFFF)

Background has 4 banks, with 256 16x16x8 tiles in each bank.
UNK-1.6    (32Kb)  GFX Background
UNK-1.7    (32Kb)  GFX Background
UNK-1.8    (32Kb)  GFX Background

UNK-1.9    (32Kb)  GFX Sprites
UNK-1.10   (32Kb)  GFX Sprites
UNK-1.11   (32Kb)  GFX Sprites

Text has 256 8x8x4 characters.
UNK-1.12   (8Kb)   GFX Text

**************************************************************************
Memory Map for Main CPU by Carlos A. Lozano
**************************************************************************

MAIN CPU
0000-03FF                                   W                   Plane0
0400-0BFF                                  R/W                  RAM
0C00-0FFF                                   W                   Plane2(Background)
1000-10FF                                   W                   Plane1(sprites)
1100-17FF                                  R/W                  RAM
1800-180F                                  R/W                  In/Out
1810-1FFF                                  R/W                  RAM (unmapped?)
2000-3FFF                                   R                   ROM Mapped(*)
4000-7FFF                                   R                   ROM(UNK-1.1)
8000-FFFF                                   R                   ROM(UNK-1.2)

Interrupts: Reset, NMI, IRQ
The test routine is at F000

Sound: YM2203 and YM3526 driven by 6809.  Sound added by Bryan McPhail, 1/4/98.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"


unsigned char *brkthru_nmi_enable; /* needs to be tracked down */
extern unsigned char *brkthru_videoram;
extern size_t brkthru_videoram_size;

WRITE_HANDLER( brkthru_1800_w );
int brkthru_vh_start(void);
void brkthru_vh_stop(void);
void brkthru_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void brkthru_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static int nmi_enable;

WRITE_HANDLER( brkthru_1803_w )
{
	/* bit 0 = NMI enable */
	nmi_enable = ~data & 1;

	/* bit 1 = ? maybe IRQ acknowledge */
}
WRITE_HANDLER( darwin_0803_w )
{
	/* bit 0 = NMI enable */
	/*nmi_enable = ~data & 1;*/
	//logerror("0803 %02X\n",data);
        nmi_enable = data;
	/* bit 1 = ? maybe IRQ acknowledge */
}

WRITE_HANDLER( brkthru_soundlatch_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt(1,M6809_INT_NMI);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },		/* Plane 0: Text */
	{ 0x0400, 0x0bff, MRA_RAM },
	{ 0x0c00, 0x0fff, MRA_RAM },		/* Plane 2  Background */
	{ 0x1000, 0x10ff, MRA_RAM },		/* Plane 1: Sprites */
	{ 0x1100, 0x17ff, MRA_RAM },
	{ 0x1800, 0x1800, input_port_0_r },	/* player controls, player start */
	{ 0x1801, 0x1801, input_port_1_r },	/* cocktail player controls */
	{ 0x1802, 0x1802, input_port_3_r },	/* DSW 0 */
	{ 0x1803, 0x1803, input_port_2_r },	/* coin input & DSW */
	{ 0x2000, 0x3fff, MRA_BANK1 },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM, &brkthru_videoram, &brkthru_videoram_size },
	{ 0x0400, 0x0bff, MWA_RAM },
	{ 0x0c00, 0x0fff, videoram_w, &videoram, &videoram_size },
	{ 0x1000, 0x10ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x1100, 0x17ff, MWA_RAM },
	{ 0x1800, 0x1801, brkthru_1800_w },	/* bg scroll and color, ROM bank selection, flip screen */
	{ 0x1802, 0x1802, brkthru_soundlatch_w },
	{ 0x1803, 0x1803, brkthru_1803_w },	/* NMI enable, + ? */
	{ 0x2000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};
static struct MemoryReadAddress darwin_readmem[] =
{
	{ 0x1000, 0x13ff, MRA_RAM },		/* Plane 0: Text */
	{ 0x0400, 0x07ff, MRA_RAM },
	{ 0x1c00, 0x1fff, MRA_RAM },		/* Plane 2  Background */
	{ 0x0000, 0x00ff, MRA_RAM },		/* Plane 1: Sprites */
 	{ 0x1400, 0x1bff, MRA_RAM },
	{ 0x0800, 0x0800, input_port_0_r },	/* player controls, player start */
	{ 0x0801, 0x0801, input_port_1_r },	/* cocktail player controls */
	{ 0x0802, 0x0802, input_port_3_r },	/* DSW 0 */
	{ 0x0803, 0x0803, input_port_2_r },	/* coin input & DSW */
	{ 0x2000, 0x3fff, MRA_BANK1 },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress darwin_writemem[] =
{
	{ 0x1000, 0x13ff, MWA_RAM, &brkthru_videoram, &brkthru_videoram_size },
	{ 0x1c00, 0x1fff, videoram_w, &videoram, &videoram_size },
	{ 0x0000, 0x00ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x1400, 0x1bff, MWA_RAM },
	{ 0x0100, 0x01ff, MWA_NOP  }, /*tidyup, nothing realy here?*/
	{ 0x0800, 0x0801, brkthru_1800_w },     /* bg scroll and color, ROM bank selection, flip screen */
	{ 0x0802, 0x0802, brkthru_soundlatch_w },
	{ 0x0803, 0x0803, darwin_0803_w },     /* NMI enable, + ? */
	{ 0x2000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x4000, 0x4000, soundlatch_r },
	{ 0x6000, 0x6000, YM2203_status_port_0_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2000, YM3526_control_port_0_w  },
	{ 0x2001, 0x2001, YM3526_write_port_0_w },
	{ 0x6000, 0x6000, YM2203_control_port_0_w },
	{ 0x6001, 0x6001, YM2203_write_port_0_w },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};



int brkthru_interrupt(void)
{
	if (cpu_getiloops() == 0)
	{
		if (nmi_enable) return nmi_interrupt();
	}
	else
	{
		/* generate IRQ on coin insertion */
		if ((readinputport(2) & 0xe0) != 0xe0) return interrupt();
	}

	return ignore_interrupt();
}

INPUT_PORTS_START( brkthru )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )	/* used only by the self test */

	PORT_START	/* IN2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_BITX(0,        0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "99", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1, 2 )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( darwin )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START	/* IN2 modified by Shingo Suzuki 1999/11/02 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x02, "20k 50k and every 50k" )
	PORT_DIPSETTING(    0x00, "30k 80k and every 80k" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x0c, "Easy" )
	PORT_DIPSETTING(    0x08, "Medium" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_LOW, IPT_COIN1, 2 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_LOW, IPT_COIN2, 2 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1, 2 )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 chars */
	256,	/* 256 characters */
	3,	/* 3 bits per pixel */
	{ 512*8*8+4, 0, 4 },	/* plane offset */
	{ 256*8*8+0, 256*8*8+1, 256*8*8+2, 256*8*8+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout tilelayout1 =
{
	16,16,	/* 16*16 tiles */
	128,	/* 128 tiles */
	3,	/* 3 bits per pixel */
	{ 0x4000*8+4, 0, 4 },	/* plane offset */
	{ 0, 1, 2, 3, 1024*8*8+0, 1024*8*8+1, 1024*8*8+2, 1024*8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+1024*8*8+0, 16*8+1024*8*8+1, 16*8+1024*8*8+2, 16*8+1024*8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every tile takes 32 consecutive bytes */
};

static struct GfxLayout tilelayout2 =
{
	16,16,	/* 16*16 tiles */
	128,	/* 128 tiles */
	3,	/* 3 bits per pixel */
	{ 0x3000*8+0, 0, 4 },	/* plane offset */
	{ 0, 1, 2, 3, 1024*8*8+0, 1024*8*8+1, 1024*8*8+2, 1024*8*8+3,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+1024*8*8+0, 16*8+1024*8*8+1, 16*8+1024*8*8+2, 16*8+1024*8*8+3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every tile takes 32 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	1024,	/* 1024 sprites */
	3,	/* 3 bits per pixel */
	{ 2*1024*32*8, 1024*32*8, 0 },	/* plane offset */
	{ 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
			0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &charlayout,      0, 1 },
	{ REGION_GFX2, 0x00000, &tilelayout1, 8+8*8, 16 },
	{ REGION_GFX2, 0x01000, &tilelayout2, 8+8*8, 16 },
	{ REGION_GFX2, 0x08000, &tilelayout1, 8+8*8, 16 },
	{ REGION_GFX2, 0x09000, &tilelayout2, 8+8*8, 16 },
	{ REGION_GFX2, 0x10000, &tilelayout1, 8+8*8, 16 },
	{ REGION_GFX2, 0x11000, &tilelayout2, 8+8*8, 16 },
	{ REGION_GFX2, 0x18000, &tilelayout1, 8+8*8, 16 },
	{ REGION_GFX2, 0x19000, &tilelayout2, 8+8*8, 16 },
	{ REGION_GFX3, 0x00000, &spritelayout,    8, 8 },
	{ -1 } /* end of array */
};

/* handler called by the 3812 emulator when the internal timers cause an IRQ */
static void irqhandler(int linestate)
{
	cpu_set_irq_line(1,0,linestate);
	//cpu_cause_interrupt(1,M6809_INT_IRQ);
}

static struct YM2203interface ym2203_interface =
{
	1,
	1500000,	/* Unknown */
	{ YM2203_VOL(25,25) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct YM3526interface ym3526_interface =
{
	1,			/* 1 chip (no more supported) */
	3000000,	/* 3.000000 MHz ? (partially supported) */
	{ 255 },		/* (not supported) */
	{ irqhandler },
};



static struct MachineDriver machine_driver_brkthru =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			1250000,        /* 1.25 Mhz ? */
			readmem,writemem,0,0,
			brkthru_interrupt,2
		},
		{
			CPU_M6809 | CPU_AUDIO_CPU,
			1250000,        /* 1.25 Mhz ? */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM3526 */
		}
	},
	58, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration (not sure) */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,	/* init machine */

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 1*8, 31*8-1 },	/* not sure */
	gfxdecodeinfo,
	256,8+8*8+16*8,
	brkthru_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	brkthru_vh_start,
	brkthru_vh_stop,
	brkthru_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3526,
			&ym3526_interface
		}
	}
};

static struct MachineDriver machine_driver_darwin =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			1500000,        /* 1.25 Mhz ? */
			darwin_readmem,darwin_writemem,0,0,
			brkthru_interrupt,2
		},
		{
			CPU_M6809 | CPU_AUDIO_CPU,
			1500000,        /* 1.25 Mhz ? */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are caused by the YM3526 */
		}
	},
	58, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration / 60->58 tuned by Shingo Suzuki 1999/10/16 */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,	/* init machine */

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo,
	256,8+8*8+16*8,
	brkthru_vh_convert_color_prom,

	VIDEO_TYPE_RASTER ,
	0,
	brkthru_vh_start,
	brkthru_vh_stop,
	brkthru_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3526,
			&ym3526_interface
		}
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( brkthru )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for main CPU + 64k for banked ROMs */
	ROM_LOAD( "brkthru.1",    0x04000, 0x4000, 0xcfb4265f )
	ROM_LOAD( "brkthru.2",    0x08000, 0x8000, 0xfa8246d9 )
	ROM_LOAD( "brkthru.4",    0x10000, 0x8000, 0x8cabf252 )
	ROM_LOAD( "brkthru.3",    0x18000, 0x8000, 0x2f2c40c2 )

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "brkthru.12",   0x00000, 0x2000, 0x58c0b29b )	/* characters */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	/* background */
	/* we do a lot of scatter loading here, to place the data in a format */
	/* which can be decoded by MAME's standard functions */
	ROM_LOAD( "brkthru.7",    0x00000, 0x4000, 0x920cc56a )	/* bitplanes 1,2 for bank 1,2 */
	ROM_CONTINUE(             0x08000, 0x4000 )				/* bitplanes 1,2 for bank 3,4 */
	ROM_LOAD( "brkthru.6",    0x10000, 0x4000, 0xfd3cee40 )	/* bitplanes 1,2 for bank 5,6 */
	ROM_CONTINUE(             0x18000, 0x4000 )				/* bitplanes 1,2 for bank 7,8 */
	ROM_LOAD( "brkthru.8",    0x04000, 0x1000, 0xf67ee64e )	/* bitplane 3 for bank 1,2 */
	ROM_CONTINUE(             0x06000, 0x1000 )
	ROM_CONTINUE(             0x0c000, 0x1000 )				/* bitplane 3 for bank 3,4 */
	ROM_CONTINUE(             0x0e000, 0x1000 )
	ROM_CONTINUE(             0x14000, 0x1000 )				/* bitplane 3 for bank 5,6 */
	ROM_CONTINUE(             0x16000, 0x1000 )
	ROM_CONTINUE(             0x1c000, 0x1000 )				/* bitplane 3 for bank 7,8 */
	ROM_CONTINUE(             0x1e000, 0x1000 )

	ROM_REGION( 0x18000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "brkthru.9",    0x00000, 0x8000, 0xf54e50a7 )	/* sprites */
	ROM_LOAD( "brkthru.10",   0x08000, 0x8000, 0xfd156945 )
	ROM_LOAD( "brkthru.11",   0x10000, 0x8000, 0xc152a99b )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "brkthru.13",   0x0000, 0x0100, 0xaae44269 ) /* red and green component */
	ROM_LOAD( "brkthru.14",   0x0100, 0x0100, 0xf2d4822a ) /* blue component */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64K for sound CPU */
	ROM_LOAD( "brkthru.5",    0x8000, 0x8000, 0xc309435f )
ROM_END

ROM_START( brkthruj )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for main CPU + 64k for banked ROMs */
	ROM_LOAD( "1",            0x04000, 0x4000, 0x09bd60ee )
	ROM_LOAD( "2",            0x08000, 0x8000, 0xf2b2cd1c )
	ROM_LOAD( "4",            0x10000, 0x8000, 0xb42b3359 )
	ROM_LOAD( "brkthru.3",    0x18000, 0x8000, 0x2f2c40c2 )

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "12",   0x00000, 0x2000, 0x3d9a7003 )	/* characters */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	/* background */
	/* we do a lot of scatter loading here, to place the data in a format */
	/* which can be decoded by MAME's standard functions */
	ROM_LOAD( "brkthru.7",    0x00000, 0x4000, 0x920cc56a )	/* bitplanes 1,2 for bank 1,2 */
	ROM_CONTINUE(             0x08000, 0x4000 )				/* bitplanes 1,2 for bank 3,4 */
	ROM_LOAD( "6",            0x10000, 0x4000, 0xcb47b395 )	/* bitplanes 1,2 for bank 5,6 */
	ROM_CONTINUE(             0x18000, 0x4000 )				/* bitplanes 1,2 for bank 7,8 */
	ROM_LOAD( "8",            0x04000, 0x1000, 0x5e5a2cd7 )	/* bitplane 3 for bank 1,2 */
	ROM_CONTINUE(             0x06000, 0x1000 )
	ROM_CONTINUE(             0x0c000, 0x1000 )				/* bitplane 3 for bank 3,4 */
	ROM_CONTINUE(             0x0e000, 0x1000 )
	ROM_CONTINUE(             0x14000, 0x1000 )				/* bitplane 3 for bank 5,6 */
	ROM_CONTINUE(             0x16000, 0x1000 )
	ROM_CONTINUE(             0x1c000, 0x1000 )				/* bitplane 3 for bank 7,8 */
	ROM_CONTINUE(             0x1e000, 0x1000 )

	ROM_REGION( 0x18000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "brkthru.9",    0x00000, 0x8000, 0xf54e50a7 )	/* sprites */
	ROM_LOAD( "brkthru.10",   0x08000, 0x8000, 0xfd156945 )
	ROM_LOAD( "brkthru.11",   0x10000, 0x8000, 0xc152a99b )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "brkthru.13",   0x0000, 0x0100, 0xaae44269 ) /* red and green component */
	ROM_LOAD( "brkthru.14",   0x0100, 0x0100, 0xf2d4822a ) /* blue component */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64K for sound CPU */
	ROM_LOAD( "brkthru.5",    0x8000, 0x8000, 0xc309435f )
ROM_END

ROM_START( darwin )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for main CPU + 64k for banked ROMs */
	ROM_LOAD( "darw_04.rom",  0x04000, 0x4000, 0x0eabf21c )
	ROM_LOAD( "darw_05.rom",  0x08000, 0x8000, 0xe771f864 )
	ROM_LOAD( "darw_07.rom",  0x10000, 0x8000, 0x97ac052c )
	ROM_LOAD( "darw_06.rom",  0x18000, 0x8000, 0x2a9fb208 )

	ROM_REGION( 0x02000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "darw_09.rom",  0x00000, 0x2000, 0x067b4cf5 )   /* characters */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	/* background */
	/* we do a lot of scatter loading here, to place the data in a format */
	/* which can be decoded by MAME's standard functions */
	ROM_LOAD( "darw_03.rom",  0x00000, 0x4000, 0x57d0350d )	/* bitplanes 1,2 for bank 1,2 */
	ROM_CONTINUE(             0x08000, 0x4000 )				/* bitplanes 1,2 for bank 3,4 */
	ROM_LOAD( "darw_02.rom",  0x10000, 0x4000, 0x559a71ab )	/* bitplanes 1,2 for bank 5,6 */
	ROM_CONTINUE(             0x18000, 0x4000 )				/* bitplanes 1,2 for bank 7,8 */
	ROM_LOAD( "darw_01.rom",  0x04000, 0x1000, 0x15a16973 )	/* bitplane 3 for bank 1,2 */
	ROM_CONTINUE(             0x06000, 0x1000 )
	ROM_CONTINUE(             0x0c000, 0x1000 )				/* bitplane 3 for bank 3,4 */
	ROM_CONTINUE(             0x0e000, 0x1000 )
	ROM_CONTINUE(             0x14000, 0x1000 )				/* bitplane 3 for bank 5,6 */
	ROM_CONTINUE(             0x16000, 0x1000 )
	ROM_CONTINUE(             0x1c000, 0x1000 )				/* bitplane 3 for bank 7,8 */
	ROM_CONTINUE(             0x1e000, 0x1000 )

	ROM_REGION( 0x18000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "darw_10.rom",  0x00000, 0x8000, 0x487a014c )	/* sprites */
	ROM_LOAD( "darw_11.rom",  0x08000, 0x8000, 0x548ce2d1 )
	ROM_LOAD( "darw_12.rom",  0x10000, 0x8000, 0xfaba5fef )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "df.12",   0x0000, 0x0100, 0x89b952ef ) /* red and green component */
	ROM_LOAD( "df.13",   0x0100, 0x0100, 0xd595e91d ) /* blue component */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64K for sound CPU */
	ROM_LOAD( "darw_08.rom",  0x8000, 0x8000, 0x6b580d58 )
ROM_END



GAME( 1986, brkthru,  0,       brkthru, brkthru, 0, ROT0,   "Data East USA", "Break Thru (US)" )
GAME( 1986, brkthruj, brkthru, brkthru, brkthru, 0, ROT0,   "Data East Corporation", "Kyohkoh-Toppa (Japan)" )
GAME( 1986, darwin,   0,       darwin,  darwin,  0, ROT270, "Data East Corporation", "Darwin 4078 (Japan)" )
