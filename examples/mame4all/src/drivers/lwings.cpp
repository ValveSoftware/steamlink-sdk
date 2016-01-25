#include "../machine/lwings.cpp"
#include "../vidhrdw/lwings.cpp"

/***************************************************************************

  Legendary Wings
  Section Z
  Trojan
  Avengers

  Driver provided by Paul Leaman

  Trojan contains a third Z80 to drive the game samples. This third
  Z80 outputs the ADPCM data byte at a time to the sound hardware. Since
  this will be expensive to do this extra processor is not emulated.

  Instead, the ADPCM data is lifted directly from the sound ROMS.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

WRITE_HANDLER( lwings_bankswitch_w );
READ_HANDLER( lwings_bankedrom_r );
int lwings_interrupt(void);
READ_HANDLER( avengers_protection_r );
WRITE_HANDLER( avengers_protection_w );

extern unsigned char *lwings_backgroundram;
extern unsigned char *lwings_backgroundattribram;
extern size_t lwings_backgroundram_size;
extern unsigned char *lwings_scrolly;
extern unsigned char *lwings_scrollx;
extern unsigned char *lwings_palette_bank;
WRITE_HANDLER( lwings_background_w );
WRITE_HANDLER( lwings_backgroundattrib_w );
WRITE_HANDLER( lwings_palette_bank_w );
int  lwings_vh_start(void);
void lwings_vh_stop(void);
void lwings_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

extern unsigned char *trojan_scrolly;
extern unsigned char *trojan_scrollx;
extern unsigned char *trojan_bk_scrolly;
extern unsigned char *trojan_bk_scrollx;
int  avengers_vh_start(void);
int  trojan_vh_start(void);
void trojan_vh_stop(void);
void trojan_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( trojan_sound_cmd_w )
{
       soundlatch_w(offset, data);
       if (data != 0xff && (data & 0x08))
       {
	      /*
	      I assume that Trojan's ADPCM output is directly derived
	      from the sound code. I can't find an output port that
	      does this on either the sound board or main board.
	      */
	      ADPCM_trigger(0, data & 0x07);
	}

#if 0
	logerror("Sound Code=%02x\n", data);
#endif
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },   /* CODE */
	{ 0x8000, 0xbfff, MRA_BANK1 },  /* CODE */
	{ 0xc000, 0xf7ff, MRA_RAM },
	{ 0xf808, 0xf808, input_port_0_r },
	{ 0xf809, 0xf809, input_port_1_r },
	{ 0xf80a, 0xf80a, input_port_2_r },
	{ 0xf80b, 0xf80b, input_port_3_r },
	{ 0xf80c, 0xf80c, input_port_4_r },
	{ 0xf80d, 0xf80d, avengers_protection_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xddff, MWA_RAM },
	{ 0xde00, 0xdfff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe000, 0xe3ff, videoram_w, &videoram, &videoram_size },
	{ 0xe400, 0xe7ff, colorram_w, &colorram },
	{ 0xe800, 0xebff, lwings_background_w, &lwings_backgroundram, &lwings_backgroundram_size },
	{ 0xec00, 0xefff, lwings_backgroundattrib_w, &lwings_backgroundattribram },
	{ 0xf000, 0xf3ff, paletteram_RRRRGGGGBBBBxxxx_split2_w, &paletteram_2 },
	{ 0xf400, 0xf7ff, paletteram_RRRRGGGGBBBBxxxx_split1_w, &paletteram },
	{ 0xf800, 0xf801, MWA_RAM, &trojan_scrollx },
	{ 0xf802, 0xf803, MWA_RAM, &trojan_scrolly },
	{ 0xf804, 0xf804, MWA_RAM, &trojan_bk_scrollx },
	{ 0xf805, 0xf805, MWA_RAM, &trojan_bk_scrolly },
	{ 0xf808, 0xf809, MWA_RAM, &lwings_scrolly},
	{ 0xf80a, 0xf80b, MWA_RAM, &lwings_scrollx},
	{ 0xf80c, 0xf80c, soundlatch_w },
	{ 0xf80d, 0xf80d, watchdog_reset_w },
	{ 0xf80e, 0xf80e, lwings_bankswitch_w },
	{ -1 }  /* end of table */
};

/* TROJAN - intercept sound command write for ADPCM samples */

static struct MemoryWriteAddress trojan_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xddff, MWA_RAM },
	{ 0xde00, 0xdf7f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xdf80, 0xdfff, MWA_RAM },
	{ 0xe000, 0xe3ff, videoram_w, &videoram, &videoram_size },
	{ 0xe400, 0xe7ff, colorram_w, &colorram },
	{ 0xe800, 0xebff, lwings_background_w, &lwings_backgroundram, &lwings_backgroundram_size },
	{ 0xec00, 0xefff, lwings_backgroundattrib_w, &lwings_backgroundattribram },
	{ 0xf000, 0xf3ff, paletteram_RRRRGGGGBBBBxxxx_split2_w, &paletteram_2 },
	{ 0xf400, 0xf7ff, paletteram_RRRRGGGGBBBBxxxx_split1_w, &paletteram },
	{ 0xf800, 0xf801, MWA_RAM, &trojan_scrollx },
	{ 0xf802, 0xf803, MWA_RAM, &trojan_scrolly },
	{ 0xf804, 0xf804, MWA_RAM, &trojan_bk_scrollx },
	{ 0xf805, 0xf805, MWA_RAM, &trojan_bk_scrolly },
	{ 0xf809, 0xf809, avengers_protection_w },
	{ 0xf80c, 0xf80c, trojan_sound_cmd_w },
	{ 0xf80d, 0xf80d, watchdog_reset_w },
	{ 0xf80e, 0xf80e, lwings_bankswitch_w },
	{ -1 }  /* end of table */
};



