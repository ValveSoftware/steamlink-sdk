#include "../vidhrdw/tsamurai.cpp"

/****************************************************************************

	Preliminary driver for Samurai, Nunchackun, Yuke Yuke Yamaguchi-kun
	(c) Taito 1985

	Known Issues:
	- some color problems (need screenshots)
	- Nunchackun has wrong colors; sprites look better if you subtract sprite color from 0x2d
	- Yuke Yuke Yamaguchi-kun isn't playable (sprite problem only?)

driver by Phil Stroffolino

****************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"
#include "vidhrdw/generic.h"

WRITE_HANDLER( tsamurai_bgcolor_w );
WRITE_HANDLER( tsamurai_textbank_w );

WRITE_HANDLER( tsamurai_scrolly_w );
WRITE_HANDLER( tsamurai_scrollx_w );
extern void tsamurai_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh );
extern void tsamurai_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( tsamurai_bg_videoram_w );
WRITE_HANDLER( tsamurai_fg_videoram_w );
WRITE_HANDLER( tsamurai_fg_colorram_w );
extern int tsamurai_vh_start( void );
extern unsigned char *tsamurai_videoram;

static struct AY8910interface ay8910_interface =
{
	1, /* number of chips */
	2000000, /* 2 MHz */
	{ 10 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct DACinterface dac_interface =
{
	2,			/* number of chips */
	{ 20, 20 }
};

static int nmi_enabled;
static int sound_command1, sound_command2;

static WRITE_HANDLER( nmi_enable_w ){
	nmi_enabled = data;
}

static int samurai_interrupt( void ){
	return nmi_enabled? nmi_interrupt():ignore_interrupt();
}

READ_HANDLER( unknown_d803_r )
{
	return 0x6b;     // nogi
}

READ_HANDLER( unknown_d806_r )
{
	return 0x40;
}

READ_HANDLER( unknown_d900_r )
{
	return 0x6a;
}

READ_HANDLER( unknown_d938_r )
{
	return 0xfb;    // nogi
}


static WRITE_HANDLER( sound_command1_w )
{
	sound_command1 = data;
	cpu_cause_interrupt( 1, Z80_IRQ_INT );
}

static WRITE_HANDLER( sound_command2_w )
{
	sound_command2 = data;
	cpu_cause_interrupt( 2, Z80_IRQ_INT );
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xcfff, MRA_RAM },

	/* protection? */
	{ 0xd803, 0xd803, unknown_d803_r },
	{ 0xd806, 0xd806, unknown_d806_r },
	{ 0xd900, 0xd900, unknown_d900_r },
	{ 0xd938, 0xd938, unknown_d938_r },

	{ 0xe000, 0xe3ff, MRA_RAM },
	{ 0xe400, 0xe7ff, MRA_RAM },
	{ 0xe800, 0xefff, MRA_RAM },
	{ 0xf000, 0xf3ff, MRA_RAM },

	{ 0xf800, 0xf800, input_port_0_r },
	{ 0xf801, 0xf801, input_port_1_r },
	{ 0xf802, 0xf802, input_port_2_r },
	{ 0xf804, 0xf804, input_port_3_r },
	{ 0xf805, 0xf805, input_port_4_r },
	{ -1 }
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xcfff, MWA_RAM },

	{ 0xe000, 0xe3ff, tsamurai_fg_videoram_w, &videoram },
	{ 0xe400, 0xe43f, tsamurai_fg_colorram_w, &colorram },    // nogi
	{ 0xe440, 0xe7ff, MWA_RAM },
	{ 0xe800, 0xefff, tsamurai_bg_videoram_w, &tsamurai_videoram },
	{ 0xf000, 0xf3ff, MWA_RAM, &spriteram },

	{ 0xf400, 0xf400, MWA_NOP },
	{ 0xf401, 0xf401, sound_command1_w },
	{ 0xf402, 0xf402, sound_command2_w },

	{ 0xf801, 0xf801, tsamurai_bgcolor_w },
	{ 0xf802, 0xf802, tsamurai_scrolly_w },
	{ 0xf803, 0xf803, tsamurai_scrollx_w },

	{ 0xfc00, 0xfc00, flip_screen_w },
	{ 0xfc01, 0xfc01, nmi_enable_w },
	{ 0xfc02, 0xfc02, tsamurai_textbank_w },
	{ 0xfc03, 0xfc04, coin_counter_w },

	{ -1 }
};

