#include "../vidhrdw/labyrunr.cpp"

/***************************************************************************

Labyrinth Runner (GX771) (c) 1987 Konami

similar to Fast Lane

Driver by Nicola Salmoria

***************************************************************************/

#include "driver.h"
#include "cpu/m6809/m6809.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"



/* from vidhrdw/labyrunr.c */
extern unsigned char *labyrunr_videoram1,*labyrunr_videoram2;
void labyrunr_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( labyrunr_vram1_w );
WRITE_HANDLER( labyrunr_vram2_w );
int labyrunr_vh_start(void);
void labyrunr_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

static int labyrunr_interrupt(void)
{
	if (cpu_getiloops() == 0)
	{
		if (K007121_ctrlram[0][0x07] & 0x02) return HD6309_INT_IRQ;
	}
	else if (cpu_getiloops() % 2)
	{
		if (K007121_ctrlram[0][0x07] & 0x01) return nmi_interrupt();
	}
	return ignore_interrupt();
}

static WRITE_HANDLER( labyrunr_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);

if (data & 0xe0) usrintf_showmessage("bankswitch %02x",data);

	/* bits 0-2 = bank number */
	bankaddress = 0x10000 + (data & 0x07) * 0x4000;
	cpu_setbank(1,&RAM[bankaddress]);

	/* bits 3 and 4 are coin counters */
	coin_counter_w(0,data & 0x08);
	coin_counter_w(1,data & 0x10);
}

static struct MemoryReadAddress labyrunr_readmem[] =
{
	{ 0x0020, 0x005f, MRA_RAM },	/* scroll registers */
	{ 0x0801, 0x0801, YM2203_status_port_0_r },
	{ 0x0800, 0x0800, YM2203_read_port_0_r },
	{ 0x0901, 0x0901, YM2203_status_port_1_r },
	{ 0x0900, 0x0900, YM2203_read_port_1_r },
	{ 0x0a00, 0x0a00, input_port_5_r },
	{ 0x0a01, 0x0a01, input_port_4_r },
	{ 0x0b00, 0x0b00, input_port_3_r },
	{ 0x0d00, 0x0d1f, K051733_r },			/* 051733 (protection) */
	{ 0x1000, 0x10ff, paletteram_r },
	{ 0x1800, 0x1fff, MRA_RAM },
	{ 0x2000, 0x3fff, MRA_RAM },
	{ 0x4000, 0x7fff, MRA_BANK1 },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress labyrunr_writemem[] =
{
	{ 0x0000, 0x0007, K007121_ctrl_0_w },
	{ 0x0020, 0x005f, MWA_RAM },	/* scroll registers */
	{ 0x0801, 0x0801, YM2203_control_port_0_w },
	{ 0x0800, 0x0800, YM2203_write_port_0_w },
	{ 0x0901, 0x0901, YM2203_control_port_1_w },
	{ 0x0900, 0x0900, YM2203_write_port_1_w },
	{ 0x0c00, 0x0c00, labyrunr_bankswitch_w },
	{ 0x0d00, 0x0d1f, K051733_w },				/* 051733 (protection) */
	{ 0x0e00, 0x0e00, watchdog_reset_w },
	{ 0x1000, 0x10ff, paletteram_xBBBBBGGGGGRRRRR_swap_w, &paletteram },
	{ 0x1800, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2fff, MWA_RAM, &spriteram },	/* Sprite RAM */
	{ 0x3000, 0x37ff, labyrunr_vram1_w, &labyrunr_videoram1 },
	{ 0x3800, 0x3fff, labyrunr_vram2_w, &labyrunr_videoram2 },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};


/***************************************************************************

	Input Ports

***************************************************************************/

INPUT_PORTS_START( labyrunr )
	PORT_START	/* DSW #1 */
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
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )
//	PORT_DIPSETTING(    0x00, "Invalid" )

	PORT_START	/* DSW #2 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ) )
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

	PORT_START	/* DSW #3 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Upright Controls" )
	PORT_DIPSETTING(    0x02, "Single" )
	PORT_DIPSETTING(    0x00, "Dual" )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, "Continues" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* COINSW */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* PLAYER 1 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* PLAYER 2 INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )
INPUT_PORTS_END



static struct GfxLayout gfxlayout =
{
	8,8,
	0x40000/32,
	4,
	{ 0, 1, 2, 3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &gfxlayout, 0, 8*16 },
	{ -1 } /* end of array */
};

/***************************************************************************

	Machine Driver

***************************************************************************/

static struct YM2203interface ym2203_interface =
{
	2,			/* 2 chips */
	3000000,	/* 24MHz/8? */
	{ YM2203_VOL(80,40), YM2203_VOL(80,40) },
	{ input_port_0_r },
	{ input_port_1_r, input_port_2_r },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_labyrunr =
{
	/* basic machine hardware */
	{
		{
			CPU_HD6309,
			3000000,		/* 24MHz/8? */
			labyrunr_readmem,labyrunr_writemem,0,0,
			labyrunr_interrupt,8	/* 1 IRQ + 4 NMI (generated by 007121) */
        }
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	37*8, 32*8, { 0*8, 35*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	128, 2*8*16*16,
	labyrunr_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	labyrunr_vh_start,
	0,
	labyrunr_vh_screenrefresh,

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

  Game ROMs

***************************************************************************/

ROM_START( labyrunr )
	ROM_REGION( 0x28000, REGION_CPU1 ) /* code + banked roms */
	ROM_LOAD( "771j04.10f", 0x10000, 0x08000, 0x354a41d0 )
	ROM_CONTINUE(           0x08000, 0x08000 )
	ROM_LOAD( "771j03.08f", 0x18000, 0x10000, 0x12b49044 )

	ROM_REGION( 0x40000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "771d01.14a",	0x00000, 0x40000, 0x15c8f5f9 )	/* tiles + sprites */

	ROM_REGION( 0x0100, REGION_PROMS )
	ROM_LOAD( "771d02.08d", 0x0000, 0x0100, 0x3d34bb5a )	/* sprite lookup table */
															/* there is no char lookup table */
ROM_END



GAMEX( 1987, labyrunr, 0, labyrunr, labyrunr, 0, ROT90, "Konami", "Labyrinth Runner (Japan)", GAME_NOT_WORKING )
