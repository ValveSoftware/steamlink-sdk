#include "../vidhrdw/wiz.cpp"

/***************************************************************************

Wiz/Stinger/Scion memory map (preliminary)

Driver by Zsolt Vasvari


These boards are similar to a Galaxian board in the way it handles scrolling
and sprites, but the similarities pretty much end there. The most notable
difference is that there are 2 independently scrollable playfields.


Main CPU:

0000-BFFF  ROM
C000-C7FF  RAM
D000-D3FF  Video RAM (Foreground)
D400-D7FF  Color RAM (Foreground) (Wiz)
D800-D83F  Attributes RAM (Foreground)
D840-D85F  Sprite RAM 1
E000-E3FF  Video RAM (Background)
E400-E7FF  Color RAM (Background) (Wiz)
E800-E83F  Attributes RAM (Background)
E840-E85F  Sprite RAM 2

I/O read:
f000 DIP SW#1
f008 DIP SW#2
f010 Input Port 1
f018 Input Port 2
f800 Watchdog

I/O write:
c800 Coin Counter A
c801 Coin Counter B
f000 Sprite bank select (Wiz)
f001 NMI enable
f002 \ Palette select
f003 /
f004 \ Character bank select
f005 /
f006 \ Flip screen
f007 /
f800 Sound Command write
f818 (?) Sound or Background color


Sound CPU:

0000-1FFF  ROM
2000-23FF  RAM

I/O read:
3000 Sound Command Read (Stinger/Scion)
7000 Sound Command Read (Wiz)

I/O write:
3000 NMI enable	(Stinger/Scion)
4000 AY8910 Control Port #1	(Wiz)
4001 AY8910 Write Port #1	(Wiz)
5000 AY8910 Control Port #2
5001 AY8910 Write Port #2
6000 AY8910 Control Port #3
6001 AY8910 Write Port #3
7000 NMI enable (Wiz)


TODO:

- Verify/Fix colors
- Sprite banking in Wiz. I have a hack in wiz_vh_screenrefresh
- background noise in scion (but not scionc). Note that the sound program is
  almost identical, except for three patches affecting noise period, noise
  channel C enable and channel C volume. So it looks just like a bug in the
  original (weird), or some strange form of protection.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char *wiz_videoram2;
extern unsigned char *wiz_colorram2;
extern unsigned char *wiz_attributesram;
extern unsigned char *wiz_attributesram2;
extern unsigned char *wiz_sprite_bank;

WRITE_HANDLER( wiz_char_bank_select_w );
WRITE_HANDLER( wiz_attributes_w );
WRITE_HANDLER( wiz_palettebank_w );
void wiz_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void wiz_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void stinger_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( wiz_flipx_w );
WRITE_HANDLER( wiz_flipy_w );


static WRITE_HANDLER( sound_command_w )
{
	if (data == 0x90)
	{
		/* ??? */
	}
	else
		soundlatch_w(0,data);	/* ??? */
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xd000, 0xd85f, MRA_RAM },
	{ 0xe000, 0xe85f, MRA_RAM },
	{ 0xf000, 0xf000, input_port_2_r },	/* DSW0 */
	{ 0xf008, 0xf008, input_port_3_r },	/* DSW1 */
	{ 0xf010, 0xf010, input_port_0_r },	/* IN0 */
	{ 0xf018, 0xf018, input_port_1_r },	/* IN1 */
	{ 0xf800, 0xf800, watchdog_reset_r },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xc800, 0xc801, coin_counter_w },
	{ 0xd000, 0xd3ff, MWA_RAM, &wiz_videoram2 },
	{ 0xd400, 0xd7ff, MWA_RAM, &wiz_colorram2 },
	{ 0xd800, 0xd83f, MWA_RAM, &wiz_attributesram2 },
	{ 0xd840, 0xd85f, MWA_RAM, &spriteram_2, &spriteram_size },
	{ 0xe000, 0xe3ff, videoram_w, &videoram, &videoram_size },
	{ 0xe400, 0xe7ff, colorram_w, &colorram },
	{ 0xe800, 0xe83f, wiz_attributes_w, &wiz_attributesram },
	{ 0xe840, 0xe85f, MWA_RAM, &spriteram },
	{ 0xf000, 0xf000, MWA_RAM, &wiz_sprite_bank },
	{ 0xf001, 0xf001, interrupt_enable_w },
	{ 0xf002, 0xf003, wiz_palettebank_w },
	{ 0xf004, 0xf005, wiz_char_bank_select_w },
	{ 0xf006, 0xf006, wiz_flipx_w },
	{ 0xf007, 0xf007, wiz_flipy_w },
	{ 0xf800, 0xf800, sound_command_w },
	{ 0xf808, 0xf808, MWA_NOP },	/* explosion sound trigger; analog? */
	{ 0xf80a, 0xf80a, MWA_NOP },	/* shoot sound trigger; analog? */
	{ 0xf818, 0xf818, MWA_NOP },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x23ff, MRA_RAM },
	{ 0x3000, 0x3000, soundlatch_r },  /* Stinger/Scion */
	{ 0x7000, 0x7000, soundlatch_r },  /* Wiz */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x2000, 0x23ff, MWA_RAM },
	{ 0x3000, 0x3000, interrupt_enable_w },		/* Stinger/Scion */
	{ 0x4000, 0x4000, AY8910_control_port_2_w },
	{ 0x4001, 0x4001, AY8910_write_port_2_w },
	{ 0x5000, 0x5000, AY8910_control_port_0_w },
	{ 0x5001, 0x5001, AY8910_write_port_0_w },
	{ 0x6000, 0x6000, AY8910_control_port_1_w },	/* Wiz only */
	{ 0x6001, 0x6001, AY8910_write_port_1_w },	/* Wiz only */
	{ 0x7000, 0x7000, interrupt_enable_w },		/* Wiz */
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( wiz )
	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )

	PORT_START      /* DSW 0 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START      /* DSW 1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x06, "4" )
	PORT_DIPNAME( 0x18, 0x10, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x18, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "255", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x60, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "10000 30000" )
	PORT_DIPSETTING(    0x20, "20000 40000" )
	PORT_DIPSETTING(    0x40, "30000 60000" )
	PORT_DIPSETTING(    0x60, "40000 80000" )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )
INPUT_PORTS_END

INPUT_PORTS_START( stinger )
	PORT_START	/* IN0 */
    PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
    PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
    PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2 )
    PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
    PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
    PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
    PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 )
    PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START	/* IN1 */
    PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
    PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
    PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
    PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
    PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x18, 0x08, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x18, "5" )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0xe0, "20000 50000" )
	PORT_DIPSETTING(    0xc0, "20000 60000" )
	PORT_DIPSETTING(    0xa0, "20000 70000" )
	PORT_DIPSETTING(    0x80, "20000 80000" )
	PORT_DIPSETTING(    0x60, "20000 90000" )
	PORT_DIPSETTING(    0x40, "30000 80000" )
	PORT_DIPSETTING(    0x20, "30000 90000" )
	PORT_DIPSETTING(    0x00, "None" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )	  // Doesn't seem to be referenced
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )    // Doesn't seem to be referenced
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )    // Doesn't seem to be referenced
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x70, 0x70, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_8C ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
INPUT_PORTS_END

