#include "../machine/lsasquad.cpp"
#include "../vidhrdw/lsasquad.cpp"

/***************************************************************************

Land Sea Air Squad / Storming Party  (c) 1986 Taito

driver by Nicola Salmoria

TODO:
- Unknown writes to YM2203 output ports (filters?)
- Wrong sprite/tilemap priority. Sprites can appear above and below the middle
  layer, it's not clear how this is selected since there are no free attribute
  bits.
  The priority seems to involve split transparency on the tilemap and also
  priority on sprites (so that people pass below doors but airplanes above).
  It is confirmed that priority is controlled by PROM a64-06.9 (grounding A9
  makes sprites disappear).
- Scrollram not entirely understood - it's most likely wrong, but more than
  enough to run this particular game.
- The video driver is pretty slow and could be optimized using temporary bitmaps
  (or tilemaps), however I haven't done that because the video circuitry is not
  entirely understood and if other games are found running on this hardware, they
  might not like the optimizations.

***************************************************************************/

#include "driver.h"

/* in vidhrdw/lsasquad.c */
extern unsigned char *lsasquad_scrollram,*lsasquad_videoram,*lsasquad_spriteram;
extern size_t lsasquad_spriteram_size;
void lsasquad_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void lsasquad_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/* in machine/lsasquad.c */
extern int lsasquad_invertcoin;
WRITE_HANDLER( lsasquad_sh_nmi_disable_w );
WRITE_HANDLER( lsasquad_sh_nmi_enable_w );
WRITE_HANDLER( lsasquad_sound_command_w );
READ_HANDLER( lsasquad_sh_sound_command_r );
WRITE_HANDLER( lsasquad_sh_result_w );
READ_HANDLER( lsasquad_sound_result_r );
READ_HANDLER( lsasquad_sound_status_r );

READ_HANDLER( lsasquad_68705_portA_r );
WRITE_HANDLER( lsasquad_68705_portA_w );
WRITE_HANDLER( lsasquad_68705_ddrA_w );
READ_HANDLER( lsasquad_68705_portB_r );
WRITE_HANDLER( lsasquad_68705_portB_w );
WRITE_HANDLER( lsasquad_68705_ddrB_w );
WRITE_HANDLER( lsasquad_mcu_w );
READ_HANDLER( lsasquad_mcu_r );
READ_HANDLER( lsasquad_mcu_status_r );