static struct MemoryReadAddress sound_readmem[] =
{
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc800, 0xc800, soundlatch_r },
	{ 0xe006, 0xe006, MRA_RAM },    /* Avengers - ADPCM status?? */
	{ 0x0000, 0x7fff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xe000, 0xe000, YM2203_control_port_0_w },
	{ 0xe001, 0xe001, YM2203_write_port_0_w },
	{ 0xe002, 0xe002, YM2203_control_port_1_w },
	{ 0xe003, 0xe003, YM2203_write_port_1_w },
	{ 0xe006, 0xe006, MWA_RAM },    /* Avengers - ADPCM output??? */
	{ 0x0000, 0x7fff, MWA_ROM },
	{ -1 }  /* end of table */
};

INPUT_PORTS_START( sectionz )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */

	PORT_START      /* DSW0 */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x06, "Normal" )
	PORT_DIPSETTING(    0x04, "Difficult" )
	PORT_DIPSETTING(    0x00, "Very Difficult" )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x38, "20000 50000" )
	PORT_DIPSETTING(    0x18, "20000 60000" )
	PORT_DIPSETTING(    0x28, "20000 70000" )
	PORT_DIPSETTING(    0x08, "30000 60000" )
	PORT_DIPSETTING(    0x30, "30000 70000" )
	PORT_DIPSETTING(    0x10, "30000 80000" )
	PORT_DIPSETTING(    0x20, "40000 100000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, "Upright One Player" )
	PORT_DIPSETTING(    0x40, "Upright Two Players" )
/*      PORT_DIPSETTING(    0x80, "???" )       probably unused */
	PORT_DIPSETTING(    0xc0, DEF_STR( Cocktail ) )
INPUT_PORTS_END

INPUT_PORTS_START( lwings )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, "Unknown 1/2" )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x08, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_4C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPNAME( 0x06, 0x06, "Difficulty?" )
	PORT_DIPSETTING(    0x02, "Easy?" )
	PORT_DIPSETTING(    0x06, "Normal?" )
	PORT_DIPSETTING(    0x04, "Difficult?" )
	PORT_DIPSETTING(    0x00, "Very Difficult?" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0xe0, "20000 50000" )
	PORT_DIPSETTING(    0x60, "20000 60000" )
	PORT_DIPSETTING(    0xa0, "20000 70000" )
	PORT_DIPSETTING(    0x20, "30000 60000" )
	PORT_DIPSETTING(    0xc0, "30000 70000" )
	PORT_DIPSETTING(    0x40, "30000 80000" )
	PORT_DIPSETTING(    0x80, "40000 100000" )
	PORT_DIPSETTING(    0x00, "None" )
INPUT_PORTS_END

INPUT_PORTS_START( trojan )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, "Upright 1 Player" )
	PORT_DIPSETTING(    0x02, "Upright 2 Players" )
	PORT_DIPSETTING(    0x03, DEF_STR( Cocktail ) )
/* 0x01 same as 0x02 or 0x03 */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x10, "20000 60000" )
	PORT_DIPSETTING(    0x0c, "20000 70000" )
	PORT_DIPSETTING(    0x08, "20000 80000" )
	PORT_DIPSETTING(    0x1c, "30000 60000" )
	PORT_DIPSETTING(    0x18, "30000 70000" )
	PORT_DIPSETTING(    0x14, "30000 80000" )
	PORT_DIPSETTING(    0x04, "40000 80000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0xe0, 0xe0, "Starting Level" )
	PORT_DIPSETTING(    0xe0, "1" )
	PORT_DIPSETTING(    0xc0, "2" )
	PORT_DIPSETTING(    0xa0, "3" )
	PORT_DIPSETTING(    0x80, "4" )
	PORT_DIPSETTING(    0x60, "5" )
	PORT_DIPSETTING(    0x40, "6" )
/* 0x00 and 0x20 start at level 6 */

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x40, 0x40, "Flip Screen?" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )
INPUT_PORTS_END

/* Trojan with level selection - starting level dip switches not used */
INPUT_PORTS_START( trojanls )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, "Upright 1 Player" )
	PORT_DIPSETTING(    0x02, "Upright 2 Players" )
	PORT_DIPSETTING(    0x03, DEF_STR( Cocktail ) )