static struct IOReadPort z80_readport[] =
{
	{ -1 }
};

static struct IOWritePort z80_writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ -1 }
};


/*******************************************************************************/

static READ_HANDLER( sound_command1_r )
{
	return sound_command1;
}

static WRITE_HANDLER( sound_out1_w )
{
	DAC_data_w(0,data);
}

static struct MemoryReadAddress readmem_sound1[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x6000, 0x6000, sound_command1_r },
	{ 0x7f00, 0x7fff, MRA_RAM },
	{ -1 }
};

static struct MemoryWriteAddress writemem_sound1[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x6001, 0x6001, MWA_NOP }, /* ? */
	{ 0x6002, 0x6002, sound_out1_w },
	{ 0x7f00, 0x7fff, MWA_RAM },
	{ -1 }
};
/*******************************************************************************/
static READ_HANDLER( sound_command2_r ){
	return sound_command2;
}

static WRITE_HANDLER( sound_out2_w ){
	DAC_data_w(1,data);
}

static struct MemoryReadAddress readmem_sound2[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x6000, 0x6000, sound_command2_r },
	{ 0x7f00, 0x7fff, MRA_RAM },
	{ -1 }
};

static struct MemoryWriteAddress writemem_sound2[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x6001, 0x6001, MWA_NOP }, /* ? */
	{ 0x6002, 0x6002, sound_out2_w },
	{ 0x7f00, 0x7fff, MWA_RAM },
	{ -1 }
};

/*******************************************************************************/

