#include "../vidhrdw/sidearms.cpp"

/***************************************************************************

  Sidearms
  ========

  Driver provided by Paul Leaman

TODO:
  There is an additional ROM which seems to contain code for a third Z80,
  however the board only has two. The ROM is related to the missing star
  background. At one point, the code jumps to A000, outside of the ROM
  address space.
  This ROM could be something entirely different from Z80 code. In another
  set, it consists of only the second half of the one we have here.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char *sidearms_bg_scrollx,*sidearms_bg_scrolly;
extern unsigned char *sidearms_bg2_scrollx,*sidearms_bg2_scrolly;

WRITE_HANDLER( sidearms_c804_w );
WRITE_HANDLER( sidearms_gfxctrl_w );
int  sidearms_vh_start(void);
void sidearms_vh_stop(void);
void sidearms_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void sidearms_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static WRITE_HANDLER( sidearms_bankswitch_w )
{
	int bankaddress;
	unsigned char *RAM = memory_region(REGION_CPU1);


	/* bits 0 and 1 select the ROM bank */
	bankaddress = 0x10000 + (data & 0x0f) * 0x4000;
	cpu_setbank(1,&RAM[bankaddress]);
}


/* Turtle Ship input ports are rotated 90 degrees */
static READ_HANDLER( turtship_ports_r )
{
	int i,res;


	res = 0;
	for (i = 0;i < 8;i++)
		res |= ((readinputport(i) >> offset) & 1) << i;

	return res;
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc800, 0xc800, input_port_0_r },
	{ 0xc801, 0xc801, input_port_1_r },
	{ 0xc802, 0xc802, input_port_2_r },
	{ 0xc803, 0xc803, input_port_3_r },
	{ 0xc804, 0xc804, input_port_4_r },
	{ 0xc805, 0xc805, input_port_5_r },
	{ 0xd000, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc3ff, paletteram_xxxxBBBBRRRRGGGG_split1_w, &paletteram },
	{ 0xc400, 0xc7ff, paletteram_xxxxBBBBRRRRGGGG_split2_w, &paletteram_2 },
	{ 0xc800, 0xc800, soundlatch_w },
	{ 0xc801, 0xc801, sidearms_bankswitch_w },
	{ 0xc802, 0xc802, watchdog_reset_w },
	{ 0xc804, 0xc804, sidearms_c804_w },
	{ 0xc805, 0xc805, MWA_RAM, &sidearms_bg2_scrollx },
	{ 0xc806, 0xc806, MWA_RAM, &sidearms_bg2_scrolly },
	{ 0xc808, 0xc809, MWA_RAM, &sidearms_bg_scrollx },
	{ 0xc80a, 0xc80b, MWA_RAM, &sidearms_bg_scrolly },
	{ 0xc80c, 0xc80c, sidearms_gfxctrl_w },	/* background and sprite enable */
	{ 0xd000, 0xd7ff, videoram_w, &videoram, &videoram_size },
	{ 0xd800, 0xdfff, colorram_w, &colorram },
	{ 0xe000, 0xefff, MWA_RAM },
	{ 0xf000, 0xffff, MWA_RAM, &spriteram, &spriteram_size },
	{ -1 }  /* end of table */
};

#ifdef THIRD_CPU
static WRITE_HANDLER( pop )
{
RAM[0xa000] = 0xc3;
RAM[0xa001] = 0x00;
RAM[0xa002] = 0xa0;
//      interrupt_enable_w(offset,data & 0x80);
}

static struct MemoryReadAddress readmem2[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xe3ff, MRA_RAM },
	{ 0xe400, 0xe7ff, MRA_RAM },
	{ 0xe800, 0xebff, MRA_RAM },
	{ 0xec00, 0xefff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem2[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xdfff, MWA_RAM },
	{ 0xe000, 0xe3ff, MWA_RAM },
	{ 0xe400, 0xe7ff, MWA_RAM },
	{ 0xe800, 0xebff, MWA_RAM },
	{ 0xec00, 0xefff, MWA_RAM },
	{ 0xf80e, 0xf80e, pop },        /* ROM bank selector? (to appear at 8000) */
	{ -1 }  /* end of table */
};
#endif