/* 0x01 same as 0x02 or 0x03 */
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x10, "20000 60000" )
	PORT_DIPSETTING(    0x0c, "20000 70000" )
	PORT_DIPSETTING(    0x08, "20000 80000" )
	PORT_DIPSETTING(    0x1c, "30000 60000" )
	PORT_DIPSETTING(    0x18, "30000 70000" )
	PORT_DIPSETTING(    0x14, "30000 80000" )
	PORT_DIPSETTING(    0x04, "40000 80000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x40, 0x40, "Flip Screen?" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	1024,   /* 1024 characters */
	2,      /* 2 bits per pixel */
	{ 0, 4 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8    /* every char takes 16 consecutive bytes */
};

static struct GfxLayout tilelayout =
{
	16,16,  /* 16*16 tiles */
	2048,   /* 2048 tiles */
	4,      /* 4 bits per pixel */
	{ 0x30000*8, 0x20000*8, 0x10000*8, 0x00000*8  },  /* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8    /* every tile takes 32 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	1024,   /* 1024 sprites */
	4,      /* 4 bits per pixel */
	{ 0x10000*8+4, 0x10000*8+0, 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			32*8+0, 32*8+1, 32*8+2, 32*8+3, 33*8+0, 33*8+1, 33*8+2, 33*8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8    /* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout spritelayout_trojan = /* LWings, with more sprites */
{
	16,16,  /* 16*16 sprites */
	2048,   /* 2048 sprites */
	4,      /* 4 bits per pixel */
	{ 0x20000*8+4, 0x20000*8+0, 4, 0 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			32*8+0, 32*8+1, 32*8+2, 32*8+3, 33*8+0, 33*8+1, 33*8+2, 33*8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8    /* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout bktilelayout_trojan =
{
	16,16,  /* 16*16 sprites */
	512,   /* 512 sprites */
	4,      /* 4 bits per pixel */
	{ 0x08000*8+0, 0x08000*8+4, 0, 4 },
	{ 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			32*8+0, 32*8+1, 32*8+2, 32*8+3, 33*8+0, 33*8+1, 33*8+2, 33*8+3 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8    /* every sprite takes 64 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   512, 16 }, /* colors 512-575 */
	{ REGION_GFX2, 0, &tilelayout,     0,  8 }, /* colors   0-127 */
	{ REGION_GFX3, 0, &spritelayout, 384,  8 }, /* colors 384-511 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfxdecodeinfo_trojan[] =
{
	{ REGION_GFX1, 0, &charlayout,          768, 16 },  /* colors 768-831 */
	{ REGION_GFX2, 0, &tilelayout,          256,  8 },  /* colors 256-383 */
	{ REGION_GFX3, 0, &spritelayout_trojan, 640,  8 },  /* colors 640-767 */
	{ REGION_GFX4, 0, &bktilelayout_trojan,   0,  8 },  /* colors   0-127 */
	{ -1 } /* end of array */
};



static struct YM2203interface ym2203_interface =
{
	2,                      /* 2 chips */
	1500000,        /* 1.5 MHz (?) */
	{ YM2203_VOL(10,20), YM2203_VOL(10,20) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_lwings =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,        /* 4 Mhz (?) */
			readmem,writemem,0,0,
			lwings_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,        /* 3 Mhz (?) */
			sound_readmem,sound_writemem,0,0,
			interrupt,4
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */
	1,      /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	lwings_vh_start,
	lwings_vh_stop,
	lwings_vh_screenrefresh,

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

ROM_START( lwings )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + 3*16k for the banked ROMs images */
	ROM_LOAD( "6c_lw01.bin",  0x00000, 0x8000, 0xb55a7f60 )
	ROM_LOAD( "7c_lw02.bin",  0x10000, 0x8000, 0xa5efbb1b )
	ROM_LOAD( "9c_lw03.bin",  0x18000, 0x8000, 0xec5cc201 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "11e_lw04.bin", 0x0000, 0x8000, 0xa20337a2 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "9h_lw05.bin",  0x00000, 0x4000, 0x091d923c ) /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "3e_lw14.bin",  0x00000, 0x8000, 0x5436392c ) /* tiles */
	ROM_LOAD( "1e_lw08.bin",  0x08000, 0x8000, 0xb491bbbb )
	ROM_LOAD( "3d_lw13.bin",  0x10000, 0x8000, 0xfdd1908a )
	ROM_LOAD( "1d_lw07.bin",  0x18000, 0x8000, 0x5c73d406 )
	ROM_LOAD( "3b_lw12.bin",  0x20000, 0x8000, 0x32e17b3c )
	ROM_LOAD( "1b_lw06.bin",  0x28000, 0x8000, 0x52e533c1 )
	ROM_LOAD( "3f_lw15.bin",  0x30000, 0x8000, 0x99e134ba )
	ROM_LOAD( "1f_lw09.bin",  0x38000, 0x8000, 0xc8f28777 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "3j_lw17.bin",  0x00000, 0x8000, 0x5ed1bc9b )  /* sprites */
	ROM_LOAD( "1j_lw11.bin",  0x08000, 0x8000, 0x2a0790d6 )
	ROM_LOAD( "3h_lw16.bin",  0x10000, 0x8000, 0xe8834006 )
	ROM_LOAD( "1h_lw10.bin",  0x18000, 0x8000, 0xb693f5a5 )
ROM_END

ROM_START( lwings2 )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + 3*16k for the banked ROMs images */
	ROM_LOAD( "u13-l",        0x00000, 0x8000, 0x3069c01c )
	ROM_LOAD( "u14-k",        0x10000, 0x8000, 0x5d91c828 )
	ROM_LOAD( "9c_lw03.bin",  0x18000, 0x8000, 0xec5cc201 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "11e_lw04.bin", 0x0000, 0x8000, 0xa20337a2 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "9h_lw05.bin",  0x00000, 0x4000, 0x091d923c )  /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b_03e.rom",    0x00000, 0x8000, 0x176e3027 )  /* tiles */
	ROM_LOAD( "b_01e.rom",    0x08000, 0x8000, 0xf5d25623 )
	ROM_LOAD( "b_03d.rom",    0x10000, 0x8000, 0x001caa35 )
	ROM_LOAD( "b_01d.rom",    0x18000, 0x8000, 0x0ba008c3 )
	ROM_LOAD( "b_03b.rom",    0x20000, 0x8000, 0x4f8182e9 )
	ROM_LOAD( "b_01b.rom",    0x28000, 0x8000, 0xf1617374 )
	ROM_LOAD( "b_03f.rom",    0x30000, 0x8000, 0x9b374dcc )
	ROM_LOAD( "b_01f.rom",    0x38000, 0x8000, 0x23654e0a )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b_03j.rom",    0x00000, 0x8000, 0x8f3c763a )  /* sprites */
	ROM_LOAD( "b_01j.rom",    0x08000, 0x8000, 0x7cc90a1d )
	ROM_LOAD( "b_03h.rom",    0x10000, 0x8000, 0x7d58f532 )
	ROM_LOAD( "b_01h.rom",    0x18000, 0x8000, 0x3e396eda )
ROM_END

ROM_START( lwingsjp )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + 3*16k for the banked ROMs images */
	ROM_LOAD( "a_06c.rom",    0x00000, 0x8000, 0x2068a738 )
	ROM_LOAD( "a_07c.rom",    0x10000, 0x8000, 0xd6a2edc4 )
	ROM_LOAD( "9c_lw03.bin",  0x18000, 0x8000, 0xec5cc201 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "11e_lw04.bin", 0x0000, 0x8000, 0xa20337a2 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "9h_lw05.bin",  0x00000, 0x4000, 0x091d923c )  /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b_03e.rom",    0x00000, 0x8000, 0x176e3027 )  /* tiles */
	ROM_LOAD( "b_01e.rom",    0x08000, 0x8000, 0xf5d25623 )
	ROM_LOAD( "b_03d.rom",    0x10000, 0x8000, 0x001caa35 )
	ROM_LOAD( "b_01d.rom",    0x18000, 0x8000, 0x0ba008c3 )
	ROM_LOAD( "b_03b.rom",    0x20000, 0x8000, 0x4f8182e9 )
	ROM_LOAD( "b_01b.rom",    0x28000, 0x8000, 0xf1617374 )
	ROM_LOAD( "b_03f.rom",    0x30000, 0x8000, 0x9b374dcc )
	ROM_LOAD( "b_01f.rom",    0x38000, 0x8000, 0x23654e0a )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b_03j.rom",    0x00000, 0x8000, 0x8f3c763a )  /* sprites */
	ROM_LOAD( "b_01j.rom",    0x08000, 0x8000, 0x7cc90a1d )
	ROM_LOAD( "b_03h.rom",    0x10000, 0x8000, 0x7d58f532 )
	ROM_LOAD( "b_01h.rom",    0x18000, 0x8000, 0x3e396eda )
ROM_END



/***************************************************************

 Section Z
 =========

   Exactly the same hardware as legendary wings, apart from the
   graphics orientation.

***************************************************************/


ROM_START( sectionz )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + 3*16k for the banked ROMs images */
	ROM_LOAD( "6c_sz01.bin",  0x00000, 0x8000, 0x69585125 )
	ROM_LOAD( "7c_sz02.bin",  0x10000, 0x8000, 0x22f161b8 )
	ROM_LOAD( "9c_sz03.bin",  0x18000, 0x8000, 0x4c7111ed )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "11e_sz04.bin", 0x0000, 0x8000, 0xa6073566 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "9h_sz05.bin",  0x00000, 0x4000, 0x3173ba2e )  /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "3e_sz14.bin",  0x00000, 0x8000, 0x63782e30 )  /* tiles */
	ROM_LOAD( "1e_sz08.bin",  0x08000, 0x8000, 0xd57d9f13 )
	ROM_LOAD( "3d_sz13.bin",  0x10000, 0x8000, 0x1b3d4d7f )
	ROM_LOAD( "1d_sz07.bin",  0x18000, 0x8000, 0xf5b3a29f )
	ROM_LOAD( "3b_sz12.bin",  0x20000, 0x8000, 0x11d47dfd )
	ROM_LOAD( "1b_sz06.bin",  0x28000, 0x8000, 0xdf703b68 )
	ROM_LOAD( "3f_sz15.bin",  0x30000, 0x8000, 0x36bb9bf7 )
	ROM_LOAD( "1f_sz09.bin",  0x38000, 0x8000, 0xda8f06c9 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "3j_sz17.bin",  0x00000, 0x8000, 0x8df7b24a )  /* sprites */
	ROM_LOAD( "1j_sz11.bin",  0x08000, 0x8000, 0x685d4c54 )
	ROM_LOAD( "3h_sz16.bin",  0x10000, 0x8000, 0x500ff2bb )
	ROM_LOAD( "1h_sz10.bin",  0x18000, 0x8000, 0x00b3d244 )
ROM_END

ROM_START( sctionza )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + 3*16k for the banked ROMs images */
	ROM_LOAD( "sz-01a.bin",   0x00000, 0x8000, 0x98df49fd )
	ROM_LOAD( "7c_sz02.bin",  0x10000, 0x8000, 0x22f161b8 )
	ROM_LOAD( "sz-03j.bin",   0x18000, 0x8000, 0x94547abf )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "11e_sz04.bin", 0x0000, 0x8000, 0xa6073566 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "9h_sz05.bin",  0x00000, 0x4000, 0x3173ba2e )  /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "3e_sz14.bin",  0x00000, 0x8000, 0x63782e30 )  /* tiles */
	ROM_LOAD( "1e_sz08.bin",  0x08000, 0x8000, 0xd57d9f13 )
	ROM_LOAD( "3d_sz13.bin",  0x10000, 0x8000, 0x1b3d4d7f )
	ROM_LOAD( "1d_sz07.bin",  0x18000, 0x8000, 0xf5b3a29f )
	ROM_LOAD( "3b_sz12.bin",  0x20000, 0x8000, 0x11d47dfd )
	ROM_LOAD( "1b_sz06.bin",  0x28000, 0x8000, 0xdf703b68 )
	ROM_LOAD( "3f_sz15.bin",  0x30000, 0x8000, 0x36bb9bf7 )
	ROM_LOAD( "1f_sz09.bin",  0x38000, 0x8000, 0xda8f06c9 )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "3j_sz17.bin",  0x00000, 0x8000, 0x8df7b24a )  /* sprites */
	ROM_LOAD( "1j_sz11.bin",  0x08000, 0x8000, 0x685d4c54 )
	ROM_LOAD( "3h_sz16.bin",  0x10000, 0x8000, 0x500ff2bb )
	ROM_LOAD( "1h_sz10.bin",  0x18000, 0x8000, 0x00b3d244 )
ROM_END



/***************************************************************

 Trojan
 ======

   Similar to Legendary Wings apart from:
   1) More sprites
   2) 3rd Z80 (ADPCM)
   3) Different palette layout
   4) Third Background tile layer

***************************************************************/



/*
ADPCM is driven by Z80 continuously outputting to a port. The following
table is lifted from the code.

Sample 5 doesn't play properly.
*/

struct ADPCMsample trojan_samples[] =
{
	{ 0x00, 0x00a7, (0x0aa9-0x00a7)*2 },
	{ 0x01, 0x0aa9, (0x12ab-0x0aa9)*2 },
	{ 0x02, 0x12ab, (0x17ad-0x12ab)*2 },
	{ 0x03, 0x17ad, (0x22af-0x17ad)*2 },
	{ 0x04, 0x22af, (0x2db1-0x22af)*2 },
	{ 0x05, 0x2db1, (0x310a-0x2db1)*2 },
	{ 0x06, 0x310a, (0x3cb3-0x310a)*2 }
};

static void trojan_adpcm_init(const struct ADPCMinterface *adpcm_intf, struct ADPCMsample *sample_list, int max)
{
	memcpy(sample_list,trojan_samples,sizeof(trojan_samples));
}

static struct ADPCMinterface trojan_adpcm_interface =
{
	1,                      /* 1 channel */
	4000,                   /* 4000Hz playback */
	REGION_CPU3,            /* memory region */
	trojan_adpcm_init,		/* init function */
	{ 255 }
};

static struct MachineDriver machine_driver_trojan =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,        /* 4 Mhz (?) */
			readmem,trojan_writemem,0,0,
			lwings_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,        /* 3 Mhz (?) */
			sound_readmem,sound_writemem,0,0,
			interrupt,4
		}
	},
	60, 2500,       /* frames per second, vblank duration */
				/* hand tuned to get rid of sprite lag */
	1,      /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo_trojan,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_AFTER_VBLANK,
	0,
	trojan_vh_start,
	trojan_vh_stop,
	trojan_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface,
		},
		{
			SOUND_ADPCM,
			&trojan_adpcm_interface
		}
	}
};

