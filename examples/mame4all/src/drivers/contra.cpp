#include "../vidhrdw/contra.cpp"

/***************************************************************************

Contra/Gryzor (c) 1987 Konami

Notes:
    Press 1P and 2P together to advance through tests.

Credits:
    Carlos A. Lozano: CPU emulation
    Phil Stroffolino: video driver
    Jose Tejada Gomez (of Grytra fame) for precious information on sprites
    Eric Hustvedt: palette optimizations and cocktail support

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"

extern unsigned char *contra_fg_vram,*contra_fg_cram;
extern unsigned char *contra_bg_vram,*contra_bg_cram;
extern unsigned char *contra_text_vram,*contra_text_cram;

void contra_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

WRITE_HANDLER( contra_fg_vram_w );
WRITE_HANDLER( contra_fg_cram_w );
WRITE_HANDLER( contra_bg_vram_w );
WRITE_HANDLER( contra_bg_cram_w );
WRITE_HANDLER( contra_text_vram_w );
WRITE_HANDLER( contra_text_cram_w );

WRITE_HANDLER( contra_K007121_ctrl_0_w );
WRITE_HANDLER( contra_K007121_ctrl_1_w );
void contra_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
int contra_vh_start(void);
void contra_vh_stop(void);


WRITE_HANDLER( contra_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);


	bankaddress = 0x10000 + (data & 0x0f) * 0x2000;
	if (bankaddress < 0x28000)	/* for safety */
		cpu_setbank(1,&RAM[bankaddress]);
}

WRITE_HANDLER( contra_sh_irqtrigger_w )
{
	cpu_cause_interrupt(1,M6809_INT_IRQ);
}

WRITE_HANDLER( contra_coin_counter_w )
{
	if (data & 0x01) coin_counter_w(0,data & 0x01);
	if (data & 0x02) coin_counter_w(1,(data & 0x02) >> 1);
}

static WRITE_HANDLER( cpu_sound_command_w )
{
	soundlatch_w(offset,data);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0010, 0x0010, input_port_0_r },		/* IN0 */
	{ 0x0011, 0x0011, input_port_1_r },		/* IN1 */
	{ 0x0012, 0x0012, input_port_2_r },		/* IN2 */

	{ 0x0014, 0x0014, input_port_3_r },		/* DIPSW1 */
	{ 0x0015, 0x0015, input_port_4_r },		/* DIPSW2 */
	{ 0x0016, 0x0016, input_port_5_r },		/* DIPSW3 */

	{ 0x0c00, 0x0cff, MRA_RAM },
	{ 0x1000, 0x5fff, MRA_RAM },
	{ 0x6000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x0007, contra_K007121_ctrl_0_w },
	{ 0x0018, 0x0018, contra_coin_counter_w },
	{ 0x001a, 0x001a, contra_sh_irqtrigger_w },
	{ 0x001c, 0x001c, cpu_sound_command_w },
	{ 0x001e, 0x001e, MWA_NOP },	/* ? */
	{ 0x0060, 0x0067, contra_K007121_ctrl_1_w },
	{ 0x0c00, 0x0cff, paletteram_xBBBBBGGGGGRRRRR_w, &paletteram },
	{ 0x1000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x23ff, contra_fg_cram_w, &contra_fg_cram },
	{ 0x2400, 0x27ff, contra_fg_vram_w, &contra_fg_vram },
	{ 0x2800, 0x2bff, contra_text_cram_w, &contra_text_cram },
	{ 0x2c00, 0x2fff, contra_text_vram_w, &contra_text_vram },
	{ 0x3000, 0x37ff, MWA_RAM, &spriteram },/* 2nd bank is at 0x5000 */
	{ 0x3800, 0x3fff, MWA_RAM }, // second sprite buffer
	{ 0x4000, 0x43ff, contra_bg_cram_w, &contra_bg_cram },
	{ 0x4400, 0x47ff, contra_bg_vram_w, &contra_bg_vram },
	{ 0x4800, 0x5fff, MWA_RAM },
	{ 0x6000, 0x6fff, MWA_ROM },
 	{ 0x7000, 0x7000, contra_bankswitch_w },
	{ 0x7001, 0xffff, MWA_ROM },
	{ -1 }
};

