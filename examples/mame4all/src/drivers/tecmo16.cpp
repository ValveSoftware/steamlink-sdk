#include "../vidhrdw/tecmo16.cpp"

/******************************************************************************

  Ganbare Ginkun  (Japan)  (c)1995 TECMO
  Final StarForce (US)     (c)1992 TECMO

  driver by Eisuke Watanabe (MHF03337@nifty.ne.jp)

  special thanks to Nekomata, NTD & code-name'Siberia'

TODO:
- wrong background in fstarfrc title

******************************************************************************/

#include "driver.h"
#include "cpu/m68000/m68000.h"
#include "cpu/z80/z80.h"
#include "vidhrdw/generic.h"


extern unsigned char *tecmo16_videoram;
extern unsigned char *tecmo16_colorram;
extern unsigned char *tecmo16_videoram2;
extern unsigned char *tecmo16_colorram2;
extern unsigned char *tecmo16_charram;

READ_HANDLER( tecmo16_videoram_r );
READ_HANDLER( tecmo16_colorram_r );
READ_HANDLER( tecmo16_videoram2_r );
READ_HANDLER( tecmo16_colorram2_r );
READ_HANDLER( tecmo16_charram_r );
READ_HANDLER( tecmo16_spriteram_r );
WRITE_HANDLER( tecmo16_videoram_w );
WRITE_HANDLER( tecmo16_colorram_w );
WRITE_HANDLER( tecmo16_videoram2_w );
WRITE_HANDLER( tecmo16_colorram2_w );
WRITE_HANDLER( tecmo16_charram_w );
WRITE_HANDLER( tecmo16_spriteram_w );

WRITE_HANDLER( tecmo16_scroll_x_w );
WRITE_HANDLER( tecmo16_scroll_y_w );
WRITE_HANDLER( tecmo16_scroll2_x_w );
WRITE_HANDLER( tecmo16_scroll2_y_w );
WRITE_HANDLER( tecmo16_scroll_char_x_w );
WRITE_HANDLER( tecmo16_scroll_char_y_w );

int fstarfrc_vh_start(void);
int ginkun_vh_start(void);
void tecmo16_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/******************************************************************************/

static WRITE_HANDLER( tecmo16_sound_command_w )
{
	soundlatch_w(0x00,data & 0xff);
	cpu_cause_interrupt(1,Z80_NMI_INT);
}

/******************************************************************************/