ROM_START( trojan )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + 3*16k for the banked ROMs images */
	ROM_LOAD( "t4",           0x00000, 0x8000, 0xc1bbeb4e )
	ROM_LOAD( "t6",           0x10000, 0x8000, 0xd49592ef )
	ROM_LOAD( "tb05.bin",     0x18000, 0x8000, 0x9273b264 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "tb02.bin",     0x0000, 0x8000, 0x21154797 )

	ROM_REGION( 0x10000, REGION_CPU3 )     /* 64k for ADPCM CPU (CPU not emulated) */
	ROM_LOAD( "tb01.bin",     0x0000, 0x4000, 0x1c0f91b2 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tb03.bin",     0x00000, 0x4000, 0x581a2b4c )     /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tb13.bin",     0x00000, 0x8000, 0x285a052b )     /* tiles */
	ROM_LOAD( "tb09.bin",     0x08000, 0x8000, 0xaeb693f7 )
	ROM_LOAD( "tb12.bin",     0x10000, 0x8000, 0xdfb0fe5c )
	ROM_LOAD( "tb08.bin",     0x18000, 0x8000, 0xd3a4c9d1 )
	ROM_LOAD( "tb11.bin",     0x20000, 0x8000, 0x00f0f4fd )
	ROM_LOAD( "tb07.bin",     0x28000, 0x8000, 0xdff2ee02 )
	ROM_LOAD( "tb14.bin",     0x30000, 0x8000, 0x14bfac18 )
	ROM_LOAD( "tb10.bin",     0x38000, 0x8000, 0x71ba8a6d )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tb18.bin",     0x00000, 0x8000, 0x862c4713 )     /* sprites */
	ROM_LOAD( "tb16.bin",     0x08000, 0x8000, 0xd86f8cbd )
	ROM_LOAD( "tb17.bin",     0x10000, 0x8000, 0x12a73b3f )
	ROM_LOAD( "tb15.bin",     0x18000, 0x8000, 0xbb1a2769 )
	ROM_LOAD( "tb22.bin",     0x20000, 0x8000, 0x39daafd4 )
	ROM_LOAD( "tb20.bin",     0x28000, 0x8000, 0x94615d2a )
	ROM_LOAD( "tb21.bin",     0x30000, 0x8000, 0x66c642bd )
	ROM_LOAD( "tb19.bin",     0x38000, 0x8000, 0x81d5ab36 )

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tb25.bin",     0x00000, 0x8000, 0x6e38c6fa )     /* Bk Tiles */
	ROM_LOAD( "tb24.bin",     0x08000, 0x8000, 0x14fc6cf2 )

	ROM_REGION( 0x08000, REGION_GFX5 )
	ROM_LOAD( "tb23.bin",     0x00000, 0x08000, 0xeda13c0e )  /* Tile Map */