INPUT_PORTS_START( tsamurai )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START /* DSW1 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START /* DSW2 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x02, "7" )
	PORT_BITX(0,        0x03, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "254", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, "DSW2 Unknown 1" )
	PORT_DIPSETTING(    0x00, "00" )
	PORT_DIPSETTING(    0x04, "30" )
	PORT_DIPSETTING(    0x08, "50" )
	PORT_DIPSETTING(    0x0c, "70" )
	PORT_DIPNAME( 0x30, 0x30, "DSW2 Unknown 2" )
	PORT_DIPSETTING(    0x00, "0x00" )
	PORT_DIPSETTING(    0x10, "0x01" )
	PORT_DIPSETTING(    0x20, "0x02" )
	PORT_DIPSETTING(    0x30, "0x03" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DSW2 Unknown 3" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END


INPUT_PORTS_START( nunchaku )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START /* DSW1 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START /* DSW2 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x02, "7" )
	PORT_BITX(0,        0x03, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "255", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, "DSW2 Unknown 1" )
	PORT_DIPSETTING(    0x00, "00" )
	PORT_DIPSETTING(    0x04, "30" )
	PORT_DIPSETTING(    0x08, "50" )
	PORT_DIPSETTING(    0x0c, "70" )
	PORT_DIPNAME( 0x30, 0x30, "DSW2 Unknown 2" )
	PORT_DIPSETTING(    0x00, "0x00" )
	PORT_DIPSETTING(    0x10, "0x01" )
	PORT_DIPSETTING(    0x20, "0x02" )
	PORT_DIPSETTING(    0x30, "0x03" )
	PORT_DIPNAME( 0x40, 0x40, "DSW2 Unknown 3" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DSW2 Unknown 4" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( yamagchi )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START /* DSW1 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x00, "Freeze" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START /* DSW2 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x02, "7" )
	PORT_BITX(0,        0x03, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "255", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, "DSW2 Unknown 1" )
	PORT_DIPSETTING(    0x00, "00" )
	PORT_DIPSETTING(    0x04, "30" )
	PORT_DIPSETTING(    0x08, "50" )
	PORT_DIPSETTING(    0x0c, "70" )
	PORT_DIPNAME( 0x10, 0x10, "Language" )
	PORT_DIPSETTING(    0x10, "English" )
	PORT_DIPSETTING(    0x00, "Japanese" )
	PORT_DIPNAME( 0x20, 0x20, "DSW2 Unknown 2" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DSW2 Unknown 3" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout char_layout =
{
	8,8,
	0x200,
	3,
	{ 2*0x1000*8, 1*0x1000*8, 0*0x1000*8 },
	{ 0,1,2,3, 4,5,6,7 },
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8 },
	8*8
};

static struct GfxLayout sprite_layout =
{
	32,32,
	0x80,
	3,
	{ 2*0x4000*8, 1*0x4000*8, 0*0x4000*8 },
	{
		0,1,2,3,4,5,6,7,
		64+0,64+1,64+2,64+3,64+4,64+5,64+6,64+7,
		128+0,128+1,128+2,128+3,128+4,128+5,128+6,128+7,
		64*3+0,64*3+1,64*3+2,64*3+3,64*3+4,64*3+5,64*3+6,64*3+7
	},
	{
		0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8,
		1*256+0*8,1*256+1*8,1*256+2*8,1*256+3*8,1*256+4*8,1*256+5*8,1*256+6*8,1*256+7*8,
		2*256+0*8,2*256+1*8,2*256+2*8,2*256+3*8,2*256+4*8,2*256+5*8,2*256+6*8,2*256+7*8,
		3*256+0*8,3*256+1*8,3*256+2*8,3*256+3*8,3*256+4*8,3*256+5*8,3*256+6*8,3*256+7*8
	},
	4*256
};

static struct GfxLayout tile_layout =
{
	8,8,
	0x400,
	3,
	{ 2*0x2000*8, 1*0x2000*8, 0*0x2000*8 },
	{ 0,1,2,3,4,5,6,7 },
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8 },
	8*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tile_layout,   0, 32 },
	{ REGION_GFX2, 0, &char_layout,   0, 32 },
	{ REGION_GFX3, 0, &sprite_layout, 0, 32 },
	{ -1 }
};

static struct MachineDriver machine_driver_tsamurai =
{
	{
		{
			CPU_Z80,
			4000000,
			readmem,writemem,z80_readport, z80_writeport,
			samurai_interrupt,1,
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			2000000,
			readmem_sound1,writemem_sound1,0,0,
			ignore_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			2000000,
			readmem_sound2,writemem_sound2,0,0,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1, /* CPU slices */
	0, /* init machine */

	/* video hardware */
	32*8, 32*8, { 0, 255, 8, 255-8 },
	gfxdecodeinfo,
	256,256,
	tsamurai_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	tsamurai_vh_start,
	0,
	tsamurai_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};