INPUT_PORTS_START( scion )
	PORT_START	/* IN0 */
    PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
    PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
    PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2 )
    PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
    PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
    PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
    PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 )
    PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START	/* IN1 */
    PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
    PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
    PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
    PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
    PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x02, "Hard" )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x0c, "5" )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20000 40000" )
	PORT_DIPSETTING(    0x20, "20000 60000" )
	PORT_DIPSETTING(    0x10, "20000 80000" )
	PORT_DIPSETTING(    0x30, "30000 90000" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )    // Doesn't seem to be referenced
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )    // Doesn't seem to be referenced
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
  //PORT_DIPSETTING(    0x20, DEF_STR( Off ) )  /* This setting will screw up the game */
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
  //PORT_DIPSETTING(    0x40, DEF_STR( Off ) )  /* This setting will screw up the game */
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
  //PORT_DIPSETTING(    0x80, DEF_STR( Off ) )  /* This setting will screw up the game */
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	256,    /* 256 characters */
	3,      /* 3 bits per pixel */
	{ 0x4000*8, 0x2000*8, 0 }, /* the three bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};


static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	256,    /* 256 sprites */
	3,      /* 3 bits per pixel */
	{ 0x4000*8, 0x2000*8, 0 }, /* the three bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
	 8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8     /* every sprite takes 32 consecutive bytes */
};