ROM_END

ROM_START( trojanr )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + 3*16k for the banked ROMs images */
	ROM_LOAD( "tb04.bin",     0x00000, 0x8000, 0x92670f27 )
	ROM_LOAD( "tb06.bin",     0x10000, 0x8000, 0xa4951173 )
	ROM_LOAD( "tb05.bin",     0x18000, 0x8000, 0x9273b264 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "tb02.bin",     0x0000, 0x8000, 0x21154797 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* 64k for ADPCM CPU (CPU not emulated) */
	ROM_LOAD( "tb01.bin",     0x0000, 0x4000, 0x1c0f91b2 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tb03.bin",     0x00000, 0x4000, 0x581a2b4c )     /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tb13.bin",     0x00000, 0x8000, 0x285a052b )     /* tiles */
	ROM_LOAD( "tb09.bin",     0x08000, 0x8000, 0xaeb693f7 )
	ROM_LOAD( "tb12.bin",     0x10000, 0x8000, 0xdfb0fe5c )
	ROM_LOAD( "tb08.bin",     0x18000, 0x8000, 0xd3a4c9d1 )
	ROM_LOAD( "tb11.bin",     0x20000, 0x8000, 0x00f0f4fd )
	ROM_LOAD( "tb07.bin",     0x28000, 0x8000, 0xdff2ee02 )
	ROM_LOAD( "tb14.bin",     0x30000, 0x8000, 0x14bfac18 )
	ROM_LOAD( "tb10.bin",     0x38000, 0x8000, 0x71ba8a6d )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tb18.bin",     0x00000, 0x8000, 0x862c4713 )     /* sprites */
	ROM_LOAD( "tb16.bin",     0x08000, 0x8000, 0xd86f8cbd )
	ROM_LOAD( "tb17.bin",     0x10000, 0x8000, 0x12a73b3f )
	ROM_LOAD( "tb15.bin",     0x18000, 0x8000, 0xbb1a2769 )
	ROM_LOAD( "tb22.bin",     0x20000, 0x8000, 0x39daafd4 )
	ROM_LOAD( "tb20.bin",     0x28000, 0x8000, 0x94615d2a )
	ROM_LOAD( "tb21.bin",     0x30000, 0x8000, 0x66c642bd )
	ROM_LOAD( "tb19.bin",     0x38000, 0x8000, 0x81d5ab36 )

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tb25.bin",     0x00000, 0x8000, 0x6e38c6fa )     /* Bk Tiles */
	ROM_LOAD( "tb24.bin",     0x08000, 0x8000, 0x14fc6cf2 )

	ROM_REGION( 0x08000, REGION_GFX5 )
	ROM_LOAD( "tb23.bin",     0x00000, 0x08000, 0xeda13c0e )  /* Tile Map */
ROM_END

ROM_START( trojanj )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + 3*16k for the banked ROMs images */
	ROM_LOAD( "troj-04.rom",  0x00000, 0x8000, 0x0b5a7f49 )
	ROM_LOAD( "troj-06.rom",  0x10000, 0x8000, 0xdee6ed92 )
	ROM_LOAD( "tb05.bin",     0x18000, 0x8000, 0x9273b264 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "tb02.bin",     0x0000, 0x8000, 0x21154797 )

	ROM_REGION( 0x10000, REGION_CPU3 )     /* 64k for ADPCM CPU (CPU not emulated) */
	ROM_LOAD( "tb01.bin",     0x0000, 0x4000, 0x1c0f91b2 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tb03.bin",     0x00000, 0x4000, 0x581a2b4c )     /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tb13.bin",     0x00000, 0x8000, 0x285a052b )     /* tiles */
	ROM_LOAD( "tb09.bin",     0x08000, 0x8000, 0xaeb693f7 )
	ROM_LOAD( "tb12.bin",     0x10000, 0x8000, 0xdfb0fe5c )
	ROM_LOAD( "tb08.bin",     0x18000, 0x8000, 0xd3a4c9d1 )
	ROM_LOAD( "tb11.bin",     0x20000, 0x8000, 0x00f0f4fd )
	ROM_LOAD( "tb07.bin",     0x28000, 0x8000, 0xdff2ee02 )
	ROM_LOAD( "tb14.bin",     0x30000, 0x8000, 0x14bfac18 )
	ROM_LOAD( "tb10.bin",     0x38000, 0x8000, 0x71ba8a6d )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tb18.bin",     0x00000, 0x8000, 0x862c4713 )     /* sprites */
	ROM_LOAD( "tb16.bin",     0x08000, 0x8000, 0xd86f8cbd )
	ROM_LOAD( "tb17.bin",     0x10000, 0x8000, 0x12a73b3f )
	ROM_LOAD( "tb15.bin",     0x18000, 0x8000, 0xbb1a2769 )
	ROM_LOAD( "tb22.bin",     0x20000, 0x8000, 0x39daafd4 )
	ROM_LOAD( "tb20.bin",     0x28000, 0x8000, 0x94615d2a )
	ROM_LOAD( "tb21.bin",     0x30000, 0x8000, 0x66c642bd )
	ROM_LOAD( "tb19.bin",     0x38000, 0x8000, 0x81d5ab36 )

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tb25.bin",     0x00000, 0x8000, 0x6e38c6fa )     /* Bk Tiles */
	ROM_LOAD( "tb24.bin",     0x08000, 0x8000, 0x14fc6cf2 )

	ROM_REGION( 0x08000, REGION_GFX5 )
	ROM_LOAD( "tb23.bin",     0x00000, 0x08000, 0xeda13c0e )  /* Tile Map */
ROM_END

/***************************************************************************

 Avengers - Doesn't work properly due to copy protection

 ***************************************************************************/

extern int avengers_interrupt(void);

WRITE_HANDLER( avengers_protection_w ){
}

READ_HANDLER( avengers_protection_r ){
	/* the protection reads are used for background palette among other things */
	static int hack;
	hack = hack&0xf;
	return hack++;
}


/*
E2 00 E4 03 E6 0C E8 10 EA 19 EC 25 EE 38 F0 3B F2 3E F4 49 F4 s
*/
struct ADPCMsample avengers_samples[] =
{
	{ 0x00, 0x00e2, (0x03e4 -0x00e2)*2 },
	{ 0x01, 0x03e4, (0x0ce6 -0x03e4)*2 },
	{ 0x02, 0x0ce6, (0x10e8 -0x0ce6)*2 },
	{ 0x03, 0x10e8, (0x19ea -0x10e8)*2 },
	{ 0x04, 0x19ea, (0x25ec -0x19ea)*2 },
	{ 0x05, 0x25ec, (0x38ee -0x25ec)*2 },
	{ 0x06, 0x38ee, (0x3bf0 -0x38ee)*2 },
	{ 0x07, 0x3bf0, (0x3ef2 -0x3bf0)*2 },
	{ 0x08, 0x3ef2, (0x49f4 -0x3ef2)*2 }
};

static void avengers_adpcm_init(const struct ADPCMinterface *adpcm_intf, struct ADPCMsample *sample_list, int max)
{
	memcpy(sample_list,avengers_samples,sizeof(avengers_samples));
}

static struct ADPCMinterface avengers_adpcm_interface =
{
	1,                      /* 1 channel */
	4000,                   /* 4000Hz playback */
	REGION_CPU3,            /* memory region */
	avengers_adpcm_init,	/* init function */
	{ 255 }
};

/*
machine driver is exactly the same as trojan apart from
a new custom interrupt handler and slightly different videohardware
*/

static struct MachineDriver machine_driver_avengers =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,        /* 4 Mhz (?) */
			readmem,trojan_writemem,0,0,
			avengers_interrupt,2
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3000000,        /* 3 Mhz (?) */
			sound_readmem,sound_writemem,0,0,
			interrupt,4
		}
	},
	60, 2500,       /* frames per second, vblank duration */
				/* hand tuned to get rid of sprite lag */
	1,      /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo_trojan,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_AFTER_VBLANK,
	0,
	avengers_vh_start,
	trojan_vh_stop,
	trojan_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface,
		},
		{
			SOUND_ADPCM,
			&avengers_adpcm_interface
		}
	}
};

