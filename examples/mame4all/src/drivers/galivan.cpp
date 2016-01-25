#include "../vidhrdw/galivan.cpp"

/***************************************************************************

TODO:
- Find out how layers are enabled\disabled
- sprite/char priority
- dangar input ports
- wrong title screen in ninjemak
- bit 3 of ninjemak_gfxbank_w, there currently is a kludge to clear text RAM
  but it should really copy stuff from the extra ROM.


Galivan
(C) 1985 Nihon Bussan

driver by

Luca Elia (eliavit@unina.it)
Olivier Galibert


Ninja Emaki (US)
(c)1986 NihonBussan Co.,Ltd.

Youma Ninpou Chou (Japan)
(c)1986 NihonBussan Co.,Ltd.

Driver by Takahiro Nogi (nogi@kt.rim.or.jp) 1999/12/17 -

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

WRITE_HANDLER( galivan_scrollx_w );
WRITE_HANDLER( galivan_scrolly_w );
WRITE_HANDLER( galivan_videoram_w );
WRITE_HANDLER( galivan_colorram_w );
WRITE_HANDLER( galivan_gfxbank_w );
void galivan_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int galivan_vh_start(void);
void galivan_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( ninjemak_scrollx_w );
WRITE_HANDLER( ninjemak_scrolly_w );
WRITE_HANDLER( ninjemak_gfxbank_w );
int ninjemak_vh_start(void);
void ninjemak_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);



static void galivan_init_machine(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	cpu_setbank(1,&RAM[0x10000]);
//	layers = 0x60;
}

static WRITE_HANDLER( galivan_sound_command_w )
{
	soundlatch_w(offset,(data << 1) | 1);
}

static READ_HANDLER( galivan_sound_command_r )
{
	int data;

	data = soundlatch_r(offset);
	soundlatch_clear_w(0,0);
	return data;
}

static READ_HANDLER( IO_port_c0_r )
{
  return (0x58); /* To Avoid Reset on Ufo Robot dangar */
}


/* the scroll registers are memory mapped in ninjemak, I/O ports in the others */
static WRITE_HANDLER( ninjemak_videoreg_w )
{
	switch (offset)
	{
		case	0x0b:
			ninjemak_scrolly_w(0, data);
			break;
		case	0x0c:
			ninjemak_scrolly_w(1, data);
			break;
		case	0x0d:
			ninjemak_scrollx_w(0, data);
			break;
		case	0x0e:
			ninjemak_scrollx_w(1, data);
			break;
		default:
			break;
	}
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xdfff, MRA_BANK1 },
	{ 0xe000, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xd800, 0xdbff, galivan_videoram_w, &videoram, &videoram_size },
	{ 0xdc00, 0xdfff, galivan_colorram_w, &colorram },
	{ 0xe000, 0xe0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe100, 0xffff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress ninjemak_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xd800, 0xd81f, ninjemak_videoreg_w },
	{ 0xd800, 0xdbff, galivan_videoram_w, &videoram, &videoram_size },
	{ 0xdc00, 0xdfff, galivan_colorram_w, &colorram },
	{ 0xe000, 0xe1ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe200, 0xffff, MWA_RAM },
	{ -1 }	/* end of table */
};


static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r },
	{ 0x01, 0x01, input_port_1_r },
	{ 0x02, 0x02, input_port_2_r },
	{ 0x03, 0x03, input_port_3_r },
	{ 0x04, 0x04, input_port_4_r },
	{ 0xc0, 0xc0, IO_port_c0_r }, /* dangar needs to return 0x58 */
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x40, 0x40, galivan_gfxbank_w },
	{ 0x41, 0x42, galivan_scrollx_w },
	{ 0x43, 0x44, galivan_scrolly_w },
	{ 0x45, 0x45, galivan_sound_command_w },
/*	{ 0x46, 0x46, IOWP_NOP }, */
/*	{ 0x47, 0x47, IOWP_NOP }, */
	{ -1 }	/* end of table */
};