// I don't know how the 32 colors are used. I'm making the
// forest and the title screen green/brown, and the ice and sky levels
// white/blue. What we need is screenshots.
static struct GfxDecodeInfo wiz_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,   0, 32 },
	{ REGION_GFX1, 0x0800, &charlayout,   0, 32 },
	{ REGION_GFX2, 0x6000, &charlayout,   0, 32 },
	{ REGION_GFX2, 0x0000, &charlayout,   0, 32 },
	{ REGION_GFX2, 0x0800, &charlayout,   0, 32 },
	{ REGION_GFX2, 0x6800, &charlayout,   0, 32 },
	{ REGION_GFX1, 0x0000, &spritelayout, 0, 32 },
	{ REGION_GFX2, 0x0000, &spritelayout, 0, 32 },
	{ REGION_GFX2, 0x6000, &spritelayout, 0, 32 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo stinger_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &charlayout,   0, 32 },
	{ REGION_GFX1, 0x0800, &charlayout,   0, 32 },
	{ REGION_GFX2, 0x0000, &charlayout,   0, 32 },
	{ REGION_GFX2, 0x0800, &charlayout,   0, 32 },
	{ REGION_GFX1, 0x0000, &spritelayout, 0, 32 },
	{ REGION_GFX2, 0x0000, &spritelayout, 0, 32 },
	{ -1 } /* end of array */
};


static struct AY8910interface wiz_ay8910_interface =
{
	3,      /* 3 chips */
	14318000/8,	/* ? */
	{ 10, 10, 10 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct AY8910interface stinger_ay8910_interface =
{
	2,      /* 2 chips */
	14318000/8,	/* ? */
	{ 25, 25 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};


#define MACHINE_DRIVER(NAME)									\
static struct MachineDriver machine_driver_##NAME =				\
{																\
	{															\
		{														\
			CPU_Z80,											\
			18432000/6,     /* 3.072 Mhz ??? */					\
			readmem,writemem,0,0,								\
			nmi_interrupt,1										\
		},														\
		{														\
			CPU_Z80 | CPU_AUDIO_CPU,							\
			14318000/8,     /* ? */								\
			sound_readmem,sound_writemem,0,0,					\
			nmi_interrupt,3 /* ??? */							\
		}														\
	},															\
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */			\
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */	\
	0,															\
																\
	/* video hardware */										\
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },					\
	NAME##_gfxdecodeinfo,										\
	256,32*8,													\
	wiz_vh_convert_color_prom,									\
																\
	VIDEO_TYPE_RASTER,											\
	0,															\
	generic_vh_start,											\
	generic_vh_stop,											\
	NAME##_vh_screenrefresh,									\
																\
	/* sound hardware */										\
	0,0,0,0,													\
	{															\
		{														\
			SOUND_AY8910,										\
			&NAME##_ay8910_interface							\
		}														\
	}															\
}

MACHINE_DRIVER(wiz);
MACHINE_DRIVER(stinger);


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( wiz )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "ic07_01.bin",  0x0000, 0x4000, 0xc05f2c78 )
	ROM_LOAD( "ic05_03.bin",  0x4000, 0x4000, 0x7978d879 )
	ROM_LOAD( "ic06_02.bin",  0x8000, 0x4000, 0x9c406ad2 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "ic57_10.bin",  0x0000, 0x2000, 0x8a7575bd )

	ROM_REGION( 0x6000,  REGION_GFX1 | REGIONFLAG_DISPOSE )	/* sprites/chars */
	ROM_LOAD( "ic12_04.bin",  0x0000, 0x2000, 0x8969acdd )
	ROM_LOAD( "ic13_05.bin",  0x2000, 0x2000, 0x2868e6a5 )
	ROM_LOAD( "ic14_06.bin",  0x4000, 0x2000, 0xb398e142 )

	ROM_REGION( 0xc000,  REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites/chars */
	ROM_LOAD( "ic03_07.bin",  0x0000, 0x2000, 0x297c02fc )
	ROM_CONTINUE(		      0x6000, 0x2000  )
	ROM_LOAD( "ic02_08.bin",  0x2000, 0x2000, 0xede77d37 )
	ROM_CONTINUE(		      0x8000, 0x2000  )
	ROM_LOAD( "ic01_09.bin",  0x4000, 0x2000, 0x4d86b041 )
	ROM_CONTINUE(		      0xa000, 0x2000  )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "ic23_3-1.bin", 0x0000, 0x0100, 0x2dd52fb2 ) /* palette red component */
	ROM_LOAD( "ic23_3-2.bin", 0x0100, 0x0100, 0x8c2880c9 ) /* palette green component */
	ROM_LOAD( "ic23_3-3.bin", 0x0200, 0x0100, 0xa488d761 ) /* palette blue component */
ROM_END

ROM_START( wizt )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "wiz1.bin",  	  0x0000, 0x4000, 0x5a6d3c60 )
	ROM_LOAD( "ic05_03.bin",  0x4000, 0x4000, 0x7978d879 )
	ROM_LOAD( "ic06_02.bin",  0x8000, 0x4000, 0x9c406ad2 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "ic57_10.bin",  0x0000, 0x2000, 0x8a7575bd )

	ROM_REGION( 0x6000,  REGION_GFX1 | REGIONFLAG_DISPOSE )	/* sprites/chars */
	ROM_LOAD( "wiz4.bin",     0x0000, 0x2000, 0xe6c636b3 )
	ROM_LOAD( "wiz5.bin",     0x2000, 0x2000, 0x77986058 )
	ROM_LOAD( "wiz6.bin",     0x4000, 0x2000, 0xf6970b23 )

	ROM_REGION( 0xc000,  REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites/chars */
	ROM_LOAD( "wiz7.bin",     0x0000, 0x2000, 0x601f2f3f )
	ROM_CONTINUE(		      0x6000, 0x2000  )
	ROM_LOAD( "wiz8.bin",     0x2000, 0x2000, 0xf5ab982d )
	ROM_CONTINUE(		      0x8000, 0x2000  )
	ROM_LOAD( "wiz9.bin",     0x4000, 0x2000, 0xf6c662e2 )
	ROM_CONTINUE(		      0xa000, 0x2000  )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "ic23_3-1.bin", 0x0000, 0x0100, 0x2dd52fb2 ) /* palette red component */
	ROM_LOAD( "ic23_3-2.bin", 0x0100, 0x0100, 0x8c2880c9 ) /* palette green component */
	ROM_LOAD( "ic23_3-3.bin", 0x0200, 0x0100, 0xa488d761 ) /* palette blue component */