INPUT_PORTS_START( avengers )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )    /* probably unused */

	PORT_START      /* DSWB */
	PORT_DIPNAME( 0x01, 0x01, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x30, 0x30, "Bonus" )
	PORT_DIPSETTING(    0x30, "20k 60k" )
	PORT_DIPSETTING(    0x10, "20k 70k" )
	PORT_DIPSETTING(    0x20, "20k 80k" )
	PORT_DIPSETTING(    0x00, "30k 80k" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x80, "5" )
	PORT_DIPSETTING(    0x00, "6" )

	PORT_START      /* DSWA */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_6C ) )
INPUT_PORTS_END

ROM_START( avengers )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + 3*16k for the banked ROMs images */
	ROM_LOAD( "04.10n",       0x00000, 0x8000, 0xa94aadcc )
	ROM_LOAD( "06.13n",       0x10000, 0x8000, 0x39cd80bd )
	ROM_LOAD( "05.12n",       0x18000, 0x8000, 0x06b1cec9 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "02.15h",       0x0000, 0x8000, 0x107a2e17 ) /* ?? */

	ROM_REGION( 0x10000, REGION_CPU3 )     /* ADPCM CPU (not emulated) */
	ROM_LOAD( "01.6d",        0x0000, 0x8000, 0xc1e5d258 ) /* adpcm player - "Talker" ROM */

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "03.8k",        0x00000, 0x4000, 0x4a297a5c )  /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "13.6b",        0x00000, 0x8000, 0x9b5ff305 ) /* plane 1 */
	ROM_LOAD( "09.6a",        0x08000, 0x8000, 0x08323355 )
	ROM_LOAD( "12.4b",        0x10000, 0x8000, 0x6d5261ba ) /* plane 2 */
	ROM_LOAD( "08.4a",        0x18000, 0x8000, 0xa13d9f54 )
	ROM_LOAD( "11.3b",        0x20000, 0x8000, 0xa2911d8b ) /* plane 3 */
	ROM_LOAD( "07.3a",        0x28000, 0x8000, 0xcde78d32 )
	ROM_LOAD( "14.8b",        0x30000, 0x8000, 0x44ac2671 ) /* plane 4 */
	ROM_LOAD( "10.8a",        0x38000, 0x8000, 0xb1a717cb )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE ) /* sprites */
	ROM_LOAD( "18.7l",        0x00000, 0x8000, 0x3c876a17 ) /* planes 0,1 */
	ROM_LOAD( "16.3l",        0x08000, 0x8000, 0x4b1ff3ac )
	ROM_LOAD( "17.5l",        0x10000, 0x8000, 0x4eb543ef )
	ROM_LOAD( "15.2l",        0x18000, 0x8000, 0x8041de7f )
	ROM_LOAD( "22.7n",        0x20000, 0x8000, 0xbdaa8b22 ) /* planes 2,3 */
	ROM_LOAD( "20.3n",        0x28000, 0x8000, 0x566e3059 )
	ROM_LOAD( "21.5n",        0x30000, 0x8000, 0x301059aa )
	ROM_LOAD( "19.2n",        0x38000, 0x8000, 0xa00485ec )

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE ) /* bg tiles */
	ROM_LOAD( "25.15n",       0x00000, 0x8000, 0x230d9e30 ) /* planes 0,1 */
	ROM_LOAD( "24.13n",       0x08000, 0x8000, 0xa6354024 ) /* planes 2,3 */

	ROM_REGION( 0x08000, REGION_GFX5 )
	ROM_LOAD( "23.9n",        0x0000, 0x08000, 0xc0a93ef6 )  /* Tile Map */