static struct MemoryReadAddress readmem_sound[] =
{
	{ 0x0000, 0x0000, soundlatch_r },
	{ 0x2001, 0x2001, YM2151_status_port_0_r },
	{ 0x6000, 0x67ff, MRA_RAM },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }
};

static struct MemoryWriteAddress writemem_sound[] =
{
	{ 0x2000, 0x2000, YM2151_register_port_0_w },
	{ 0x2001, 0x2001, YM2151_data_port_0_w },
	{ 0x4000, 0x4000, MWA_NOP }, /* read triggers irq reset and latch read (in the hardware only). */
	{ 0x6000, 0x67ff, MWA_RAM },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }
};



INPUT_PORTS_START( contra )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
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
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )//marvins.c
	/* 0x00 is invalid */

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "30000 70000" )
	PORT_DIPSETTING(    0x10, "40000 80000" )
	PORT_DIPSETTING(    0x08, "40000" )
	PORT_DIPSETTING(    0x00, "50000" )
	PORT_DIPNAME( 0x60, 0x60, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW2 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Upright Controls" )
	PORT_DIPSETTING(    0x02, "Single" )
	PORT_DIPSETTING(    0x00, "Dual" )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, "Sound" )
	PORT_DIPSETTING(    0x00, "Mono" )
	PORT_DIPSETTING(    0x08, "Stereo" )
INPUT_PORTS_END



static struct GfxLayout gfx_layout =
{
	8,8,
	0x4000,
	4,
	{ 0, 1, 2, 3 },
	{ 0, 4, 8, 12, 16, 20, 24, 28 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &gfx_layout,       0, 8*16 },
	{ REGION_GFX2, 0, &gfx_layout, 8*16*16, 8*16 },
	{ -1 }
};



static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3582071,	/* seems to be the standard */
	{ YM3012_VOL(60,MIXER_PAN_RIGHT,60,MIXER_PAN_LEFT) },
	{ 0 }
};



static struct MachineDriver machine_driver_contra =
{
	{
		{
 			CPU_M6809,
			1500000,
			readmem,writemem,0,0,
			interrupt,1
		},
		{
 			CPU_M6809 | CPU_AUDIO_CPU,
			2000000,
			readmem_sound,writemem_sound,0,0,
			ignore_interrupt,0	/* IRQs are caused by the main CPU */
		},
	},
	60,DEFAULT_REAL_60HZ_VBLANK_DURATION,
	10,	/* 10 CPU slices per frame - enough for the sound CPU to read all commands */
	0,

	/* video hardware */

	37*8, 32*8, { 0*8, 35*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	128, 2*8*16*16,
	contra_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	contra_vh_start,
	contra_vh_stop,
	contra_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		}
	}
};



#if 0
	/* bootleg versions use smaller gfx ROMs, but the data is the same */
	ROM_LOAD( "g-7.rom",      0x00000, 0x10000, 0x57f467d2 )
	ROM_LOAD( "g-10.rom",     0x10000, 0x10000, 0xe6db9685 )
	ROM_LOAD( "g-9.rom",      0x20000, 0x10000, 0x875c61de )
	ROM_LOAD( "g-8.rom",      0x30000, 0x10000, 0x642765d6 )
	ROM_LOAD( "g-15.rom",     0x40000, 0x10000, 0xdaa2324b )
	ROM_LOAD( "g-16.rom",     0x50000, 0x10000, 0xe27cc835 )
	ROM_LOAD( "g-17.rom",     0x60000, 0x10000, 0xce4330b9 )
	ROM_LOAD( "g-18.rom",     0x70000, 0x10000, 0x1571ce42 )
	ROM_LOAD( "g-4.rom",      0x80000, 0x10000, 0x2cc7e52c )
	ROM_LOAD( "g-5.rom",      0x90000, 0x10000, 0xe01a5b9c )
	ROM_LOAD( "g-6.rom",      0xa0000, 0x10000, 0xaeea6744 )
	ROM_LOAD( "g-14.rom",     0xb0000, 0x10000, 0xfca77c5a )
	ROM_LOAD( "g-11.rom",     0xc0000, 0x10000, 0xbd9ba92c )
	ROM_LOAD( "g-12.rom",     0xd0000, 0x10000, 0xd0be7ec2 )
	ROM_LOAD( "g-13.rom",     0xe0000, 0x10000, 0x2b513d12 )