WRITE_HANDLER( lsasquad_bankswitch_w )
{
	unsigned char *ROM = memory_region(REGION_CPU1);

	/* bits 0-2 select ROM bank */
	cpu_setbank(1,&ROM[0x10000 + 0x2000 * (data & 7)]);

	/* bit 3 is zeroed on startup, maybe reset sound CPU */

	/* bit 4 flips screen */
	flip_screen_w(offset,data & 0x10);

	/* other bits unknown */
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x9fff, MRA_BANK1 },
	{ 0xa000, 0xe5ff, MRA_RAM },
	{ 0xe800, 0xe800, input_port_0_r },	/* DSWA */
	{ 0xe801, 0xe801, input_port_1_r },	/* DSWB */
	{ 0xe802, 0xe802, input_port_2_r },	/* DSWC */
	{ 0xe803, 0xe803, lsasquad_mcu_status_r },	/* COIN + 68705 status */
	{ 0xe804, 0xe804, input_port_4_r },	/* IN0 */
	{ 0xe805, 0xe805, input_port_5_r },	/* IN1 */
	{ 0xe806, 0xe806, input_port_6_r },	/* START */
	{ 0xe807, 0xe807, input_port_7_r },	/* SERVICE/TILT */
	{ 0xec00, 0xec00, lsasquad_sound_result_r },
	{ 0xec01, 0xec01, lsasquad_sound_status_r },
	{ 0xee00, 0xee00, lsasquad_mcu_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x9fff, MWA_ROM },
	{ 0xa000, 0xbfff, MWA_RAM },	/* SRAM */
	{ 0xc000, 0xdfff, MWA_RAM, &lsasquad_videoram },	/* SCREEN RAM */
	{ 0xe000, 0xe3ff, MWA_RAM, &lsasquad_scrollram },	/* SCROLL RAM */
	{ 0xe400, 0xe5ff, MWA_RAM, &lsasquad_spriteram, &lsasquad_spriteram_size },	/* OBJECT RAM */
	{ 0xea00, 0xea00, lsasquad_bankswitch_w },
	{ 0xec00, 0xec00, lsasquad_sound_command_w },
	{ 0xee00, 0xee00, lsasquad_mcu_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xa000, 0xa000, YM2203_status_port_0_r },
	{ 0xa001, 0xa001, YM2203_read_port_0_r },
	{ 0xd000, 0xd000, lsasquad_sh_sound_command_r },
	{ 0xd800, 0xd800, lsasquad_sound_status_r },
	{ 0xe000, 0xefff, MRA_ROM },	/* space for diagnostic ROM? */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xa000, 0xa000, YM2203_control_port_0_w },
	{ 0xa001, 0xa001, YM2203_write_port_0_w },
	{ 0xc000, 0xc000, YM2203_control_port_0_w },	/* actually AY8910 */
	{ 0xc001, 0xc001, YM2203_write_port_0_w },		/* actually AY8910 */
	{ 0xd000, 0xd000, lsasquad_sh_result_w },
	{ 0xd400, 0xd400, lsasquad_sh_nmi_disable_w },
	{ 0xd800, 0xd800, lsasquad_sh_nmi_enable_w },
	{ 0xe000, 0xefff, MWA_ROM },	/* space for diagnostic ROM? */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress m68705_readmem[] =
{
	{ 0x0000, 0x0000, lsasquad_68705_portA_r },
	{ 0x0001, 0x0001, lsasquad_68705_portB_r },
	{ 0x0002, 0x0002, lsasquad_mcu_status_r },
	{ 0x0010, 0x007f, MRA_RAM },
	{ 0x0080, 0x07ff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress m68705_writemem[] =
{
	{ 0x0000, 0x0000, lsasquad_68705_portA_w },
	{ 0x0001, 0x0001, lsasquad_68705_portB_w },
	{ 0x0004, 0x0004, lsasquad_68705_ddrA_w },
	{ 0x0005, 0x0005, lsasquad_68705_ddrB_w },
	{ 0x0010, 0x007f, MWA_RAM },
	{ 0x0080, 0x07ff, MWA_ROM },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( lsasquad )
	PORT_START	/* DSWA */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START	/* DSWB */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "50000 100000" )
	PORT_DIPSETTING(    0x0c, "80000 150000" )
	PORT_DIPSETTING(    0x04, "100000 200000" )
	PORT_DIPSETTING(    0x00, "150000 300000" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x80, "Japanese" )

	PORT_START	/* DSWC */
	PORT_DIPNAME( 0x01, 0x01, "Freeze" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x04, 0x04, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
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
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* 68705 ready to receive cmd */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_SPECIAL )	/* 0 = 68705 has sent result */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,
	RGN_FRAC(1,2),
	4,
	{ RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+4, 0, 4 },
	{ 3, 2, 1, 0, 8+3, 8+2, 8+1, 8+0 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8
};

static struct GfxLayout spritelayout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+4, 0, 4 },
	{ 3, 2, 1, 0, 8+3, 8+2, 8+1, 8+0,
			16*8+3, 16*8+2, 16*8+1, 16*8+0, 16*8+8+3, 16*8+8+2, 16*8+8+1, 16*8+8+0 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 },
	64*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,     0, 16 },
	{ REGION_GFX2, 0, &spritelayout, 256, 16 },
	{ -1 }	/* end of array */
};



static void irqhandler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static WRITE_HANDLER( unk )
{
}

/* actually there is one AY8910 and one YM2203, but the sound core doesn't */
/* support that so we use 2 YM2203 */
static struct YM2203interface ym2203_interface =
{
	2,			/* 2 chips */
	3000000,	/* 3 MHz???? */
	{ YM2203_VOL(100,20), YM2203_VOL(0,20) },
	{ 0 },
	{ 0 },
	{ unk },
	{ unk },
	{ irqhandler }
};