static struct MemoryReadAddress turtship_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xe7ff, MRA_RAM },
	{ 0xe800, 0xe807, turtship_ports_r },
	{ 0xf000, 0xffff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress turtship_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM },
	{ 0xd000, 0xdfff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe000, 0xe3ff, paletteram_xxxxBBBBRRRRGGGG_split1_w, &paletteram },
	{ 0xe400, 0xe7ff, paletteram_xxxxBBBBRRRRGGGG_split2_w, &paletteram_2 },
	{ 0xe800, 0xe800, soundlatch_w },
	{ 0xe801, 0xe801, sidearms_bankswitch_w },
	{ 0xe802, 0xe802, watchdog_reset_w },
	{ 0xe804, 0xe804, sidearms_c804_w },
	{ 0xe805, 0xe805, MWA_RAM, &sidearms_bg2_scrollx },
	{ 0xe806, 0xe806, MWA_RAM, &sidearms_bg2_scrolly },
	{ 0xe808, 0xe809, MWA_RAM, &sidearms_bg_scrollx },
	{ 0xe80a, 0xe80b, MWA_RAM, &sidearms_bg_scrolly },
	{ 0xe80a, 0xe80b, MWA_RAM, &sidearms_bg_scrolly },
	{ 0xe80c, 0xe80c, sidearms_gfxctrl_w },	/* background and sprite enable */
	{ 0xf000, 0xf7ff, videoram_w, &videoram, &videoram_size },
	{ 0xf800, 0xffff, colorram_w, &colorram },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xd000, 0xd000, soundlatch_r },
	{ 0xf000, 0xf000, YM2203_status_port_0_r },
	{ 0xf002, 0xf002, YM2203_status_port_1_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xf000, 0xf000, YM2203_control_port_0_w },
	{ 0xf001, 0xf001, YM2203_write_port_0_w },
	{ 0xf002, 0xf002, YM2203_control_port_1_w },
	{ 0xf003, 0xf003, YM2203_write_port_1_w },
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( sidearms )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x08, 0x08, "Freeze" )	/* I'm not sure it's really a dip switch */
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "0 (Easiest)" )
	PORT_DIPSETTING(    0x06, "1" )
	PORT_DIPSETTING(    0x05, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x03, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x01, "6" )
	PORT_DIPSETTING(    0x00, "7 (Hardest)" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "100000" )
	PORT_DIPSETTING(    0x20, "100000 100000" )
	PORT_DIPSETTING(    0x10, "150000 150000" )
	PORT_DIPSETTING(    0x00, "200000 200000" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START      /* DSW2 */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )     /* not sure, but likely */
INPUT_PORTS_END