static struct MemoryReadAddress fstarfrc_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x100000, 0x103fff, MRA_BANK1 },	/* Main RAM */
	{ 0x110000, 0x110fff, tecmo16_charram_r },
	{ 0x120000, 0x1207ff, tecmo16_videoram_r },
	{ 0x120800, 0x120fff, tecmo16_colorram_r },
	{ 0x121000, 0x1217ff, tecmo16_videoram2_r },
	{ 0x121800, 0x121fff, tecmo16_colorram2_r },
	{ 0x122000, 0x127fff, MRA_BANK2 },	/* work area */
	{ 0x130000, 0x130fff, tecmo16_spriteram_r },
	{ 0x140000, 0x1407ff, paletteram_word_r },
	{ 0x150030, 0x150031, input_port_1_r },
	{ 0x150040, 0x150041, input_port_0_r },
	{ 0x150050, 0x150051, input_port_2_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress fstarfrc_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x100000, 0x103fff, MWA_BANK1 },	/* Main RAM */
	{ 0x110000, 0x110fff, tecmo16_charram_w, &tecmo16_charram },
	{ 0x120000, 0x1207ff, tecmo16_videoram_w, &tecmo16_videoram },
	{ 0x120800, 0x120fff, tecmo16_colorram_w, &tecmo16_colorram },
	{ 0x121000, 0x1217ff, tecmo16_videoram2_w, &tecmo16_videoram2 },
	{ 0x121800, 0x121fff, tecmo16_colorram2_w, &tecmo16_colorram2 },
	{ 0x122000, 0x127fff, MWA_BANK2 },	/* work area */
	{ 0x130000, 0x130fff, tecmo16_spriteram_w, &spriteram, &spriteram_size },
	{ 0x140000, 0x1407ff, paletteram_xxxxBBBBGGGGRRRR_word_w, &paletteram },
	{ 0x150010, 0x150011, tecmo16_sound_command_w },
	{ 0x150030, 0x150031, MWA_NOP },
	{ 0x160000, 0x160001, tecmo16_scroll_char_x_w },
	{ 0x16000c, 0x16000d, tecmo16_scroll_x_w },
	{ 0x160012, 0x160013, tecmo16_scroll_y_w },
	{ 0x160018, 0x160019, tecmo16_scroll2_x_w },
	{ 0x16001e, 0x16001f, tecmo16_scroll2_y_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress ginkun_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x100000, 0x103fff, MRA_BANK1 },	/* Main RAM */
	{ 0x110000, 0x110fff, tecmo16_charram_r },
	{ 0x120000, 0x120fff, tecmo16_videoram_r },
	{ 0x121000, 0x121fff, tecmo16_colorram_r },
	{ 0x122000, 0x122fff, tecmo16_videoram2_r },
	{ 0x123000, 0x123fff, tecmo16_colorram2_r },
	{ 0x130000, 0x130fff, tecmo16_spriteram_r },
	{ 0x140000, 0x1407ff, paletteram_word_r },
	{ 0x150030, 0x150031, input_port_1_r },
	{ 0x150040, 0x150041, input_port_0_r },
	{ 0x150050, 0x150051, input_port_2_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress ginkun_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x100000, 0x103fff, MWA_BANK1 },	/* Main RAM */
	{ 0x110000, 0x110fff, tecmo16_charram_w, &tecmo16_charram },
	{ 0x120000, 0x120fff, tecmo16_videoram_w, &tecmo16_videoram },
	{ 0x121000, 0x121fff, tecmo16_colorram_w, &tecmo16_colorram },
	{ 0x122000, 0x122fff, tecmo16_videoram2_w, &tecmo16_videoram2 },
	{ 0x123000, 0x123fff, tecmo16_colorram2_w, &tecmo16_colorram2 },
	{ 0x130000, 0x130fff, tecmo16_spriteram_w, &spriteram, &spriteram_size },
	{ 0x140000, 0x1407ff, paletteram_xxxxBBBBGGGGRRRR_word_w, &paletteram },
	{ 0x150010, 0x150011, tecmo16_sound_command_w },
	{ 0x160000, 0x160001, tecmo16_scroll_char_x_w },
	{ 0x160006, 0x160007, tecmo16_scroll_char_y_w },
	{ 0x16000c, 0x16000d, tecmo16_scroll_x_w },
	{ 0x160012, 0x160013, tecmo16_scroll_y_w },
	{ 0x160018, 0x160019, tecmo16_scroll2_x_w },
	{ 0x16001e, 0x16001f, tecmo16_scroll2_y_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0xefff, MRA_ROM },
	{ 0xf000, 0xfbff, MRA_RAM },	/* Sound RAM */
	{ 0xfc00, 0xfc00, OKIM6295_status_0_r },
	{ 0xfc05, 0xfc05, YM2151_status_port_0_r },
	{ 0xfc08, 0xfc08, soundlatch_r },
	{ 0xfc0c, 0xfc0c, MRA_NOP },
	{ 0xfffe, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0xefff, MWA_ROM },
	{ 0xf000, 0xfbff, MWA_RAM },	/* Sound RAM */
	{ 0xfc00, 0xfc00, OKIM6295_data_0_w },
	{ 0xfc04, 0xfc04, YM2151_register_port_0_w },
	{ 0xfc05, 0xfc05, YM2151_data_port_0_w },
	{ 0xfc0c, 0xfc0c, MWA_NOP },
	{ 0xfffe, 0xffff, MWA_RAM },
	{ -1 }	/* end of table */
};

/******************************************************************************/

INPUT_PORTS_START( fstarfrc )
	PORT_START			/* DIP SW 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START			/* DIP SW 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW,  IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW,  IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW,  IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW,  IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW,  IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW,  IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW,  IPT_START1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW,  IPT_START2 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW,  IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW,  IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW,  IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW,  IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW,  IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW,  IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_COIN2 )
INPUT_PORTS_END

INPUT_PORTS_START( ginkun )
	PORT_START			/* DIP SW 1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x10, 0x10, "Continue Plus 1up" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START			/* DIP SW 2 */
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
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )		/* Doesn't work? */
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW,  IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW,  IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW,  IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW,  IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW,  IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW,  IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW,  IPT_START1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW,  IPT_START2 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW,  IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW,  IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW,  IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW,  IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW,  IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW,  IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x8000, IP_ACTIVE_HIGH, IPT_COIN2 )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	4096,	/* 4096 characters */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },	/* the bitplanes are packed in one nibble */
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8	/* every char takes 32 consecutive bytes */
};