static struct IOReadPort ninjemak_readport[] =
{
	{ 0x80, 0x80, input_port_0_r },
	{ 0x81, 0x81, input_port_1_r },
	{ 0x82, 0x82, input_port_2_r },
	{ 0x83, 0x83, input_port_3_r },
	{ 0x84, 0x84, input_port_4_r },
	{ 0x85, 0x85, input_port_5_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort ninjemak_writeport[] =
{
	{ 0x80, 0x80, ninjemak_gfxbank_w },
	{ 0x85, 0x85, galivan_sound_command_w },
//	{ 0x86, 0x86, IOWP_NOP },			// ??
//	{ 0x87, 0x87, IOWP_NOP },			// ??
	{ -1 }	/* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ -1 }	/* end of table */
};


static struct IOReadPort sound_readport[] =
{
/*	{ 0x04, 0x04, IORP_NOP },    value read and *discarded*    */
	{ 0x06, 0x06, galivan_sound_command_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x00, 0x00, YM3526_control_port_0_w },
	{ 0x01, 0x01, YM3526_write_port_0_w },
	{ 0x02, 0x02, DAC_0_data_w },
	{ 0x03, 0x03, DAC_1_data_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( galivan )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )

	PORT_START  /* IN2 - TEST, COIN, START */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x20, 0x20, 0, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN3 - DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "20000 and every 60000" )
	PORT_DIPSETTING(    0x04, "20000 and every 90000" )
	PORT_DIPSETTING(    0x08, "50000 and every 60000" )
	PORT_DIPSETTING(    0x00, "50000 and every 90000" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* IN4 - DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/* the last two dip switches are different in each version */
INPUT_PORTS_START( dangar )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )

	PORT_START  /* IN2 - TEST, COIN, START */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x20, 0x20, 0, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN3 - DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "20000 and every 60000" )
	PORT_DIPSETTING(    0x04, "20000 and every 90000" )
	PORT_DIPSETTING(    0x08, "50000 and every 60000" )
	PORT_DIPSETTING(    0x00, "50000 and every 90000" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* IN4 - DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	/* two switches to allow continue... both work */
    	PORT_DIPNAME( 0xc0, 0x00, "Continue" )
    	PORT_DIPSETTING(    0xc0, DEF_STR( No ) )
    	PORT_DIPSETTING(    0x80, "3 Times" )
    	PORT_DIPSETTING(    0x40, "5 Times" )
    	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
INPUT_PORTS_END

/* the last two dip switches are different in each version */
INPUT_PORTS_START( dangar2 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )

	PORT_START  /* IN2 - TEST, COIN, START */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x20, 0x20, 0, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN3 - DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "20000 and every 60000" )
	PORT_DIPSETTING(    0x04, "20000 and every 90000" )
	PORT_DIPSETTING(    0x08, "50000 and every 60000" )
	PORT_DIPSETTING(    0x00, "50000 and every 90000" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* IN4 - DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Complete Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Base Ship Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/* the last two dip switches are different in each version */
INPUT_PORTS_START( dangarb )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )

	PORT_START  /* IN2 - TEST, COIN, START */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BITX(0x20, 0x20, 0, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN3 - DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "20000 and every 60000" )
	PORT_DIPSETTING(    0x04, "20000 and every 90000" )
	PORT_DIPSETTING(    0x08, "50000 and every 60000" )
	PORT_DIPSETTING(    0x00, "50000 and every 90000" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* IN4 - DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Complete Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Base Ship Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( ninjemak )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 - TEST, COIN, START */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN3 - TEST */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x02, IP_ACTIVE_LOW )
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

	PORT_START	/* IN4 - TEST */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "20000 and every 60000" )
	PORT_DIPSETTING(    0x08, "50000 and every 60000" )
	PORT_DIPSETTING(    0x04, "20000 and every 90000" )
	PORT_DIPSETTING(    0x00, "50000 and every 90000" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START	/* IN5 - TEST */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Allow Continue" )
	PORT_DIPSETTING(    0x80, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
INPUT_PORTS_END

INPUT_PORTS_START( youma )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 - TEST, COIN, START */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN3 - TEST */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x02, IP_ACTIVE_LOW )
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

	PORT_START	/* IN4 - TEST */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "20000 and every 60000" )
	PORT_DIPSETTING(    0x08, "50000 and every 60000" )
	PORT_DIPSETTING(    0x04, "20000 and every 90000" )
	PORT_DIPSETTING(    0x00, "50000 and every 90000" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START	/* IN5 - TEST */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x00, "Hard" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END



#define CHARLAYOUT(NUM) static struct GfxLayout charlayout_##NUM =  \
{																	\
	8,8,	/* 8*8 characters */									\
	NUM,	/* NUM characters */									\
	4,	/* 4 bits per pixel */										\
	{ 0, 1, 2, 3 },													\
	{ 1*4, 0*4, 3*4, 2*4, 5*4, 4*4, 7*4, 6*4 },						\
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },				\
	32*8	/* every char takes 32 consecutive bytes */				\
}