ROM_END

ROM_START( stinger )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "n1.bin",       0x0000, 0x2000, 0xf2d2790c )	/* encrypted */
	ROM_LOAD( "n2.bin",       0x2000, 0x2000, 0x8fd2d8d8 )	/* encrypted */
	ROM_LOAD( "n3.bin",       0x4000, 0x2000, 0xf1794d36 )	/* encrypted */
	ROM_LOAD( "n4.bin",       0x6000, 0x2000, 0x230ba682 )	/* encrypted */
	ROM_LOAD( "n5.bin",       0x8000, 0x2000, 0xa03a01da )	/* encrypted */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu */
	ROM_LOAD( "6.bin",        0x0000, 0x2000, 0x79757f0c )

	ROM_REGION( 0x6000,  REGION_GFX1 | REGIONFLAG_DISPOSE )	/* sprites/chars */
	ROM_LOAD( "7.bin",        0x0000, 0x2000, 0x775489be )
	ROM_LOAD( "8.bin",        0x2000, 0x2000, 0x43c61b3f )
	ROM_LOAD( "9.bin",        0x4000, 0x2000, 0xc9ed8fc7 )

	ROM_REGION( 0x6000,  REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites/chars */
	ROM_LOAD( "10.bin",       0x0000, 0x2000, 0xf6721930 )
	ROM_LOAD( "11.bin",       0x2000, 0x2000, 0xa4404e63 )
	ROM_LOAD( "12.bin",       0x4000, 0x2000, 0xb60fa88c )

	ROM_REGION( 0x0300,  REGION_PROMS )
	ROM_LOAD( "stinger.a7",   0x0000, 0x0100, 0x52c06fc2 )	/* red component */
	ROM_LOAD( "stinger.b7",   0x0100, 0x0100, 0x9985e575 )	/* green component */
	ROM_LOAD( "stinger.a8",   0x0200, 0x0100, 0x76b57629 )	/* blue component */
ROM_END

ROM_START( scion )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "sc1",          0x0000, 0x2000, 0x8dcad575 )
	ROM_LOAD( "sc2",          0x2000, 0x2000, 0xf608e0ba )
	ROM_LOAD( "sc3",          0x4000, 0x2000, 0x915289b9 )
	ROM_LOAD( "4.9j",         0x6000, 0x2000, 0x0f40d002 )
	ROM_LOAD( "5.10j",        0x8000, 0x2000, 0xdc4923b7 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu */
	ROM_LOAD( "sc6",         0x0000, 0x2000, 0x09f5f9c1 )

	ROM_REGION( 0x6000,  REGION_GFX1 | REGIONFLAG_DISPOSE )	/* sprites/chars */
	ROM_LOAD( "7.10e",        0x0000, 0x2000, 0x223e0d2a )
	ROM_LOAD( "8.12e",        0x2000, 0x2000, 0xd3e39b48 )
	ROM_LOAD( "9.15e",        0x4000, 0x2000, 0x630861b5 )

	ROM_REGION( 0x6000,  REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites/chars */
	ROM_LOAD( "10.10h",       0x0000, 0x2000, 0x0d2a0d1e )
	ROM_LOAD( "11.12h",       0x2000, 0x2000, 0xdc6ef8ab )
	ROM_LOAD( "12.15h",       0x4000, 0x2000, 0xc82c28bf )

	ROM_REGION( 0x0300,  REGION_PROMS )
	ROM_LOAD( "82s129.7a",    0x0000, 0x0100, 0x2f89d9ea )	/* red component */
	ROM_LOAD( "82s129.7b",    0x0100, 0x0100, 0xba151e6a )	/* green component */
	ROM_LOAD( "82s129.8a",    0x0200, 0x0100, 0xf681ce59 )	/* blue component */
ROM_END

ROM_START( scionc )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "1.5j",         0x0000, 0x2000, 0x5aaf571e )
	ROM_LOAD( "2.6j",         0x2000, 0x2000, 0xd5a66ac9 )
	ROM_LOAD( "3.8j",         0x4000, 0x2000, 0x6e616f28 )
	ROM_LOAD( "4.9j",         0x6000, 0x2000, 0x0f40d002 )
	ROM_LOAD( "5.10j",        0x8000, 0x2000, 0xdc4923b7 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu */
	ROM_LOAD( "6.9f",         0x0000, 0x2000, 0xa66a0ce6 )

	ROM_REGION( 0x6000,  REGION_GFX1 | REGIONFLAG_DISPOSE )	/* sprites/chars */
	ROM_LOAD( "7.10e",        0x0000, 0x2000, 0x223e0d2a )
	ROM_LOAD( "8.12e",        0x2000, 0x2000, 0xd3e39b48 )
	ROM_LOAD( "9.15e",        0x4000, 0x2000, 0x630861b5 )

	ROM_REGION( 0x6000,  REGION_GFX2 | REGIONFLAG_DISPOSE )	/* sprites/chars */
	ROM_LOAD( "10.10h",       0x0000, 0x2000, 0x0d2a0d1e )
	ROM_LOAD( "11.12h",       0x2000, 0x2000, 0xdc6ef8ab )
	ROM_LOAD( "12.15h",       0x4000, 0x2000, 0xc82c28bf )

	ROM_REGION( 0x0300,  REGION_PROMS )
	ROM_LOAD( "82s129.7a",    0x0000, 0x0100, 0x2f89d9ea )	/* red component */
	ROM_LOAD( "82s129.7b",    0x0100, 0x0100, 0xba151e6a )	/* green component */
	ROM_LOAD( "82s129.8a",    0x0200, 0x0100, 0xf681ce59 )	/* blue component */
ROM_END



static void init_stinger(void)
{
	static const unsigned char xortable[4][4] =
	{
		{ 0xa0,0x88,0x88,0xa0 },	/* .........00.0... */
		{ 0x88,0x00,0xa0,0x28 },	/* .........00.1... */
		{ 0x80,0xa8,0x20,0x08 },	/* .........01.0... */
		{ 0x28,0x28,0x88,0x88 }		/* .........01.1... */
	};
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;
	int A;


	memory_set_opcode_base(0,rom+diff);

	for (A = 0x0000;A < 0x10000;A++)
	{
		int row,col;
		unsigned char src;


		if (A & 0x2040)
		{
			/* not encrypted */
			rom[A+diff] = rom[A];
		}
		else
		{
			src = rom[A];

			/* pick the translation table from bits 3 and 5 */
			row = ((A >> 3) & 1) + (((A >> 5) & 1) << 1);

			/* pick the offset in the table from bits 3 and 5 of the source data */
			col = ((src >> 3) & 1) + (((src >> 5) & 1) << 1);
			/* the bottom half of the translation table is the mirror image of the top */
			if (src & 0x80) col = 3 - col;

			/* decode the opcodes */
			rom[A+diff] = src ^ xortable[row][col];
		}
	}
}



GAMEX( 1983, stinger, 0,     stinger, stinger, stinger, ROT90,  "Seibu Denshi", "Stinger",     GAME_IMPERFECT_COLORS )
GAMEX( 1984, scion,   0,     stinger, scion,   0,       ROT0,   "Seibu Denshi", "Scion",       GAME_IMPERFECT_COLORS )
GAMEX( 1984, scionc,  scion, stinger, scion,   0,       ROT0,   "Seibu Denshi [Cinematronics license]", "Scion (Cinematronics)", GAME_IMPERFECT_COLORS )
GAMEX( 1985, wiz,     0,     wiz,     wiz,     0,       ROT270, "Seibu Kaihatsu Inc.", "Wiz",  GAME_IMPERFECT_COLORS )
GAMEX( 1985, wizt,    wiz,   wiz,     wiz,     0,       ROT270, "Taito Corp.",  "Wiz (Taito)", GAME_IMPERFECT_COLORS )