INPUT_PORTS_START( turtship )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSW0 */
	PORT_BITX( 0x01, 0x01, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", 0, IP_JOY_NONE )
	PORT_DIPSETTING( 0x01, DEF_STR( Off ) )
	PORT_DIPSETTING( 0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x02, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Normal" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0xe0, 0xa0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0xe0, "1" )
	PORT_DIPSETTING(    0x60, "2" )
	PORT_DIPSETTING(    0xa0, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0xc0, "5" )
	PORT_DIPSETTING(    0x40, "6" )
	PORT_DIPSETTING(    0x80, "7" )
	PORT_DIPSETTING(    0x00, "8" )

	PORT_START      /* DSW1 */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x08, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "Every 150000" )
	PORT_DIPSETTING(    0x00, "Every 200000" )
	PORT_DIPSETTING(    0x0c, "150000 only" )
	PORT_DIPSETTING(    0x04, "200000 only" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	/* 0xc0 1 Coin/1 Credit */
INPUT_PORTS_END

INPUT_PORTS_START( dyger )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* seems to be 1-player only */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* seems to be 1-player only */

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x02, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0xe0, 0xa0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0xe0, "1" )
	PORT_DIPSETTING(    0x60, "2" )
	PORT_DIPSETTING(    0xa0, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0xc0, "5" )
	PORT_DIPSETTING(    0x40, "6" )
	PORT_DIPSETTING(    0x80, "7" )
	PORT_DIPSETTING(    0x00, "8" )

	PORT_START      /* DSW1 */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x04, "Every 150000" )
	PORT_DIPSETTING(    0x00, "Every 200000" )
	PORT_DIPSETTING(    0x0c, "150000 only" )
	PORT_DIPSETTING(    0x08, "200000 only" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	/* 0xc0 1 Coin/1 Credit */
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	1024,   /* 1024 characters */
	2,      /* 2 bits per pixel */
	{ 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8    /* every char takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	2048,   /* 2048 sprites */
	4,      /* 4 bits per pixel */
	{ 2048*64*8+4, 2048*64*8+0, 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			32*8+0, 32*8+1, 32*8+2, 32*8+3, 33*8+0, 33*8+1, 33*8+2, 33*8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8    /* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout tilelayout =
{
	32,32,  /* 32*32 tiles */
	512,    /* 512 tiles */
	4,      /* 4 bits per pixel */
	{ 512*256*8+4, 512*256*8+0, 4, 0 },
	{
		0,       1,       2,       3,       8+0,       8+1,       8+2,       8+3,
		32*16+0, 32*16+1, 32*16+2, 32*16+3, 32*16+8+0, 32*16+8+1, 32*16+8+2, 32*16+8+3,
		64*16+0, 64*16+1, 64*16+2, 64*16+3, 64*16+8+0, 64*16+8+1, 64*16+8+2, 64*16+8+3,
		96*16+0, 96*16+1, 96*16+2, 96*16+3, 96*16+8+0, 96*16+8+1, 96*16+8+2, 96*16+8+3,
	},
	{
		0*16,  1*16,  2*16,  3*16,  4*16,  5*16,  6*16,  7*16,
		8*16,  9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16,
		16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16,
		24*16, 25*16, 26*16, 27*16, 28*16, 29*16, 30*16, 31*16
	},
	256*8   /* every tile takes 256 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   768, 64 }, /* colors 768-1023 */
	{ REGION_GFX2, 0, &tilelayout,     0, 32 }, /* colors   0-511 */
	{ REGION_GFX3, 0, &spritelayout, 512, 16 }, /* colors 512-767 */
	{ -1 } /* end of array */
};



static struct GfxLayout turtship_tilelayout =
{
	32,32,  /* 32*32 tiles */
	768,    /* 768 tiles */
	4,      /* 4 bits per pixel */
	{ 768*256*8+4, 768*256*8+0, 4, 0 },
	{
		0,       1,       2,       3,       8+0,       8+1,       8+2,       8+3,
		32*16+0, 32*16+1, 32*16+2, 32*16+3, 32*16+8+0, 32*16+8+1, 32*16+8+2, 32*16+8+3,
		64*16+0, 64*16+1, 64*16+2, 64*16+3, 64*16+8+0, 64*16+8+1, 64*16+8+2, 64*16+8+3,
		96*16+0, 96*16+1, 96*16+2, 96*16+3, 96*16+8+0, 96*16+8+1, 96*16+8+2, 96*16+8+3,
	},
	{
		0*16,  1*16,  2*16,  3*16,  4*16,  5*16,  6*16,  7*16,
		8*16,  9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16,
		16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16,
		24*16, 25*16, 26*16, 27*16, 28*16, 29*16, 30*16, 31*16
	},
	256*8   /* every tile takes 256 consecutive bytes */
};

static struct GfxDecodeInfo turtship_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,          768, 64 },	/* colors 768-1023 */
	{ REGION_GFX2, 0, &turtship_tilelayout,   0, 32 },	/* colors   0-511 */
	{ REGION_GFX3, 0, &spritelayout,        512, 16 },	/* colors 512-767 */
	{ -1 } /* end of array */
};