#endif


ROM_START( contra )
	ROM_REGION( 0x28000, REGION_CPU1 )	/* 64k for code + 96k for banked ROMs */
	ROM_LOAD( "633e03.18a",   0x20000, 0x08000, 0x7fc0d8cf )
	ROM_CONTINUE(			  0x08000, 0x08000 )
	ROM_LOAD( "633e02.17a",   0x10000, 0x10000, 0xb2f7bd9a )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for SOUND code */
	ROM_LOAD( "633e01.12a",   0x08000, 0x08000, 0xd1549255 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "633e04.7d",    0x00000, 0x40000, 0x14ddc542 )
	ROM_LOAD_GFX_ODD ( "633e05.7f",    0x00000, 0x40000, 0x42185044 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "633e06.16d",   0x00000, 0x40000, 0x9cf6faae )
	ROM_LOAD_GFX_ODD ( "633e07.16f",   0x00000, 0x40000, 0xf2d06638 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "633e08.10g",   0x0000, 0x0100, 0x9f0949fa )	/* 007121 #0 sprite lookup table */
	ROM_LOAD( "633e09.12g",   0x0100, 0x0100, 0x14ca5e19 )	/* 007121 #0 char lookup table */
	ROM_LOAD( "633f10.18g",   0x0200, 0x0100, 0x2b244d84 )	/* 007121 #1 sprite lookup table */
	ROM_LOAD( "633f11.20g",   0x0300, 0x0100, 0x14ca5e19 )	/* 007121 #1 char lookup table */
ROM_END

ROM_START( contrab )
	ROM_REGION( 0x28000, REGION_CPU1 )	/* 64k for code + 96k for banked ROMs */
	ROM_LOAD( "contra.20",    0x20000, 0x08000, 0xd045e1da )
	ROM_CONTINUE(             0x08000, 0x08000 )
	ROM_LOAD( "633e02.17a",   0x10000, 0x10000, 0xb2f7bd9a )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for SOUND code */
	ROM_LOAD( "633e01.12a",   0x08000, 0x08000, 0xd1549255 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "633e04.7d",    0x00000, 0x40000, 0x14ddc542 )
	ROM_LOAD_GFX_ODD ( "633e05.7f",    0x00000, 0x40000, 0x42185044 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "633e06.16d",   0x00000, 0x40000, 0x9cf6faae )
	ROM_LOAD_GFX_ODD ( "633e07.16f",   0x00000, 0x40000, 0xf2d06638 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "633e08.10g",   0x0000, 0x0100, 0x9f0949fa )	/* 007121 #0 sprite lookup table */
	ROM_LOAD( "633e09.12g",   0x0100, 0x0100, 0x14ca5e19 )	/* 007121 #0 char lookup table */
	ROM_LOAD( "633f10.18g",   0x0200, 0x0100, 0x2b244d84 )	/* 007121 #1 sprite lookup table */
	ROM_LOAD( "633f11.20g",   0x0300, 0x0100, 0x14ca5e19 )	/* 007121 #1 char lookup table */
ROM_END