static struct GfxLayout tilelayout =
{
	16,16,	/* 16*16 tiles */
	8192,	/* 8192 tiles */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },	/* the bitplanes are packed in one nibble */
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4,
			32*8+0*4, 32*8+1*4, 32*8+2*4, 32*8+3*4, 32*8+4*4, 32*8+5*4, 32*8+6*4, 32*8+7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			16*32, 17*32, 18*32, 19*32, 20*32, 21*32, 22*32, 23*32 },
	128*8	/* every sprite takes 128 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	8,8,	/* 8*8 sprites */
	32768,	/* 32768 sprites */
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 },	/* the bitplanes are packed in one nibble */
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8	/* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   1*16*16, 16   },
	{ REGION_GFX2, 0, &tilelayout,   2*16*16, 16*2 },
	{ REGION_GFX3, 0, &spritelayout, 0*16*16, 16   },
	{ -1 } /* end of array */
};

/******************************************************************************/

static void irqhandler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2151interface ym2151_interface =
{
	1,
	8000000/2,	/* 4MHz */
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) },
	{ irqhandler }
};

static struct OKIM6295interface okim6295_interface =
{
	1,			/* 1 chip */
	{ 7575 },		/* 7575Hz playback */
	{ REGION_SOUND1 },
	{ 40 }
};

/******************************************************************************/

static struct MachineDriver machine_driver_fstarfrc =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			24000000/2,			/* 12MHz */
			fstarfrc_readmem,fstarfrc_writemem,0,0,
			m68_level5_irq,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			8000000/2,			/* 4MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2151 */
								/* NMIs are triggered by the main CPU */
		}
	},

	60, DEFAULT_60HZ_VBLANK_DURATION,
	10,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	fstarfrc_vh_start,
	0,
	tecmo16_vh_screenrefresh,

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

static struct MachineDriver machine_driver_ginkun =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			24000000/2,			/* 12MHz */
			ginkun_readmem,ginkun_writemem,0,0,
			m68_level5_irq,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			8000000/2,			/* 4MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM2151 */
								/* NMIs are triggered by the main CPU */
		}
	},

	60, DEFAULT_60HZ_VBLANK_DURATION,
	10,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	ginkun_vh_start,
	0,
	tecmo16_vh_screenrefresh,

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

ROM_START( fstarfrc )
	ROM_REGION( 0x80000, REGION_CPU1 )
	ROM_LOAD_EVEN( "fstarf01.rom",      0x00000, 0x40000, 0x94c71de6 )
	ROM_LOAD_ODD ( "fstarf02.rom",      0x00000, 0x40000, 0xb1a07761 )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "fstarf07.rom",           0x00000, 0x10000, 0xe0ad5de1 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fstarf03.rom",           0x00000, 0x20000, 0x54375335 )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "fstarf05.rom",  0x00000, 0x80000, 0x77a281e7 )
	ROM_LOAD_GFX_ODD ( "fstarf04.rom",  0x00000, 0x80000, 0x398a920d )

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "fstarf09.rom",  0x00000, 0x80000, 0xd51341d2 )
	ROM_LOAD_GFX_ODD ( "fstarf06.rom",  0x00000, 0x80000, 0x07e40e87 )

	ROM_REGION( 0x20000, REGION_SOUND1 )
	ROM_LOAD( "fstarf08.rom",           0x00000, 0x20000, 0xf0ad5693 )
ROM_END

ROM_START( ginkun )
	ROM_REGION( 0x80000, REGION_CPU1 )
	ROM_LOAD_EVEN( "ginkun01.i01",      0x00000, 0x40000, 0x98946fd5 )
	ROM_LOAD_ODD ( "ginkun02.i02",      0x00000, 0x40000, 0xe98757f6 )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "ginkun07.i17",           0x00000, 0x10000, 0x8836b1aa )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ginkun03.i03",           0x00000, 0x20000, 0x4456e0df )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "ginkun05.i09",  0x00000, 0x80000, 0x1263bd42 )
	ROM_LOAD_GFX_ODD ( "ginkun04.i05",  0x00000, 0x80000, 0x9e4cf611 )

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "ginkun09.i22",  0x00000, 0x80000, 0x233384b9 )
	ROM_LOAD_GFX_ODD ( "ginkun06.i16",  0x00000, 0x80000, 0xf8589184 )

	ROM_REGION( 0x20000, REGION_SOUND1 )
	ROM_LOAD( "ginkun08.i18",           0x00000, 0x20000, 0x8b7583c7 )
ROM_END

/******************************************************************************/

GAME( 1992, fstarfrc, 0, fstarfrc, fstarfrc, 0, ROT90, "Tecmo", "Final Star Force (US)" )
GAME( 1995, ginkun,   0, ginkun,   ginkun,   0, ROT0,  "Tecmo", "Ganbare Ginkun" )