/* handler called by the 2203 emulator when the internal timers cause an IRQ */
static void irqhandler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2203interface ym2203_interface =
{
	2,                      /* 2 chips */
	4000000,        /* 4 MHz ? (hand tuned) */
	{ YM2203_VOL(15,25), YM2203_VOL(15,25) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler }
};



static struct MachineDriver machine_driver_sidearms =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,        /* 4 Mhz (?) */
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,        /* 4 Mhz (?) */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0      /* IRQs are triggered by the YM2203 */
		},
#ifdef THIRD_CPU
		{
			CPU_Z80,
			4000000,        /* 4 Mhz (?) */
			readmem2,writemem2,0,0,
			nmi_interrupt,1
		}
#endif
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,  /* frames per second, vblank duration */
	1,      /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 8*8, (64-8)*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sidearms_vh_start,
	sidearms_vh_stop,
	sidearms_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};

static struct MachineDriver machine_driver_turtship =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,        /* 4 Mhz (?) */
			turtship_readmem,turtship_writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,        /* 4 Mhz (?) */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0      /* IRQs are triggered by the YM2203 */
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,  /* frames per second, vblank duration */
	1,      /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	64*8, 32*8, { 8*8, (64-8)*8-1, 2*8, 30*8-1 },
	turtship_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	sidearms_vh_start,
	sidearms_vh_stop,
	sidearms_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};