static struct MachineDriver machine_driver_lsasquad =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,	/* 6 MHz? */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,	/* 4 MHz? */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* NMIs are triggered by the main CPU */
								/* IRQs are triggered by the YM2203 */
		},
		{
			CPU_M68705,
			4000000/2,	/* ? */
			m68705_readmem,m68705_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	500,	/* 500 CPU slices per frame - an high value to ensure proper */
			/* synchronization of the CPUs */
			/* main<->sound synchronization depends on this */
	0,		/* init_machine() */

	/* video hardware */
	32*8, 32*8,	{ 0, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	512, 512,
	lsasquad_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	0,
	0,
	lsasquad_vh_screenrefresh,

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

ROM_START( lsasquad )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "a64-21.4",     0x00000, 0x8000, 0x5ff6b017 )
    /* ROMs banked at 8000-9fff */
	ROM_LOAD( "a64-20.3",     0x10000, 0x8000, 0x7f8b4979 )
	ROM_LOAD( "a64-19.2",     0x18000, 0x8000, 0xba31d34a )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "a64-04.44",    0x0000, 0x8000, 0xc238406a )

	ROM_REGION( 0x0800, REGION_CPU3 )	/* 2k for the microcontroller */
	ROM_LOAD( "a64-05.35",    0x0000, 0x0800, 0x572677b9 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a64-10.27",    0x00000, 0x8000, 0xbb4f1b37 )
	ROM_LOAD( "a64-22.28",    0x08000, 0x8000, 0x58e03b89 )
	ROM_LOAD( "a64-11.40",    0x10000, 0x8000, 0xa3bbc0b3 )
	ROM_LOAD( "a64-23.41",    0x18000, 0x8000, 0x377a538b )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a64-14.2",     0x00000, 0x8000, 0xa72e2041 )
	ROM_LOAD( "a64-16.3",     0x08000, 0x8000, 0x05206333 )
	ROM_LOAD( "a64-15.25",    0x10000, 0x8000, 0x01ed5851 )
	ROM_LOAD( "a64-17.26",    0x18000, 0x8000, 0x6eaf3735 )

	ROM_REGION( 0x1000, REGION_PROMS )
	ROM_LOAD( "a64-07.22",    0x0000, 0x0400, 0x82802bbb )	/* red   (bottom half unused) */
	ROM_LOAD( "a64-08.23",    0x0400, 0x0400, 0xaa9e1dbd )	/* green (bottom half unused) */
	ROM_LOAD( "a64-09.24",    0x0800, 0x0400, 0xdca86295 )	/* blue  (bottom half unused) */
	ROM_LOAD( "a64-06.9",     0x0c00, 0x0400, 0x7ced30ba )	/* priority */
ROM_END

ROM_START( storming )
	ROM_REGION( 0x20000, REGION_CPU1 )
	ROM_LOAD( "stpartyj.001", 0x00000, 0x8000, 0x07e6bc61 )
    /* ROMs banked at 8000-9fff */
	ROM_LOAD( "stpartyj.002", 0x10000, 0x8000, 0x1c7fe5d5 )
	ROM_LOAD( "stpartyj.003", 0x18000, 0x8000, 0x159f23a6 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "a64-04.44",    0x0000, 0x8000, 0xc238406a )

	ROM_REGION( 0x0800, REGION_CPU3 )	/* 2k for the microcontroller */
	ROM_LOAD( "a64-05.35",    0x0000, 0x0800, 0x572677b9 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a64-10.27",    0x00000, 0x8000, 0xbb4f1b37 )
	ROM_LOAD( "stpartyj.009", 0x08000, 0x8000, 0x8ee2443b )
	ROM_LOAD( "a64-11.40",    0x10000, 0x8000, 0xa3bbc0b3 )
	ROM_LOAD( "stpartyj.011", 0x18000, 0x8000, 0xf342d42f )

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a64-14.2",     0x00000, 0x8000, 0xa72e2041 )
	ROM_LOAD( "a64-16.3",     0x08000, 0x8000, 0x05206333 )
	ROM_LOAD( "a64-15.25",    0x10000, 0x8000, 0x01ed5851 )
	ROM_LOAD( "a64-17.26",    0x18000, 0x8000, 0x6eaf3735 )

	ROM_REGION( 0x1000, REGION_PROMS )
	ROM_LOAD( "a64-07.22",    0x0000, 0x0400, 0x82802bbb )	/* red   (bottom half unused) */
	ROM_LOAD( "a64-08.23",    0x0400, 0x0400, 0xaa9e1dbd )	/* green (bottom half unused) */
	ROM_LOAD( "a64-09.24",    0x0800, 0x0400, 0xdca86295 )	/* blue  (bottom half unused) */
	ROM_LOAD( "a64-06.9",     0x0c00, 0x0400, 0x7ced30ba )	/* priority */
ROM_END


/* coin inputs are inverted in storming */
static void init_lsasquad(void) { lsasquad_invertcoin = 0x00; }
static void init_storming(void) { lsasquad_invertcoin = 0x0c; }


GAME( 1986, lsasquad, 0,        lsasquad, lsasquad, lsasquad, ROT270, "Taito", "Land Sea Air Squad / Riku Kai Kuu Saizensen" )
GAME( 1986, storming, lsasquad, lsasquad, lsasquad, storming, ROT270, "Taito", "Storming Party / Riku Kai Kuu Saizensen" )