ROM_START( contraj )
	ROM_REGION( 0x28000, REGION_CPU1 )	/* 64k for code + 96k for banked ROMs */
	ROM_LOAD( "633n03.18a",   0x20000, 0x08000, 0xfedab568 )
	ROM_CONTINUE(             0x08000, 0x08000 )
	ROM_LOAD( "633k02.17a",   0x10000, 0x10000, 0x5d5f7438 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for SOUND code */
	ROM_LOAD( "633e01.12a",   0x08000, 0x08000, 0xd1549255 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "633e04.7d",    0x00000, 0x40000, 0x14ddc542 )
	ROM_LOAD_GFX_ODD ( "633e05.7f",    0x00000, 0x40000, 0x42185044 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "633e06.16d",   0x00000, 0x40000, 0x9cf6faae )
	ROM_LOAD_GFX_ODD ( "633e07.16f",   0x00000, 0x40000, 0xf2d06638 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "633e08.10g",   0x0000, 0x0100, 0x9f0949fa )	/* 007121 #0 sprite lookup table */
	ROM_LOAD( "633e09.12g",   0x0100, 0x0100, 0x14ca5e19 )	/* 007121 #0 char lookup table */
	ROM_LOAD( "633f10.18g",   0x0200, 0x0100, 0x2b244d84 )	/* 007121 #1 sprite lookup table */
	ROM_LOAD( "633f11.20g",   0x0300, 0x0100, 0x14ca5e19 )	/* 007121 #1 char lookup table */
ROM_END

ROM_START( contrajb )
	ROM_REGION( 0x28000, REGION_CPU1 )	/* 64k for code + 96k for banked ROMs */
	ROM_LOAD( "g-2.rom",      0x20000, 0x08000, 0xbdb9196d )
	ROM_CONTINUE(             0x08000, 0x08000 )
	ROM_LOAD( "633k02.17a",   0x10000, 0x10000, 0x5d5f7438 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for SOUND code */
	ROM_LOAD( "633e01.12a",   0x08000, 0x08000, 0xd1549255 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "633e04.7d",    0x00000, 0x40000, 0x14ddc542 )
	ROM_LOAD_GFX_ODD ( "633e05.7f",    0x00000, 0x40000, 0x42185044 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "633e06.16d",   0x00000, 0x40000, 0x9cf6faae )
	ROM_LOAD_GFX_ODD ( "633e07.16f",   0x00000, 0x40000, 0xf2d06638 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "633e08.10g",   0x0000, 0x0100, 0x9f0949fa )	/* 007121 #0 sprite lookup table */
	ROM_LOAD( "633e09.12g",   0x0100, 0x0100, 0x14ca5e19 )	/* 007121 #0 char lookup table */
	ROM_LOAD( "633f10.18g",   0x0200, 0x0100, 0x2b244d84 )	/* 007121 #1 sprite lookup table */
	ROM_LOAD( "633f11.20g",   0x0300, 0x0100, 0x14ca5e19 )	/* 007121 #1 char lookup table */
ROM_END

ROM_START( gryzor )
	ROM_REGION( 0x28000, REGION_CPU1 )	/* 64k for code + 96k for banked ROMs */
	ROM_LOAD( "g2",           0x20000, 0x08000, 0x92ca77bd )
	ROM_CONTINUE(             0x08000, 0x08000 )
	ROM_LOAD( "g3",           0x10000, 0x10000, 0xbbd9e95e )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for SOUND code */
	ROM_LOAD( "633e01.12a",   0x08000, 0x08000, 0xd1549255 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "633e04.7d",    0x00000, 0x40000, 0x14ddc542 )
	ROM_LOAD_GFX_ODD ( "633e05.7f",    0x00000, 0x40000, 0x42185044 )

	ROM_REGION( 0x80000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN( "633e06.16d",   0x00000, 0x40000, 0x9cf6faae )
	ROM_LOAD_GFX_ODD ( "633e07.16f",   0x00000, 0x40000, 0xf2d06638 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "633e08.10g",   0x0000, 0x0100, 0x9f0949fa )	/* 007121 #0 sprite lookup table */
	ROM_LOAD( "633e09.12g",   0x0100, 0x0100, 0x14ca5e19 )	/* 007121 #0 char lookup table */
	ROM_LOAD( "633f10.18g",   0x0200, 0x0100, 0x2b244d84 )	/* 007121 #1 sprite lookup table */
	ROM_LOAD( "633f11.20g",   0x0300, 0x0100, 0x14ca5e19 )	/* 007121 #1 char lookup table */
ROM_END



GAME( 1987, contra,   0,      contra, contra, 0, ROT90, "Konami", "Contra (US)" )
GAME( 1987, contrab,  contra, contra, contra, 0, ROT90, "bootleg", "Contra (US bootleg)" )
GAME( 1987, contraj,  contra, contra, contra, 0, ROT90, "Konami", "Contra (Japan)" )
GAME( 1987, contrajb, contra, contra, contra, 0, ROT90, "bootleg", "Contra (Japan bootleg)" )
GAME( 1987, gryzor,   contra, contra, contra, 0, ROT90, "Konami", "Gryzor" )