ROM_START( sidearms )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + banked ROMs images */
	ROM_LOAD( "sa03.bin",     0x00000, 0x08000, 0xe10fe6a0 )        /* CODE */
	ROM_LOAD( "a_14e.rom",    0x10000, 0x08000, 0x4925ed03 )        /* 0+1 */
	ROM_LOAD( "a_12e.rom",    0x18000, 0x08000, 0x81d0ece7 )        /* 2+3 */

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "a_04k.rom",    0x0000, 0x8000, 0x34efe2d2 )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* unknown, looks like Z80 code */
	ROM_LOAD( "b_11j.rom",    0x0000, 0x8000, 0x134dc35b )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a_10j.rom",    0x00000, 0x4000, 0x651fef75 ) /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b_13d.rom",    0x00000, 0x8000, 0x3c59afe1 ) /* tiles */
	ROM_LOAD( "b_13e.rom",    0x08000, 0x8000, 0x64bc3b77 )
	ROM_LOAD( "b_13f.rom",    0x10000, 0x8000, 0xe6bcea6f )
	ROM_LOAD( "b_13g.rom",    0x18000, 0x8000, 0xc71a3053 )
	ROM_LOAD( "b_14d.rom",    0x20000, 0x8000, 0x826e8a97 )
	ROM_LOAD( "b_14e.rom",    0x28000, 0x8000, 0x6cfc02a4 )
	ROM_LOAD( "b_14f.rom",    0x30000, 0x8000, 0x9b9f6730 )
	ROM_LOAD( "b_14g.rom",    0x38000, 0x8000, 0xef6af630 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b_11b.rom",    0x00000, 0x8000, 0xeb6f278c ) /* sprites */
	ROM_LOAD( "b_13b.rom",    0x08000, 0x8000, 0xe91b4014 )
	ROM_LOAD( "b_11a.rom",    0x10000, 0x8000, 0x2822c522 )
	ROM_LOAD( "b_13a.rom",    0x18000, 0x8000, 0x3e8a9f75 )
	ROM_LOAD( "b_12b.rom",    0x20000, 0x8000, 0x86e43eda )
	ROM_LOAD( "b_14b.rom",    0x28000, 0x8000, 0x076e92d1 )
	ROM_LOAD( "b_12a.rom",    0x30000, 0x8000, 0xce107f3c )
	ROM_LOAD( "b_14a.rom",    0x38000, 0x8000, 0xdba06076 )

	ROM_REGION( 0x08000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "b_03d.rom",    0x0000, 0x8000, 0x6f348008 )
ROM_END

ROM_START( sidearmr )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + banked ROMs images */
	ROM_LOAD( "03",           0x00000, 0x08000, 0x9a799c45 )        /* CODE */
	ROM_LOAD( "a_14e.rom",    0x10000, 0x08000, 0x4925ed03 )        /* 0+1 */
	ROM_LOAD( "a_12e.rom",    0x18000, 0x08000, 0x81d0ece7 )        /* 2+3 */

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "a_04k.rom",    0x0000, 0x8000, 0x34efe2d2 )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* unknown, looks like Z80 code */
	ROM_LOAD( "b_11j.rom",    0x0000, 0x8000, 0x134dc35b )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a_10j.rom",    0x00000, 0x4000, 0x651fef75 ) /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b_13d.rom",    0x00000, 0x8000, 0x3c59afe1 ) /* tiles */
	ROM_LOAD( "b_13e.rom",    0x08000, 0x8000, 0x64bc3b77 )
	ROM_LOAD( "b_13f.rom",    0x10000, 0x8000, 0xe6bcea6f )
	ROM_LOAD( "b_13g.rom",    0x18000, 0x8000, 0xc71a3053 )
	ROM_LOAD( "b_14d.rom",    0x20000, 0x8000, 0x826e8a97 )
	ROM_LOAD( "b_14e.rom",    0x28000, 0x8000, 0x6cfc02a4 )
	ROM_LOAD( "b_14f.rom",    0x30000, 0x8000, 0x9b9f6730 )
	ROM_LOAD( "b_14g.rom",    0x38000, 0x8000, 0xef6af630 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b_11b.rom",    0x00000, 0x8000, 0xeb6f278c ) /* sprites */
	ROM_LOAD( "b_13b.rom",    0x08000, 0x8000, 0xe91b4014 )
	ROM_LOAD( "b_11a.rom",    0x10000, 0x8000, 0x2822c522 )
	ROM_LOAD( "b_13a.rom",    0x18000, 0x8000, 0x3e8a9f75 )
	ROM_LOAD( "b_12b.rom",    0x20000, 0x8000, 0x86e43eda )
	ROM_LOAD( "b_14b.rom",    0x28000, 0x8000, 0x076e92d1 )
	ROM_LOAD( "b_12a.rom",    0x30000, 0x8000, 0xce107f3c )
	ROM_LOAD( "b_14a.rom",    0x38000, 0x8000, 0xdba06076 )

	ROM_REGION( 0x08000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "b_03d.rom",    0x0000, 0x8000, 0x6f348008 )
ROM_END

ROM_START( sidearjp )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + banked ROMs images */
	ROM_LOAD( "a_15e.rom",    0x00000, 0x08000, 0x61ceb0cc )        /* CODE */
	ROM_LOAD( "a_14e.rom",    0x10000, 0x08000, 0x4925ed03 )        /* 0+1 */
	ROM_LOAD( "a_12e.rom",    0x18000, 0x08000, 0x81d0ece7 )        /* 2+3 */

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "a_04k.rom",    0x0000, 0x8000, 0x34efe2d2 )

	ROM_REGION( 0x10000, REGION_CPU3 )	/* unknown, looks like Z80 code */
	ROM_LOAD( "b_11j.rom",    0x0000, 0x8000, 0x134dc35b )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a_10j.rom",    0x00000, 0x4000, 0x651fef75 ) /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b_13d.rom",    0x00000, 0x8000, 0x3c59afe1 ) /* tiles */
	ROM_LOAD( "b_13e.rom",    0x08000, 0x8000, 0x64bc3b77 )
	ROM_LOAD( "b_13f.rom",    0x10000, 0x8000, 0xe6bcea6f )
	ROM_LOAD( "b_13g.rom",    0x18000, 0x8000, 0xc71a3053 )
	ROM_LOAD( "b_14d.rom",    0x20000, 0x8000, 0x826e8a97 )
	ROM_LOAD( "b_14e.rom",    0x28000, 0x8000, 0x6cfc02a4 )
	ROM_LOAD( "b_14f.rom",    0x30000, 0x8000, 0x9b9f6730 )
	ROM_LOAD( "b_14g.rom",    0x38000, 0x8000, 0xef6af630 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b_11b.rom",    0x00000, 0x8000, 0xeb6f278c ) /* sprites */
	ROM_LOAD( "b_13b.rom",    0x08000, 0x8000, 0xe91b4014 )
	ROM_LOAD( "b_11a.rom",    0x10000, 0x8000, 0x2822c522 )
	ROM_LOAD( "b_13a.rom",    0x18000, 0x8000, 0x3e8a9f75 )
	ROM_LOAD( "b_12b.rom",    0x20000, 0x8000, 0x86e43eda )
	ROM_LOAD( "b_14b.rom",    0x28000, 0x8000, 0x076e92d1 )
	ROM_LOAD( "b_12a.rom",    0x30000, 0x8000, 0xce107f3c )
	ROM_LOAD( "b_14a.rom",    0x38000, 0x8000, 0xdba06076 )

	ROM_REGION( 0x08000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "b_03d.rom",    0x0000, 0x8000, 0x6f348008 )
ROM_END

ROM_START( turtship )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + banked ROMs images */
	ROM_LOAD( "turtship.003",    0x00000, 0x08000, 0xe7a7fc2e )
	ROM_LOAD( "turtship.002",    0x10000, 0x08000, 0xe576f482 )
	ROM_LOAD( "turtship.001",    0x18000, 0x08000, 0xa9b64240 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "turtship.004",    0x0000, 0x8000, 0x1cbe48e8 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "turtship.005",    0x00000, 0x04000, 0x651fef75 )	/* characters */

	ROM_REGION( 0x60000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "turtship.008",    0x00000, 0x10000, 0xe0658469 )	/* tiles */
	ROM_LOAD( "turtship.010",    0x10000, 0x10000, 0x76bb73bb )
	ROM_LOAD( "turtship.011",    0x20000, 0x10000, 0x53da6cb1 )
	ROM_LOAD( "turtship.006",    0x30000, 0x10000, 0xa7cce654 )
	ROM_LOAD( "turtship.007",    0x40000, 0x10000, 0x3ccf11b9 )
	ROM_LOAD( "turtship.009",    0x50000, 0x10000, 0x44762916 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "turtship.013",    0x00000, 0x10000, 0x599f5246 )	/* sprites */
	ROM_LOAD( "turtship.015",    0x10000, 0x10000, 0x69fd202f )
	ROM_LOAD( "turtship.012",    0x20000, 0x10000, 0xfb54cd33 )
	ROM_LOAD( "turtship.014",    0x30000, 0x10000, 0xb3ea74a3 )

	ROM_REGION( 0x08000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "turtship.016",    0x0000, 0x8000, 0xaffd51dd )
ROM_END

ROM_START( dyger )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + banked ROMs images */
	ROM_LOAD( "dyger.003",    0x00000, 0x08000, 0xbae9882e )
	ROM_LOAD( "dyger.002",    0x10000, 0x08000, 0x059ac4dc )
	ROM_LOAD( "dyger.001",    0x18000, 0x08000, 0xd8440f66 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "dyger.004",    0x0000, 0x8000, 0x8a256c09 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dyger.005",    0x00000, 0x04000, 0xc4bc72a5 )	/* characters */
	ROM_CONTINUE(             0x00000, 0x04000 )	/* is the first half used? */

	ROM_REGION( 0x60000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dyger.010",    0x00000, 0x10000, 0x9715880d )	/* tiles */
	ROM_LOAD( "dyger.009",    0x10000, 0x10000, 0x628dae72 )
	ROM_LOAD( "dyger.011",    0x20000, 0x10000, 0x23248db1 )
	ROM_LOAD( "dyger.006",    0x30000, 0x10000, 0x4ba7a437 )
	ROM_LOAD( "dyger.008",    0x40000, 0x10000, 0x6c0f0e0c )
	ROM_LOAD( "dyger.007",    0x50000, 0x10000, 0x2c50a229 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dyger.014",    0x00000, 0x10000, 0x99c60b26 )	/* sprites */
	ROM_LOAD( "dyger.015",    0x10000, 0x10000, 0xd6475ecc )
	ROM_LOAD( "dyger.012",    0x20000, 0x10000, 0xe345705f )
	ROM_LOAD( "dyger.013",    0x30000, 0x10000, 0xfaf4be3a )

	ROM_REGION( 0x08000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "dyger.016",    0x0000, 0x8000, 0x0792e8f2 )
ROM_END

ROM_START( dygera )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + banked ROMs images */
	ROM_LOAD( "dygar_t3.bin", 0x00000, 0x08000, 0xfc63da8b )
	ROM_LOAD( "dyger.002",    0x10000, 0x08000, 0x059ac4dc )
	ROM_LOAD( "dyger.001",    0x18000, 0x08000, 0xd8440f66 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "dyger.004",    0x0000, 0x8000, 0x8a256c09 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dyger.005",    0x00000, 0x04000, 0xc4bc72a5 )	/* characters */
	ROM_CONTINUE(             0x00000, 0x04000 )	/* is the first half used? */

	ROM_REGION( 0x60000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dyger.010",    0x00000, 0x10000, 0x9715880d )	/* tiles */
	ROM_LOAD( "dyger.009",    0x10000, 0x10000, 0x628dae72 )
	ROM_LOAD( "dyger.011",    0x20000, 0x10000, 0x23248db1 )
	ROM_LOAD( "dyger.006",    0x30000, 0x10000, 0x4ba7a437 )
	ROM_LOAD( "dyger.008",    0x40000, 0x10000, 0x6c0f0e0c )
	ROM_LOAD( "dyger.007",    0x50000, 0x10000, 0x2c50a229 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dyger.014",    0x00000, 0x10000, 0x99c60b26 )	/* sprites */
	ROM_LOAD( "dyger.015",    0x10000, 0x10000, 0xd6475ecc )
	ROM_LOAD( "dyger.012",    0x20000, 0x10000, 0xe345705f )
	ROM_LOAD( "dyger.013",    0x30000, 0x10000, 0xfaf4be3a )

	ROM_REGION( 0x08000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "dyger.016",    0x0000, 0x8000, 0x0792e8f2 )
ROM_END



GAMEX( 1986, sidearms, 0,        sidearms, sidearms, 0, ROT0,   "Capcom", "Side Arms - Hyper Dyne (World)", GAME_NO_COCKTAIL )
GAMEX( 1988, sidearmr, sidearms, sidearms, sidearms, 0, ROT0,   "Capcom (Romstar license)", "Side Arms - Hyper Dyne (US)", GAME_NO_COCKTAIL )
GAMEX( 1986, sidearjp, sidearms, sidearms, sidearms, 0, ROT0,   "Capcom", "Side Arms - Hyper Dyne (Japan)", GAME_NO_COCKTAIL )
GAMEX( 1988, turtship, 0,        turtship, turtship, 0, ROT0,   "Philko", "Turtle Ship", GAME_NO_COCKTAIL )
GAMEX( 1989, dyger,    0,        turtship, dyger,    0, ROT270, "Philko", "Dyger (set 1)", GAME_NO_COCKTAIL )
GAMEX( 1989, dygera,   dyger,    turtship, dyger,    0, ROT270, "Philko", "Dyger (set 2)", GAME_NO_COCKTAIL )