ROM_END

ROM_START( avenger2 )
	ROM_REGION( 0x20000, REGION_CPU1 )     /* 64k for code + 3*16k for the banked ROMs images */
	ROM_LOAD( "avg4.bin",     0x00000, 0x8000, 0x0fea7ac5 )
	ROM_LOAD( "avg6.bin",     0x10000, 0x8000, 0x491a712c )
	ROM_LOAD( "avg5.bin",     0x18000, 0x8000, 0x9a214b42 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "02.15h",       0x0000, 0x8000, 0x107a2e17 ) /* MISSING from this set */

	ROM_REGION( 0x10000, REGION_CPU3 )     /* ADPCM CPU (not emulated) */
	ROM_LOAD( "01.6d",        0x0000, 0x8000, 0xc1e5d258 ) /* adpcm player - "Talker" ROM */

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "03.8k",        0x00000, 0x4000, 0x4a297a5c )  /* characters */

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE ) /* tiles */
	ROM_LOAD( "13.6b",        0x00000, 0x8000, 0x9b5ff305 ) /* plane 1 */
	ROM_LOAD( "09.6a",        0x08000, 0x8000, 0x08323355 )
	ROM_LOAD( "12.4b",        0x10000, 0x8000, 0x6d5261ba ) /* plane 2 */
	ROM_LOAD( "08.4a",        0x18000, 0x8000, 0xa13d9f54 )
	ROM_LOAD( "11.3b",        0x20000, 0x8000, 0xa2911d8b ) /* plane 3 */
	ROM_LOAD( "07.3a",        0x28000, 0x8000, 0xcde78d32 )
	ROM_LOAD( "14.8b",        0x30000, 0x8000, 0x44ac2671 ) /* plane 4 */
	ROM_LOAD( "10.8a",        0x38000, 0x8000, 0xb1a717cb )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE ) /* sprites */
	ROM_LOAD( "18.7l",        0x00000, 0x8000, 0x3c876a17 ) /* planes 0,1 */
	ROM_LOAD( "16.3l",        0x08000, 0x8000, 0x4b1ff3ac )
	ROM_LOAD( "17.5l",        0x10000, 0x8000, 0x4eb543ef )
	ROM_LOAD( "15.2l",        0x18000, 0x8000, 0x8041de7f )
	ROM_LOAD( "22.7n",        0x20000, 0x8000, 0xbdaa8b22 ) /* planes 2,3 */
	ROM_LOAD( "20.3n",        0x28000, 0x8000, 0x566e3059 )
	ROM_LOAD( "21.5n",        0x30000, 0x8000, 0x301059aa )
	ROM_LOAD( "19.2n",        0x38000, 0x8000, 0xa00485ec )

	ROM_REGION( 0x10000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "25.15n",       0x00000, 0x8000, 0x230d9e30 ) /* planes 0,1 */
	ROM_LOAD( "24.13n",       0x08000, 0x8000, 0xa6354024 ) /* planes 2,3 */

	ROM_REGION( 0x08000, REGION_GFX5 )
	ROM_LOAD( "23.9n",        0x0000, 0x08000, 0xc0a93ef6 )  /* Tile Map */