ROM_START( tsamurai )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* Z80 code  - main CPU */
	ROM_LOAD( "01.3r",      0x0000, 0x4000, 0xd09c8609 )
	ROM_LOAD( "02.3t",      0x4000, 0x4000, 0xd0f2221c )
	ROM_LOAD( "03.3v",      0x8000, 0x4000, 0xeee8b0c9 )

	ROM_REGION(  0x10000 , REGION_CPU2 ) /* Z80 code - sample player#1 */
	ROM_LOAD( "14.4e",      0x0000, 0x2000, 0x220e9c04 )
	ROM_LOAD( "a35-15.4c",  0x2000, 0x2000, 0x1e0d1e33 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* Z80 code - sample player#2 */
	ROM_LOAD( "13.4j",      0x0000, 0x2000, 0x73feb0e2 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a35-04.10a", 0x00000, 0x2000, 0xb97ce9b1 ) // tiles
	ROM_LOAD( "a35-05.10b", 0x02000, 0x2000, 0x55a17b08 )
	ROM_LOAD( "a35-06.10d", 0x04000, 0x2000, 0xf5ee6f8f )

	ROM_REGION( 0x03000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a35-10.11n", 0x00000, 0x1000, 0x0b5a0c45 ) // characters
	ROM_LOAD( "a35-11.11q", 0x01000, 0x1000, 0x93346d75 )
	ROM_LOAD( "a35-12.11r", 0x02000, 0x1000, 0xf4c69d8a )

	ROM_REGION( 0x0c000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a35-07.12h", 0x00000, 0x4000, 0x38fc349f ) // sprites
	ROM_LOAD( "a35-08.12j", 0x04000, 0x4000, 0xa07d6dc3 )
	ROM_LOAD( "a35-09.12k", 0x08000, 0x4000, 0xc0784a0e )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "a35-16.2j",  0x0000, 0x0100, 0x72d8b332 )
	ROM_LOAD( "a35-17.2l",  0x0100, 0x0100, 0x9bf1829e )
	ROM_LOAD( "a35-18.2m",  0x0200, 0x0100, 0x918e4732 )
ROM_END

ROM_START( tsamura2 )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* Z80 code  - main CPU */
	ROM_LOAD( "a35-01.3r",  0x0000, 0x4000, 0x282d96ad )
	ROM_LOAD( "a35-02.3t",  0x4000, 0x4000, 0xe3fa0cfa )
	ROM_LOAD( "a35-03.3v",  0x8000, 0x4000, 0x2fff1e0a )

	ROM_REGION(  0x10000 , REGION_CPU2 ) /* Z80 code - sample player#1 */
	ROM_LOAD( "a35-14.4e",  0x0000, 0x2000, 0xf10aee3b )
	ROM_LOAD( "a35-15.4c",  0x2000, 0x2000, 0x1e0d1e33 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* Z80 code - sample player#2 */
	ROM_LOAD( "a35-13.4j",  0x0000, 0x2000, 0x3828f4d2 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a35-04.10a", 0x00000, 0x2000, 0xb97ce9b1 ) // tiles
	ROM_LOAD( "a35-05.10b", 0x02000, 0x2000, 0x55a17b08 )
	ROM_LOAD( "a35-06.10d", 0x04000, 0x2000, 0xf5ee6f8f )

	ROM_REGION( 0x03000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a35-10.11n", 0x00000, 0x1000, 0x0b5a0c45 ) // characters
	ROM_LOAD( "a35-11.11q", 0x01000, 0x1000, 0x93346d75 )
	ROM_LOAD( "a35-12.11r", 0x02000, 0x1000, 0xf4c69d8a )

	ROM_REGION( 0x0c000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a35-07.12h", 0x00000, 0x4000, 0x38fc349f ) // sprites
	ROM_LOAD( "a35-08.12j", 0x04000, 0x4000, 0xa07d6dc3 )
	ROM_LOAD( "a35-09.12k", 0x08000, 0x4000, 0xc0784a0e )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "a35-16.2j",  0x0000, 0x0100, 0x72d8b332 )
	ROM_LOAD( "a35-17.2l",  0x0100, 0x0100, 0x9bf1829e )
	ROM_LOAD( "a35-18.2m",  0x0200, 0x0100, 0x918e4732 )
ROM_END

ROM_START( nunchaku )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* Z80 code  - main CPU */
	ROM_LOAD( "nunchack.p1", 0x0000, 0x4000, 0x4385aca6 )
	ROM_LOAD( "nunchack.p2", 0x4000, 0x4000, 0xf9beb72c )
	ROM_LOAD( "nunchack.p3", 0x8000, 0x4000, 0xcde5d674 )

	ROM_REGION(  0x10000 , REGION_CPU2 ) /* Z80 code - sample player */
	ROM_LOAD( "nunchack.m3", 0x0000, 0x2000, 0x9036c945 )
	ROM_LOAD( "nunchack.m4", 0x2000, 0x2000, 0xe7206724 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* Z80 code - sample player */
	ROM_LOAD( "nunchack.m1", 0x0000, 0x2000, 0xb53d73f6 )
	ROM_LOAD( "nunchack.m2", 0x2000, 0x2000, 0xf37d7c49 )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nunchack.b1", 0x00000, 0x2000, 0x48c88fea ) // tiles
	ROM_LOAD( "nunchack.b2", 0x02000, 0x2000, 0xeec818e4 )
	ROM_LOAD( "nunchack.b3", 0x04000, 0x2000, 0x5f16473f )

	ROM_REGION( 0x03000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nunchack.v1", 0x00000, 0x1000, 0x358a3714 ) // characters
	ROM_LOAD( "nunchack.v2", 0x01000, 0x1000, 0x54c18d8e )
	ROM_LOAD( "nunchack.v3", 0x02000, 0x1000, 0xf7ac203a )

	ROM_REGION( 0x0c000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nunchack.c1", 0x00000, 0x4000, 0x797cbc8a ) // sprites
	ROM_LOAD( "nunchack.c2", 0x04000, 0x4000, 0x701a0cc3 )
	ROM_LOAD( "nunchack.c3", 0x08000, 0x4000, 0xffb841fc )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "nunchack.016", 0x000, 0x100, 0xa7b077d4 )
	ROM_LOAD( "nunchack.017", 0x100, 0x100, 0x1c04c087 )
	ROM_LOAD( "nunchack.018", 0x200, 0x100, 0xf5ce3c45 )
ROM_END

ROM_START( yamagchi )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* Z80 code  - main CPU */
	ROM_LOAD( "a38-01.3s", 0x0000, 0x4000, 0x1a6c8498 )
	ROM_LOAD( "a38-02.3t", 0x4000, 0x4000, 0xfa66b396 )
	ROM_LOAD( "a38-03.3v", 0x8000, 0x4000, 0x6a4239cf )

	ROM_REGION(  0x10000 , REGION_CPU2 ) /* Z80 code - sample player */
	ROM_LOAD( "a38-14.4e", 0x0000, 0x2000, 0x5a758992 )

	ROM_REGION( 0x10000, REGION_CPU3 ) /* Z80 code - sample player */
	ROM_LOAD( "a38-13.4j", 0x0000, 0x2000, 0xa26445bb )

	ROM_REGION( 0x06000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a38-04.10a", 0x00000, 0x2000, 0x6bc69d4d ) // tiles
	ROM_LOAD( "a38-05.10b", 0x02000, 0x2000, 0x047fb315 )
	ROM_LOAD( "a38-06.10d", 0x04000, 0x2000, 0xa636afb2 )

	ROM_REGION( 0x03000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a38-10.11n", 0x00000, 0x1000, 0x51ab4671 ) // characters
	ROM_LOAD( "a38-11.11p", 0x01000, 0x1000, 0x27890169 )
	ROM_LOAD( "a38-12.11r", 0x02000, 0x1000, 0xc98d5cf2 )

	ROM_REGION( 0x0c000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a38-07.12h", 0x00000, 0x4000, 0xa3a521b6 ) // sprites
	ROM_LOAD( "a38-08.12j", 0x04000, 0x4000, 0x553afc66 )
	ROM_LOAD( "a38-09.12l", 0x08000, 0x4000, 0x574156ae )

	ROM_REGION( 0x0300, REGION_PROMS )
	ROM_LOAD( "mb7114e.2k", 0x000, 0x100, 0xe7648110 )
	ROM_LOAD( "mb7114e.2l", 0x100, 0x100, 0x7b874ee6 )
	ROM_LOAD( "mb7114e.2m", 0x200, 0x100, 0x938d0fce )
ROM_END



GAME( 1985, tsamurai, 0,        tsamurai, tsamurai, 0, ROT90, "Taito", "Samurai Nihon-ichi (set 1)" )
GAME( 1985, tsamura2, tsamurai, tsamurai, tsamurai, 0, ROT90, "Taito", "Samurai Nihon-ichi (set 2)" )
GAMEX(1985, nunchaku, 0,        tsamurai, nunchaku, 0, ROT90, "Taito", "Nunchackun", GAME_WRONG_COLORS )
GAMEX(1985, yamagchi, 0,        tsamurai, yamagchi, 0, ROT90, "Taito", "Go Go Mr. Yamaguchi / Yuke Yuke Yamaguchi-kun", GAME_IMPERFECT_COLORS )