CHARLAYOUT(512);
CHARLAYOUT(1024);

static struct GfxLayout tilelayout =
{
	16,16,
	1024,
	4,
	{ 0, 1, 2, 3 },
	{ 4,0,12,8,20,16,28,24,36,32,44,40,52,48,60,56 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
	  8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	16*16*4
};

#define SPRITELAYOUT(NUM) static struct GfxLayout spritelayout_##NUM =  \
{																		\
	16,16,	/* 16*16 sprites */											\
	NUM,	/* NUM sprites */											\
	4,	/* 4 bits per pixel */											\
	{ 0, 1, 2, 3 },														\
	{ 1*4, 0*4, 1*4+NUM*64*8, 0*4+NUM*64*8, 3*4, 2*4, 3*4+NUM*64*8, 2*4+NUM*64*8,			\
			5*4, 4*4, 5*4+NUM*64*8, 4*4+NUM*64*8, 7*4, 6*4, 7*4+NUM*64*8, 6*4+NUM*64*8 },	\
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,					\
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },		\
	64*8	/* every sprite takes 64 consecutive bytes */				\
}

SPRITELAYOUT(512);
SPRITELAYOUT(1024);


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout_512,            0,   8 },
	{ REGION_GFX2, 0, &tilelayout,             8*16,  16 },
	{ REGION_GFX3, 0, &spritelayout_512, 8*16+16*16, 256 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo ninjemak_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout_1024,            0,   8 },
	{ REGION_GFX2, 0, &tilelayout,              8*16,  16 },
	{ REGION_GFX3, 0, &spritelayout_1024, 8*16+16*16, 256 },
	{ -1 } /* end of array */
};



static struct YM3526interface YM3526_interface =
{
	1,
	8000000/2,	/* 4 MHz? */
	{ 60 }
};

static struct DACinterface dac_interface =
{
	2,
	{ 60, 60 }
};


static struct MachineDriver machine_driver_galivan =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			12000000/2,		/* 6 MHz? */
			readmem,writemem,readport,writeport,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			8000000/2,		/* 4 MHz? */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,0,
			interrupt,7250  /* timed interrupt, ?? Hz */
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	galivan_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256, 8*16+16*16+256*16,
	galivan_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	galivan_vh_start,
	0,
	galivan_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM3526,
			&YM3526_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};