ROM_END


GAME( 1986, lwings,   0,        lwings,   lwings,   0, ROT270, "Capcom", "Legendary Wings (US set 1)" )
GAME( 1986, lwings2,  lwings,   lwings,   lwings,   0, ROT270, "Capcom", "Legendary Wings (US set 2)" )
GAME( 1986, lwingsjp, lwings,   lwings,   lwings,   0, ROT270, "Capcom", "Ales no Tsubasa (Japan)" )
GAME( 1985, sectionz, 0,        lwings,   sectionz, 0, ROT0,   "Capcom", "Section Z (set 1)" )
GAME( 1985, sctionza, sectionz, lwings,   sectionz, 0, ROT0,   "Capcom", "Section Z (set 2)" )
GAME( 1986, trojan,   0,        trojan,   trojanls, 0, ROT0,   "Capcom", "Trojan (US)" )
GAME( 1986, trojanr,  trojan,   trojan,   trojan,   0, ROT0,   "Capcom (Romstar license)", "Trojan (Romstar)" )
GAME( 1986, trojanj,  trojan,   trojan,   trojan,   0, ROT0,   "Capcom", "Tatakai no Banka (Japan)" )
GAMEX(1987, avengers, 0,        avengers, avengers, 0, ROT270, "Capcom", "Avengers (set 1)", GAME_NOT_WORKING | GAME_WRONG_COLORS )
GAMEX(1987, avenger2, avengers, avengers, avengers, 0, ROT270, "Capcom", "Avengers (set 2)", GAME_NOT_WORKING | GAME_WRONG_COLORS )