static struct MachineDriver machine_driver_ninjemak =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			12000000/2,		/* 6 MHz? */
			readmem,ninjemak_writemem,ninjemak_readport,ninjemak_writeport,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			8000000/2,		/* 4 MHz? */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,0,
			interrupt,7250	/* timed interrupt, ?? Hz */
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	galivan_init_machine,

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 2*8, 30*8-1 },
	ninjemak_gfxdecodeinfo,
	256, 8*16+16*16+256*16,
	galivan_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	ninjemak_vh_start,
	0,
	ninjemak_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM3526,
			&YM3526_interface
		},
		{
			SOUND_DAC,
			&dac_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( galivan )
	ROM_REGION( 0x14000, REGION_CPU1 )	/* main cpu code */
	ROM_LOAD( "gv1.1b",       0x00000, 0x8000, 0x5e480bfc )
	ROM_LOAD( "gv2.3b",       0x08000, 0x4000, 0x0d1b3538 )
	ROM_LOAD( "gv3.4b",       0x10000, 0x4000, 0x82f0c5e6 ) /* 2 banks at c000 */

	ROM_REGION( 0x10000, REGION_CPU2 )		/* sound cpu code */
	ROM_LOAD( "gv11.14b",     0x0000, 0x4000, 0x05f1a0e3 )
	ROM_LOAD( "gv12.15b",     0x4000, 0x8000, 0x5b7a0d6d )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gv4.13d",      0x00000, 0x4000, 0x162490b4 ) /* chars */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gv7.14f",      0x00000, 0x8000, 0xeaa1a0db ) /* tiles */
	ROM_LOAD( "gv8.15f",      0x08000, 0x8000, 0xf174a41e )
	ROM_LOAD( "gv9.17f",      0x10000, 0x8000, 0xedc60f5d )
	ROM_LOAD( "gv10.19f",     0x18000, 0x8000, 0x41f27fca )

	ROM_REGION( 0x10000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gv14.4f",      0x00000, 0x8000, 0x03e2229f ) /* sprites */
	ROM_LOAD( "gv13.1f",      0x08000, 0x8000, 0xbca9e66b )

	ROM_REGION( 0x8000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "gv6.19d",      0x0000, 0x4000, 0xda38168b )
	ROM_LOAD( "gv5.17d",      0x4000, 0x4000, 0x22492d2a )

	ROM_REGION( 0x0500, REGION_PROMS )
	ROM_LOAD( "mb7114e.9f",   0x0000, 0x0100, 0xde782b3e )	/* red */
	ROM_LOAD( "mb7114e.10f",  0x0100, 0x0100, 0x0ae2a857 )	/* green */
	ROM_LOAD( "mb7114e.11f",  0x0200, 0x0100, 0x7ba8b9d1 )	/* blue */
	ROM_LOAD( "mb7114e.2d",   0x0300, 0x0100, 0x75466109 )	/* sprite lookup table */
	ROM_LOAD( "mb7114e.7f",   0x0400, 0x0100, 0x06538736 )	/* sprite palette bank */
ROM_END

ROM_START( galivan2 )
	ROM_REGION( 0x14000, REGION_CPU1 )		/* main cpu code */
	ROM_LOAD( "e-1",          0x00000, 0x8000, 0xd8cc72b8 )
	ROM_LOAD( "e-2",          0x08000, 0x4000, 0x9e5b3157 )
	ROM_LOAD( "gv3.4b",       0x10000, 0x4000, 0x82f0c5e6 ) /* 2 banks at c000 */

	ROM_REGION( 0x10000, REGION_CPU2 )		/* sound cpu code */
	ROM_LOAD( "gv11.14b",     0x0000, 0x4000, 0x05f1a0e3 )
	ROM_LOAD( "gv12.15b",     0x4000, 0x8000, 0x5b7a0d6d )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gv4.13d",      0x00000, 0x4000, 0x162490b4 ) /* chars */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gv7.14f",      0x00000, 0x8000, 0xeaa1a0db ) /* tiles */
	ROM_LOAD( "gv8.15f",      0x08000, 0x8000, 0xf174a41e )
	ROM_LOAD( "gv9.17f",      0x10000, 0x8000, 0xedc60f5d )
	ROM_LOAD( "gv10.19f",     0x18000, 0x8000, 0x41f27fca )

	ROM_REGION( 0x10000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gv14.4f",      0x00000, 0x8000, 0x03e2229f ) /* sprites */
	ROM_LOAD( "gv13.1f",      0x08000, 0x8000, 0xbca9e66b )

	ROM_REGION( 0x8000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "gv6.19d",      0x0000, 0x4000, 0xda38168b )
	ROM_LOAD( "gv5.17d",      0x4000, 0x4000, 0x22492d2a )

	ROM_REGION( 0x0500, REGION_PROMS )
	ROM_LOAD( "mb7114e.9f",   0x0000, 0x0100, 0xde782b3e )	/* red */
	ROM_LOAD( "mb7114e.10f",  0x0100, 0x0100, 0x0ae2a857 )	/* green */
	ROM_LOAD( "mb7114e.11f",  0x0200, 0x0100, 0x7ba8b9d1 )	/* blue */
	ROM_LOAD( "mb7114e.2d",   0x0300, 0x0100, 0x75466109 )	/* sprite lookup table */
	ROM_LOAD( "mb7114e.7f",   0x0400, 0x0100, 0x06538736 )	/* sprite palette bank */
ROM_END

ROM_START( dangar )
	ROM_REGION( 0x14000, REGION_CPU1 )		/* main cpu code */
	ROM_LOAD( "dangar08.1b",  0x00000, 0x8000, 0xe52638f2 )
	ROM_LOAD( "dangar09.3b",  0x08000, 0x4000, 0x809d280f )
	ROM_LOAD( "dangar10.5b",  0x10000, 0x4000, 0x99a3591b )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* sound cpu code */
	ROM_LOAD( "dangar13.b14", 0x0000, 0x4000, 0x3e041873 )
	ROM_LOAD( "dangar14.b15", 0x4000, 0x8000, 0x488e3463 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dangar05.13d", 0x00000, 0x4000, 0x40cb378a )	/* chars */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dangar01.14f", 0x00000, 0x8000, 0xd59ed1f1 )  /* tiles */
	ROM_LOAD( "dangar02.15f", 0x08000, 0x8000, 0xdfdb931c )
	ROM_LOAD( "dangar03.17f", 0x10000, 0x8000, 0x6954e8c3 )
	ROM_LOAD( "dangar04.19f", 0x18000, 0x8000, 0x4af6a8bf )

	ROM_REGION( 0x10000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dangarxx.f4",  0x00000, 0x8000, 0x55711884 )  /* sprites */
	ROM_LOAD( "dangarxx.f1",  0x08000, 0x8000, 0x8cf11419 )

	ROM_REGION( 0x8000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "dangar07.19d", 0x0000, 0x4000, 0x6dba32cf )
	ROM_LOAD( "dangar06.17d", 0x4000, 0x4000, 0x6c899071 )

	ROM_REGION( 0x0500, REGION_PROMS )
	ROM_LOAD( "82s129.9f",    0x0000, 0x0100, 0xb29f6a07 )	/* red */
	ROM_LOAD( "82s129.10f",   0x0100, 0x0100, 0xc6de5ecb )	/* green */
	ROM_LOAD( "82s129.11f",   0x0200, 0x0100, 0xa5bbd6dc )	/* blue */
	ROM_LOAD( "82s129.2d",    0x0300, 0x0100, 0xa4ac95a5 )	/* sprite lookup table */
	ROM_LOAD( "82s129.7f",    0x0400, 0x0100, 0x29bc6216 )	/* sprite palette bank */
ROM_END

ROM_START( dangar2 )
	ROM_REGION( 0x14000, REGION_CPU1 )		/* main cpu code */
	ROM_LOAD( "dangar2.016",  0x00000, 0x8000, 0x743fa2d4 )
	ROM_LOAD( "dangar2.017",  0x08000, 0x4000, 0x1cdc60a5 )
	ROM_LOAD( "dangar2.018",  0x10000, 0x4000, 0xdb7f6613 )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* sound cpu code */
	ROM_LOAD( "dangar13.b14", 0x0000, 0x4000, 0x3e041873 )
	ROM_LOAD( "dangar14.b15", 0x4000, 0x8000, 0x488e3463 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dangar2.011",  0x00000, 0x4000, 0xe804ffe1 )	/* chars */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dangar01.14f", 0x00000, 0x8000, 0xd59ed1f1 )  /* tiles */
	ROM_LOAD( "dangar02.15f", 0x08000, 0x8000, 0xdfdb931c )
	ROM_LOAD( "dangar03.17f", 0x10000, 0x8000, 0x6954e8c3 )
	ROM_LOAD( "dangar04.19f", 0x18000, 0x8000, 0x4af6a8bf )

	ROM_REGION( 0x10000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dangarxx.f4",  0x00000, 0x8000, 0x55711884 )  /* sprites */
	ROM_LOAD( "dangarxx.f1",  0x08000, 0x8000, 0x8cf11419 )

	ROM_REGION( 0x8000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "dangar07.19d", 0x0000, 0x4000, 0x6dba32cf )
	ROM_LOAD( "dangar06.17d", 0x4000, 0x4000, 0x6c899071 )

	ROM_REGION( 0x0500, REGION_PROMS )
	ROM_LOAD( "82s129.9f",    0x0000, 0x0100, 0xb29f6a07 )	/* red */
	ROM_LOAD( "82s129.10f",   0x0100, 0x0100, 0xc6de5ecb )	/* green */
	ROM_LOAD( "82s129.11f",   0x0200, 0x0100, 0xa5bbd6dc )	/* blue */
	ROM_LOAD( "82s129.2d",    0x0300, 0x0100, 0xa4ac95a5 )	/* sprite lookup table */
	ROM_LOAD( "82s129.7f",    0x0400, 0x0100, 0x29bc6216 )	/* sprite palette bank */
ROM_END

ROM_START( dangarb )
	ROM_REGION( 0x14000, REGION_CPU1 )		/* main cpu code */
	ROM_LOAD( "8",            0x00000, 0x8000, 0x8136fd10 )
	ROM_LOAD( "9",            0x08000, 0x4000, 0x3ce5ec11 )
	ROM_LOAD( "dangar2.018",  0x10000, 0x4000, 0xdb7f6613 )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* sound cpu code */
	ROM_LOAD( "dangar13.b14", 0x0000, 0x4000, 0x3e041873 )
	ROM_LOAD( "dangar14.b15", 0x4000, 0x8000, 0x488e3463 )

	ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dangar2.011",  0x00000, 0x4000, 0xe804ffe1 )	/* chars */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dangar01.14f", 0x00000, 0x8000, 0xd59ed1f1 )  /* tiles */
	ROM_LOAD( "dangar02.15f", 0x08000, 0x8000, 0xdfdb931c )
	ROM_LOAD( "dangar03.17f", 0x10000, 0x8000, 0x6954e8c3 )
	ROM_LOAD( "dangar04.19f", 0x18000, 0x8000, 0x4af6a8bf )

	ROM_REGION( 0x10000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dangarxx.f4",  0x00000, 0x8000, 0x55711884 )  /* sprites */
	ROM_LOAD( "dangarxx.f1",  0x08000, 0x8000, 0x8cf11419 )

	ROM_REGION( 0x8000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "dangar07.19d", 0x0000, 0x4000, 0x6dba32cf )
	ROM_LOAD( "dangar06.17d", 0x4000, 0x4000, 0x6c899071 )

	ROM_REGION( 0x0500, REGION_PROMS )
	ROM_LOAD( "82s129.9f",    0x0000, 0x0100, 0xb29f6a07 )	/* red */
	ROM_LOAD( "82s129.10f",   0x0100, 0x0100, 0xc6de5ecb )	/* green */
	ROM_LOAD( "82s129.11f",   0x0200, 0x0100, 0xa5bbd6dc )	/* blue */
	ROM_LOAD( "82s129.2d",    0x0300, 0x0100, 0xa4ac95a5 )	/* sprite lookup table */
	ROM_LOAD( "82s129.7f",    0x0400, 0x0100, 0x29bc6216 )	/* sprite palette bank */
ROM_END

ROM_START( ninjemak )
	ROM_REGION( 0x18000, REGION_CPU1 )	/* main cpu code */
	ROM_LOAD( "ninjemak.1",   0x00000, 0x8000, 0x12b0a619 )
	ROM_LOAD( "ninjemak.2",   0x08000, 0x4000, 0xd5b505d1 )
	ROM_LOAD( "ninjemak.3",   0x10000, 0x8000, 0x68c92bf6 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* sound cpu code */
	ROM_LOAD( "ninjemak.12",  0x0000, 0x4000, 0x3d1cd329 )
	ROM_LOAD( "ninjemak.13",  0x4000, 0x8000, 0xac3a0b81 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ninjemak.4",   0x00000, 0x8000, 0x83702c37 )	/* chars */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ninjemak.8",   0x00000, 0x8000, 0x655f0a58 ) /* tiles */
	ROM_LOAD( "ninjemak.9",   0x08000, 0x8000, 0x934e1703 )
	ROM_LOAD( "ninjemak.10",  0x10000, 0x8000, 0x955b5c45 )
	ROM_LOAD( "ninjemak.11",  0x18000, 0x8000, 0xbbd2e51c )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ninjemak.16",  0x00000, 0x8000, 0x8df93fed )  /* sprites */
	ROM_LOAD( "ninjemak.17",  0x08000, 0x8000, 0xa3efd0fc )
	ROM_LOAD( "ninjemak.14",  0x10000, 0x8000, 0xbff332d3 )
	ROM_LOAD( "ninjemak.15",  0x18000, 0x8000, 0x56430ed4 )

	ROM_REGION( 0x8000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "ninjemak.7",   0x0000, 0x4000, 0x80c20d36 )
	ROM_LOAD( "ninjemak.6",   0x4000, 0x4000, 0x1da7a651 )

	ROM_REGION( 0x4000, REGION_GFX5 )	/* data for mcu/blitter? */
	ROM_LOAD( "ninjemak.5",   0x0000, 0x4000, 0x5f91dd30 )	/* text layer data */

	ROM_REGION( 0x0500, REGION_PROMS )	/* Region 3 - color data */
	ROM_LOAD( "ninjemak.pr1", 0x0000, 0x0100, 0x8a62d4e4 )	/* red */
	ROM_LOAD( "ninjemak.pr2", 0x0100, 0x0100, 0x2ccf976f )	/* green */
	ROM_LOAD( "ninjemak.pr3", 0x0200, 0x0100, 0x16b2a7a4 )	/* blue */
	ROM_LOAD( "yncp-2d.bin",  0x0300, 0x0100, BADCRC( 0x23bade78 ) )	/* sprite lookup table */
	ROM_LOAD( "yncp-7f.bin",  0x0400, 0x0100, BADCRC( 0x262d0809 ) )	/* sprite palette bank */
ROM_END

ROM_START( youma )
	ROM_REGION( 0x18000, REGION_CPU1 )	/* main cpu code */
	ROM_LOAD( "ync-1.bin",    0x00000, 0x8000, 0x0552adab )
	ROM_LOAD( "ync-2.bin",    0x08000, 0x4000, 0xf961e5e6 )
	ROM_LOAD( "ync-3.bin",    0x10000, 0x8000, 0x9ad50a5e )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* sound cpu code */
	ROM_LOAD( "ninjemak.12",  0x0000, 0x4000, 0x3d1cd329 )
	ROM_LOAD( "ninjemak.13",  0x4000, 0x8000, 0xac3a0b81 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ync-4.bin",    0x00000, 0x8000, 0xa1954f44 )	/* chars */

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ninjemak.8",   0x00000, 0x8000, 0x655f0a58 ) /* tiles */
	ROM_LOAD( "ninjemak.9",   0x08000, 0x8000, 0x934e1703 )
	ROM_LOAD( "ninjemak.10",  0x10000, 0x8000, 0x955b5c45 )
	ROM_LOAD( "ninjemak.11",  0x18000, 0x8000, 0xbbd2e51c )

	ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ninjemak.16",  0x00000, 0x8000, 0x8df93fed )  /* sprites */
	ROM_LOAD( "ninjemak.17",  0x08000, 0x8000, 0xa3efd0fc )
	ROM_LOAD( "ninjemak.14",  0x10000, 0x8000, 0xbff332d3 )
	ROM_LOAD( "ninjemak.15",  0x18000, 0x8000, 0x56430ed4 )

	ROM_REGION( 0x8000, REGION_GFX4 )	/* background tilemaps */
	ROM_LOAD( "ninjemak.7",   0x0000, 0x4000, 0x80c20d36 )
	ROM_LOAD( "ninjemak.6",   0x4000, 0x4000, 0x1da7a651 )

	ROM_REGION( 0x4000, REGION_GFX5 )	/* data for mcu/blitter? */
	ROM_LOAD( "ync-5.bin",    0x0000, 0x4000, 0x993e4ab2 )	/* text layer data */

	ROM_REGION( 0x0500, REGION_PROMS )	/* Region 3 - color data */
	ROM_LOAD( "yncp-6e.bin",  0x0000, 0x0100, 0xea47b91a )	/* red */
	ROM_LOAD( "yncp-7e.bin",  0x0100, 0x0100, 0xe94c0fed )	/* green */
	ROM_LOAD( "yncp-8e.bin",  0x0200, 0x0100, 0xffb4b287 )	/* blue */
	ROM_LOAD( "yncp-2d.bin",  0x0300, 0x0100, 0x23bade78 )	/* sprite lookup table */
	ROM_LOAD( "yncp-7f.bin",  0x0400, 0x0100, 0x262d0809 )	/* sprite palette bank */
ROM_END



GAME( 1985, galivan,  0,        galivan,  galivan,  0, ROT270, "Nichibutsu", "Galivan - Cosmo Police (12/16/1985)" )
GAME( 1985, galivan2, galivan,  galivan,  galivan,  0, ROT270, "Nichibutsu", "Galivan - Cosmo Police (12/11/1985)" )
GAME( 1986, dangar,   0,        galivan,  dangar,   0, ROT270, "Nichibutsu", "Dangar - Ufo Robo (12/1/1986)" )
GAME( 1986, dangar2,  dangar,   galivan,  dangar2,  0, ROT270, "Nichibutsu", "Dangar - Ufo Robo (9/26/1986)" )
GAME( 1986, dangarb,  dangar,   galivan,  dangarb,  0, ROT270, "bootleg", "Dangar - Ufo Robo (bootleg)" )
GAME( 1986, ninjemak, 0,        ninjemak, ninjemak, 0, ROT270, "Nichibutsu", "Ninja Emaki (US)" )
GAME( 1986, youma,    ninjemak, ninjemak, youma,    0, ROT270, "Nichibutsu", "Youma Ninpou Chou (Japan)" )

