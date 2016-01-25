/***************************************************************************

Super Cobra and Co. memory map (preliminary)

Main CPU:
--------

There seems to be 2 main board types:

Type 1      Type 2

0000-7fff   0000-7fff	ROM (not all games use the entire range)
8000-87ff   8000-87ff	RAM
8800-8bff   9000-93ff	video RAM
9000-903f   8800-883f	screen attributes
9040-905f   8840-885f	sprites
9060-907f   8860-887f	bullets

read:
b000      	9800		watchdog reset
9800      	a000		IN0
9801      	a004		IN1
9802		a008		IN2

write:
a801      	b004		interrupt enable
a802      	b006		coin counter
a803      	b002		? (POUT1)
a804      	b000		stars on
a805      	b00a		? (POUT2)
a806      	b00e		screen vertical flip
a807      	b00c		screen horizontal flip
a000      	a800		To AY-3-8910 port A (commands for the audio CPU)
a001      	a804		bit 3 = trigger interrupt on audio CPU


Sound CPU:

0000-1fff   ROM
8000-83ff   RAM
9000-9fff   R/C Filter (2 bits for each of the 6 channels)

I/O:

10  		AY8910 #0 control
20			AY8910 #0 data port
40			AY8910 #1 control port
80			AY8910 #1 data port


TODO:
----

	Need correct color PROMs for:
		Super Bond

Notes:
-----

- Calipso was apperantly redesigned for two player simultanious play.
  There is code at $298a to flip the screen, but location $8669 has to be
  set to 2. It's set to 1 no matter how many players are playing.
  It's possible that there is a cocktail version of the game.

- Video Hustler and its two bootlegs all have identical code, the only
  differences are the title, copyright removed, different encryptions or
  no encryption, plus hustlerb has a different memory map.

- In Tazmania, when set to Upright mode, player 2 left skips the current
  level

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



extern unsigned char *galaxian_attributesram;
extern unsigned char *galaxian_bulletsram;
extern size_t galaxian_bulletsram_size;

void galaxian_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void rescue_vh_convert_color_prom  (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void minefld_vh_convert_color_prom (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void stratgyx_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

int  scramble_vh_start(void);
int  rescue_vh_start  (void);
int  minefld_vh_start (void);
int  calipso_vh_start (void);
int  stratgyx_vh_start(void);

void galaxian_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( galaxian_attributes_w );
WRITE_HANDLER( galaxian_stars_w );
int  scramble_vh_interrupt(void);
WRITE_HANDLER( scramble_background_w );
WRITE_HANDLER( scramble_filter_w );

READ_HANDLER( scramble_portB_r );
READ_HANDLER( frogger_portB_r );
WRITE_HANDLER( scramble_sh_irqtrigger_w );


static void scobra_init_machine(void)
{
	/* we must start with NMI interrupts disabled, otherwise some games */
	/* (e.g. Lost Tomb, Rescue) will not pass the startup test. */
	interrupt_enable_w(0,0);
}

static READ_HANDLER( moonwar2_IN0_r )
{
	int sign;
	int delta;

	delta = readinputport(3);

	sign = (delta & 0x80) >> 3;
	delta &= 0x0f;

	return ((readinputport(0) & 0xe0) | delta | sign );
}

static WRITE_HANDLER( stratgyx_coin_counter_w )
{
	/* Bit 1 selects coin counter */
	coin_counter_w(offset >> 1, data);
}


static struct MemoryReadAddress type1_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8bff, MRA_RAM },	/* RAM and Video RAM */
	{ 0x9000, 0x907f, MRA_RAM },	/* screen attributes, sprites, bullets */
	{ 0x9800, 0x9800, input_port_0_r },	/* IN0 */
	{ 0x9801, 0x9801, input_port_1_r },	/* IN1 */
	{ 0x9802, 0x9802, input_port_2_r },	/* IN2 */
	{ 0xb000, 0xb000, watchdog_reset_r },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress type1_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8800, 0x8bff, videoram_w, &videoram, &videoram_size },
	{ 0x8c00, 0x8fff, MWA_NOP},
	{ 0x9000, 0x903f, galaxian_attributes_w, &galaxian_attributesram },
	{ 0x9040, 0x905f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x9060, 0x907f, MWA_RAM, &galaxian_bulletsram, &galaxian_bulletsram_size },
	{ 0x9080, 0x90ff, MWA_NOP},
	{ 0xa000, 0xa000, soundlatch_w },
	{ 0xa001, 0xa001, scramble_sh_irqtrigger_w },
	{ 0xa801, 0xa801, interrupt_enable_w },
	{ 0xa802, 0xa802, coin_counter_w },
	{ 0xa803, 0xa803, scramble_background_w },
	{ 0xa804, 0xa804, galaxian_stars_w },
	{ 0xa806, 0xa806, flip_screen_x_w },
	{ 0xa807, 0xa807, flip_screen_y_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress type2_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8bff, MRA_RAM },	/* RAM and Video RAM */
	{ 0x9000, 0x93ff, MRA_RAM },	/* screen attributes, sprites, bullets */
	{ 0x9800, 0x9800, watchdog_reset_r},
	{ 0xa000, 0xa000, input_port_0_r },	/* IN0 */
	{ 0xa004, 0xa004, input_port_1_r },	/* IN1 */
	{ 0xa008, 0xa008, input_port_2_r },	/* IN2 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress type2_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8800, 0x883f, galaxian_attributes_w, &galaxian_attributesram },
	{ 0x8840, 0x885f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x8860, 0x887f, MWA_RAM, &galaxian_bulletsram, &galaxian_bulletsram_size },
	{ 0x8880, 0x88ff, MWA_NOP},
	{ 0x9000, 0x93ff, videoram_w, &videoram, &videoram_size },
	{ 0xa800, 0xa800, soundlatch_w },
	{ 0xa804, 0xa804, scramble_sh_irqtrigger_w },
	{ 0xb000, 0xb000, galaxian_stars_w },
	{ 0xb002, 0xb002, scramble_background_w },
	{ 0xb004, 0xb004, interrupt_enable_w },
	{ 0xb006, 0xb008, stratgyx_coin_counter_w },
	{ 0xb00c, 0xb00c, flip_screen_y_w },
	{ 0xb00e, 0xb00e, flip_screen_x_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress hustler_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x8bff, MRA_RAM },	/* RAM and Video RAM */
	{ 0x9000, 0x907f, MRA_RAM },	/* screen attributes, sprites, bullets */
	{ 0xb800, 0xb800, watchdog_reset_r },
	{ 0xd000, 0xd000, input_port_0_r },	/* IN0 */
	{ 0xd008, 0xd008, input_port_1_r },	/* IN1 */
	{ 0xd010, 0xd010, input_port_2_r },	/* IN2 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress hustler_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8800, 0x8bff, videoram_w, &videoram, &videoram_size },
	{ 0x9000, 0x903f, galaxian_attributes_w, &galaxian_attributesram },
	{ 0x9040, 0x905f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x9060, 0x907f, MWA_RAM, &galaxian_bulletsram, &galaxian_bulletsram_size },
	{ 0xa802, 0xa802, flip_screen_x_w },
	{ 0xa804, 0xa804, interrupt_enable_w },
	{ 0xa806, 0xa806, flip_screen_y_w },
	{ 0xa80e, 0xa80e, MWA_NOP },	/* coin counters */
	{ 0xe000, 0xe000, soundlatch_w },
	{ 0xe008, 0xe008, scramble_sh_irqtrigger_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress hustlerb_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x8000, 0x8bff, MRA_RAM },	/* RAM and Video RAM */
	{ 0x9000, 0x907f, MRA_RAM },	/* screen attributes, sprites, bullets */
	{ 0xb000, 0xb000, watchdog_reset_r },
	{ 0xc100, 0xc100, input_port_0_r },	/* IN0 */
	{ 0xc101, 0xc101, input_port_1_r },	/* IN1 */
	{ 0xc102, 0xc102, input_port_2_r },	/* IN2 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress hustlerb_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8800, 0x8bff, videoram_w, &videoram, &videoram_size },
	{ 0x9000, 0x903f, galaxian_attributes_w, &galaxian_attributesram },
	{ 0x9040, 0x905f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x9060, 0x907f, MWA_RAM, &galaxian_bulletsram, &galaxian_bulletsram_size },
	{ 0xa801, 0xa801, interrupt_enable_w },
	{ 0xa802, 0xa802, MWA_NOP },	/* coin counters */
	{ 0xa806, 0xa806, flip_screen_y_w },
	{ 0xa807, 0xa807, flip_screen_x_w },
	{ 0xc200, 0xc200, soundlatch_w },
	{ 0xc201, 0xc201, scramble_sh_irqtrigger_w },
	{ -1 }	/* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x8000, 0x83ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x8000, 0x83ff, MWA_RAM },
	{ 0x9000, 0x9fff, scramble_filter_w },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress hustler_sound_readmem[] =
{
	{ 0x0000, 0x0fff, MRA_ROM },
	{ 0x4000, 0x43ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress hustler_sound_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_ROM },
	{ 0x4000, 0x43ff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct IOReadPort sound_readport[] =
{
	{ 0x20, 0x20, AY8910_read_port_0_r },
	{ 0x80, 0x80, AY8910_read_port_1_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort sound_writeport[] =
{
	{ 0x10, 0x10, AY8910_control_port_0_w },
	{ 0x20, 0x20, AY8910_write_port_0_w },
	{ 0x40, 0x40, AY8910_control_port_1_w },
	{ 0x80, 0x80, AY8910_write_port_1_w },
	{ -1 }	/* end of table */
};

static struct IOReadPort hustler_sound_readport[] =
{
	{ 0x40, 0x40, AY8910_read_port_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort hustler_sound_writeport[] =
{
	{ 0x40, 0x40, AY8910_write_port_0_w },
	{ 0x80, 0x80, AY8910_control_port_0_w },
	{ -1 }	/* end of table */
};

static struct IOReadPort hustlerb_sound_readport[] =
{
	{ 0x80, 0x80, AY8910_read_port_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort hustlerb_sound_writeport[] =
{
	{ 0x40, 0x40, AY8910_control_port_0_w },
	{ 0x80, 0x80, AY8910_write_port_0_w },
	{ -1 }	/* end of table */
};


INPUT_PORTS_START( scobra )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x01, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "1 Coin/99 Credits" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* identical to scobra apart from the number of lives */
INPUT_PORTS_START( scobrak )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x01, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "1 Coin/99 Credits" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( stratgyx )
	PORT_START      /* IN0 */
	PORT_BIT( 0x81, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x03, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "99", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP   | IPF_8WAY )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2  )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START      /* IN2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "1 Coin/99 Credits" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP   | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( armorcar )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* IN1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_COCKTAIL )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "Coin A 1/2 Coin B 2/1" )
	PORT_DIPSETTING(    0x04, "Coin A 1/3 Coin B 3/1" )
	PORT_DIPSETTING(    0x06, "Coin A 1/4 Coin B 4/1" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( moonwar2 )
	PORT_START      /* IN0 */
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* the spinner */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, DEF_STR( Free_Play ) )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START      /* IN2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_4C ) )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 - dummy port for the dial */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_CENTER, 25, 10, 0, 0 )
INPUT_PORTS_END

INPUT_PORTS_START( monwar2a )
	PORT_START      /* IN0 */
	PORT_BIT( 0x1f, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* the spinner */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, DEF_STR( Free_Play ) )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START      /* IN2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_4C ) )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN3 - dummy port for the dial */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_CENTER, 25, 10, 0, 0 )
INPUT_PORTS_END

INPUT_PORTS_START( spdcoin )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x01, 0x00, "Freeze" )   /* Dip Sw #2 */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR ( Unknown ) ) /* Dip Sw #1 */
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )    /* Dip Sw #5 */
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )    /* Dip Sw #4 */
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Lives ) )    /* Dip Sw #3 */
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN3 - dummy port for the dial */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_CENTER, 25, 10, 0, 0 )
INPUT_PORTS_END

INPUT_PORTS_START( darkplnt )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* IN1 */
	PORT_DIPNAME( 0x01, 0x01, "Bonus Occurrence" )
	PORT_DIPSETTING(    0x01, "Once" )
	PORT_DIPSETTING(    0x00, "Every" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "Coin A 1/2 Coin B 2/1" )
	PORT_DIPSETTING(    0x04, "Coin A 1/3 Coin B 3/1" )
	PORT_DIPSETTING(    0x06, "Coin A 1/4 Coin B 4/1" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "100k" )
	PORT_DIPSETTING(    0x08, "200k" )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( tazmania )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* IN1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "Coin A 1/2 Coin B 2/1" )
	PORT_DIPSETTING(    0x04, "Coin A 1/3 Coin B 3/1" )
	PORT_DIPSETTING(    0x06, "Coin A 1/4 Coin B 4/1" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/* Cocktail mode is N/A */
INPUT_PORTS_START( calipso )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x08, 0x08, "Cabinet (Not Supported)" )
	PORT_DIPSETTING(    0x08, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

/* Cocktail mode not working due to bug */
INPUT_PORTS_START( anteater )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* IN1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "Coin A 1/2 Coin B 2/1" )
	PORT_DIPSETTING(    0x04, "Coin A 1/3 Coin B 3/1" )
	PORT_DIPSETTING(    0x06, "Coin A 1/4 Coin B 4/1" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( rescue )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON6 )
	PORT_DIPNAME( 0x02, 0x02, "Starting Level" )
	PORT_DIPSETTING(    0x02, "1" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* IN1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "Coin A 1/2 Coin B 2/1" )
	PORT_DIPSETTING(    0x04, "Coin A 1/3 Coin B 3/1" )
	PORT_DIPSETTING(    0x06, "Coin A 1/4 Coin B 4/1" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( minefld )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON6 )
	PORT_DIPNAME( 0x02, 0x02, "Starting Level" )
	PORT_DIPSETTING(    0x02, "1" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* IN1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "Coin A 1/2 Coin B 2/1" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x0c, "Easy" )
	PORT_DIPSETTING(    0x08, "Medium" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/* Cocktail mode is N/A */
INPUT_PORTS_START( losttomb )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, DEF_STR( Free_Play ) )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON6 )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "A 1/2 B 2/1" )
	PORT_DIPSETTING(    0x04, "A 1/3 B 3/1" )
	PORT_DIPSETTING(    0x06, "A 1/4 B 4/1" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/* Cocktail mode is N/A */
INPUT_PORTS_START( superbon )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START	/* IN1 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, DEF_STR( Free_Play ) )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, "A 1/2 B 2/1" )
	PORT_DIPSETTING(    0x04, "A 1/3 B 3/1" )
	PORT_DIPSETTING(    0x06, "A 1/4 B 4/1" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( hustler )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START      /* IN1 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_BITX(    0x02, 0x00, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Infinite Lives", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	2,	/* 2 bits per pixel */
	{ 0, 256*8*8 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	/* 64 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 64*16*16 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};
static struct GfxLayout bulletlayout =
{
	/* there is no gfx ROM for this one, it is generated by the hardware */
	7,1,	/* it's just 1 pixel, but we use 7*1 to position it correctly */
	1,	/* just one */
	1,	/* 1 bit per pixel */
	{ 17*8*8 },	/* point to letter "A" */
	{ 3, 0, 0, 0, 0, 0, 0 },	/* I "know" that this bit of the */
	{ 1*8 },						/* graphics ROMs is 1 */
	0	/* no use */
};
static struct GfxLayout armorcar_bulletlayout =
{
	/* there is no gfx ROM for this one, it is generated by the hardware */
	7,1,	/* 4*1 line, I think - 7*1 to position it correctly */
	1,	/* just one */
	1,	/* 1 bit per pixel */
	{ 0 },
	{ 2, 2, 2, 2, 0, 0, 0 },	/* I "know" that this bit of the */
	{ 8 },						/* graphics ROMs is 1 */
	0	/* no use */
};

static struct GfxLayout calipso_charlayout =
{
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	2,	/* 2 bits per pixel */
	{ 0, 1024*8*8 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout calipso_spritelayout =
{
	16,16,	/* 16*16 sprites */
	256,	/* 256 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 256*16*16 },	/* the two bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};
static struct GfxLayout backgroundlayout =
{
	/* there is no gfx ROM for this one, it is generated by the hardware */
	8,8,
	32,	/* one for each column */
	7,	/* 128 colors max */
	{ 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8*8, 1*8*8, 2*8*8, 3*8*8, 4*8*8, 5*8*8, 6*8*8, 7*8*8 },
	{ 0, 8, 16, 24, 32, 40, 48, 56 },
	8*8*8	/* each character takes 64 bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,     0, 8 },
	{ REGION_GFX1, 0, &spritelayout,   0, 8 },
	{ REGION_GFX1, 0, &bulletlayout, 8*4, 1 },	/* 1 color code instead of 2, so all */
											/* shots will be yellow */
	{ 0,           0, &backgroundlayout, 8*4+2*2, 1 },	/* this will be dynamically created */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo armorcar_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,     0, 8 },
	{ REGION_GFX1, 0, &spritelayout,   0, 8 },
	{ REGION_GFX1, 0, &armorcar_bulletlayout, 8*4, 2 },
	{ 0,           0, &backgroundlayout, 8*4+2*2, 1 },	/* this will be dynamically created */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo calipso_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &calipso_charlayout,     0, 8 },
	{ REGION_GFX1, 0, &calipso_spritelayout,   0, 8 },
	{ REGION_GFX1, 0, &bulletlayout, 8*4, 1 },	/* 1 color code instead of 2, so all */
											/* shots will be yellow */
	{ 0,           0, &backgroundlayout, 8*4+2*2, 1 },	/* this will be dynamically created */
	{ -1 } /* end of array */
};



static struct AY8910interface ay8910_interface =
{
	2,	/* 2 chips */
	14318000/8,	/* 1.78975 Mhz */
	/* Ant Eater clips if the volume is set higher than this */
	{ MIXERG(16,MIXER_GAIN_2x,MIXER_PAN_CENTER), MIXERG(16,MIXER_GAIN_2x,MIXER_PAN_CENTER) },
	{ 0, soundlatch_r },
	{ 0, scramble_portB_r },
	{ 0 },
	{ 0 }
};

static struct AY8910interface hustler_ay8910_interface =
{
	1,	/* 1 chip */
	14318000/8,	/* 1.78975 Mhz */
	{ 80 },
	{ soundlatch_r },
	{ frogger_portB_r },
	{ 0 },
	{ 0 }
};



static struct MachineDriver machine_driver_type1 =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			type1_readmem,type1_writemem,0,0,
			scramble_vh_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	scobra_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32+64+1,8*4+2*2+128*1,	/* 32 for the characters, 64 for the stars, 1 for background */
	galaxian_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	scramble_vh_start,
	generic_vh_stop,
	galaxian_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

/* same as the above, the only difference is in gfxdecodeinfo to have long bullets */
static struct MachineDriver machine_driver_armorcar =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			type1_readmem,type1_writemem,0,0,
			scramble_vh_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	scobra_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	armorcar_gfxdecodeinfo,
	32+64+1,8*4+2*2+128*1,	/* 32 for the characters, 64 for the stars, 1 for background */
	galaxian_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	scramble_vh_start,
	generic_vh_stop,
	galaxian_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

/* Rescue, Minefield and Strategy X have extra colours, and custom video initialise */
/* routines to set up the graduated colour backgound they use */
static struct MachineDriver machine_driver_rescue =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			type1_readmem,type1_writemem,0,0,
			scramble_vh_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	scobra_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32+64+64,8*4+2*2+128*1,	/* 32 for the characters, 64 for the stars, 64 for background */
	rescue_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	rescue_vh_start,
	generic_vh_stop,
	galaxian_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

static struct MachineDriver machine_driver_minefld =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			type1_readmem,type1_writemem,0,0,
			scramble_vh_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	scobra_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32+64+128,8*4+2*2+128*1,	/* 32 for the characters, 64 for the stars, 128 for background */
	minefld_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	minefld_vh_start,
	generic_vh_stop,
	galaxian_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

static struct MachineDriver machine_driver_stratgyx =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			type2_readmem,type2_writemem,0,0,
			scramble_vh_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	scobra_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32+64+2,8*4+2*2+128*1,	/* 32 for the characters, 64 for the stars, 2 for background */
	stratgyx_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	stratgyx_vh_start,
	generic_vh_stop,
	galaxian_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

static struct MachineDriver machine_driver_type2 =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			type2_readmem,type2_writemem,0,0,
			scramble_vh_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	scobra_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32+64+1,8*4+2*2+128*1,	/* 32 for the characters, 64 for the stars, 1 for background */
	galaxian_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	scramble_vh_start,
	generic_vh_stop,
	galaxian_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

static struct MachineDriver machine_driver_hustler =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			hustler_readmem,hustler_writemem,0,0,
			scramble_vh_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			hustler_sound_readmem,hustler_sound_writemem,hustler_sound_readport,hustler_sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	scobra_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32+64+1,8*4+2*2+128*1,	/* 32 for the characters, 64 for the stars, 1 for background */
	galaxian_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	scramble_vh_start,
	generic_vh_stop,
	galaxian_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&hustler_ay8910_interface
		}
	}
};

static struct MachineDriver machine_driver_hustlerb =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			hustlerb_readmem,hustlerb_writemem,0,0,
			scramble_vh_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			sound_readmem,sound_writemem,hustlerb_sound_readport,hustlerb_sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	scobra_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	32+64+1,8*4+2*2+128*1,	/* 32 for the characters, 64 for the stars, 1 for background */
	galaxian_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	scramble_vh_start,
	generic_vh_stop,
	galaxian_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&hustler_ay8910_interface
		}
	}
};

static struct MachineDriver machine_driver_calipso =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,	/* 3.072 Mhz */
			type1_readmem,type1_writemem,0,0,
			scramble_vh_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,	/* 1.78975 Mhz */
			sound_readmem,sound_writemem,sound_readport,sound_writeport,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, 2500,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	scobra_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	calipso_gfxdecodeinfo,
	32+64+1,8*4+2*2+128*1,	/* 32 for the characters, 64 for the stars, 1 for background */
	galaxian_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	calipso_vh_start,
	generic_vh_stop,
	galaxian_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( scobra )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "2c",           0x0000, 0x1000, 0xa0744b3f )
	ROM_LOAD( "2e",           0x1000, 0x1000, 0x8e7245cd )
	ROM_LOAD( "2f",           0x2000, 0x1000, 0x47a4e6fb )
	ROM_LOAD( "2h",           0x3000, 0x1000, 0x7244f21c )
	ROM_LOAD( "2j",           0x4000, 0x1000, 0xe1f8a801 )
	ROM_LOAD( "2l",           0x5000, 0x1000, 0xd52affde )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "5c",           0x0000, 0x0800, 0xd4346959 )
	ROM_LOAD( "5d",           0x0800, 0x0800, 0xcc025d95 )
	ROM_LOAD( "5e",           0x1000, 0x0800, 0x1628c53f )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5f",           0x0000, 0x0800, 0x64d113b4 )
	ROM_LOAD( "5h",           0x0800, 0x0800, 0xa96316d3 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "82s123.6e",    0x0000, 0x0020, 0x9b87f90d )
ROM_END

ROM_START( scobras )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "scobra2c.bin", 0x0000, 0x1000, 0xe15ade38 )
	ROM_LOAD( "scobra2e.bin", 0x1000, 0x1000, 0xa270e44d )
	ROM_LOAD( "scobra2f.bin", 0x2000, 0x1000, 0xbdd70346 )
	ROM_LOAD( "scobra2h.bin", 0x3000, 0x1000, 0xdca5ec31 )
	ROM_LOAD( "scobra2j.bin", 0x4000, 0x1000, 0x0d8f6b6e )
	ROM_LOAD( "scobra2l.bin", 0x5000, 0x1000, 0x6f80f3a9 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "snd_5c.bin",   0x0000, 0x0800, 0xdeeb0dd3 )
	ROM_LOAD( "snd_5d.bin",   0x0800, 0x0800, 0x872c1a74 )
	ROM_LOAD( "snd_5e.bin",   0x1000, 0x0800, 0xccd7a110 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5f",           0x0000, 0x0800, 0x64d113b4 )
	ROM_LOAD( "5h",           0x0800, 0x0800, 0xa96316d3 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "82s123.6e",    0x0000, 0x0020, 0x9b87f90d )
ROM_END

ROM_START( scobrab )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "vid_2c.bin",   0x0000, 0x0800, 0xaeddf391 )
	ROM_LOAD( "vid_2e.bin",   0x0800, 0x0800, 0x72b57eb7 )
	ROM_LOAD( "scobra2e.bin", 0x1000, 0x1000, 0xa270e44d )
	ROM_LOAD( "scobra2f.bin", 0x2000, 0x1000, 0xbdd70346 )
	ROM_LOAD( "scobra2h.bin", 0x3000, 0x1000, 0xdca5ec31 )
	ROM_LOAD( "scobra2j.bin", 0x4000, 0x1000, 0x0d8f6b6e )
	ROM_LOAD( "scobra2l.bin", 0x5000, 0x1000, 0x6f80f3a9 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "snd_5c.bin",   0x0000, 0x0800, 0xdeeb0dd3 )
	ROM_LOAD( "snd_5d.bin",   0x0800, 0x0800, 0x872c1a74 )
	ROM_LOAD( "snd_5e.bin",   0x1000, 0x0800, 0xccd7a110 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5f",           0x0000, 0x0800, 0x64d113b4 )
	ROM_LOAD( "5h",           0x0800, 0x0800, 0xa96316d3 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "82s123.6e",    0x0000, 0x0020, 0x9b87f90d )
ROM_END

ROM_START( stratgyx )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "2c_1.bin",     0x0000, 0x1000, 0xeec01237 )
	ROM_LOAD( "2e_2.bin",     0x1000, 0x1000, 0x926cb2d5 )
	ROM_LOAD( "2f_3.bin",     0x2000, 0x1000, 0x849e2504 )
	ROM_LOAD( "2h_4.bin",     0x3000, 0x1000, 0x8a64069b )
	ROM_LOAD( "2j_5.bin",     0x4000, 0x1000, 0x78b9b898 )
	ROM_LOAD( "2l_6.bin",     0x5000, 0x1000, 0x20bae414 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound code */
	ROM_LOAD( "s1.bin",       0x0000, 0x1000, 0x713a5db8 )
	ROM_LOAD( "s2.bin",       0x1000, 0x1000, 0x46079411 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5f_c2.bin",    0x0000, 0x0800, 0x7121b679 )
	ROM_LOAD( "5h_c1.bin",    0x0800, 0x0800, 0xd105ad91 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "strategy.6e",  0x0000, 0x0020, 0x51a629e1 )
ROM_END

ROM_START( stratgys )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "2c.cpu",       0x0000, 0x1000, 0xf2aaaf2b )
	ROM_LOAD( "2e.cpu",       0x1000, 0x1000, 0x5873fdc8 )
	ROM_LOAD( "2f.cpu",       0x2000, 0x1000, 0x532d604f )
	ROM_LOAD( "2h.cpu",       0x3000, 0x1000, 0x82b1d95e )
	ROM_LOAD( "2j.cpu",       0x4000, 0x1000, 0x66e84cde )
	ROM_LOAD( "2l.cpu",       0x5000, 0x1000, 0x62b032d0 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound code */
	ROM_LOAD( "s1.bin",       0x0000, 0x1000, 0x713a5db8 )
	ROM_LOAD( "s2.bin",       0x1000, 0x1000, 0x46079411 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5f.cpu",       0x0000, 0x0800, 0xf4aa5ddd )
	ROM_LOAD( "5h.cpu",       0x0800, 0x0800, 0x548e4635 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "strategy.6e",  0x0000, 0x0020, 0x51a629e1 )
ROM_END

ROM_START( armorcar )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "cpu.2c",       0x0000, 0x1000, 0x0d7bfdfb )
	ROM_LOAD( "cpu.2e",       0x1000, 0x1000, 0x76463213 )
	ROM_LOAD( "cpu.2f",       0x2000, 0x1000, 0x2cc6d5f0 )
	ROM_LOAD( "cpu.2h",       0x3000, 0x1000, 0x61278dbb )
	ROM_LOAD( "cpu.2j",       0x4000, 0x1000, 0xfb158d8c )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "sound.5c",     0x0000, 0x0800, 0x54ee7753 )
	ROM_LOAD( "sound.5d",     0x0800, 0x0800, 0x5218fec0 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu.5f",       0x0000, 0x0800, 0x8a3da4d1 )
	ROM_LOAD( "cpu.5h",       0x0800, 0x0800, 0x85bdb113 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "82s123.6e",    0x0000, 0x0020, 0x9b87f90d )
ROM_END

ROM_START( armorca2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "2c",           0x0000, 0x1000, 0xe393bd2f )
	ROM_LOAD( "2e",           0x1000, 0x1000, 0xb7d443af )
	ROM_LOAD( "2g",           0x2000, 0x1000, 0xe67380a4 )
	ROM_LOAD( "2h",           0x3000, 0x1000, 0x72af7b37 )
	ROM_LOAD( "2j",           0x4000, 0x1000, 0xe6b0dd7f )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "sound.5c",     0x0000, 0x0800, 0x54ee7753 )
	ROM_LOAD( "sound.5d",     0x0800, 0x0800, 0x5218fec0 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cpu.5f",       0x0000, 0x0800, 0x8a3da4d1 )
	ROM_LOAD( "cpu.5h",       0x0800, 0x0800, 0x85bdb113 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "82s123.6e",    0x0000, 0x0020, 0x9b87f90d )
ROM_END

ROM_START( moonwar2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "mw2.2c",       0x0000, 0x1000, 0x7c11b4d9 )
	ROM_LOAD( "mw2.2e",       0x1000, 0x1000, 0x1b6362be )
	ROM_LOAD( "mw2.2f",       0x2000, 0x1000, 0x4fd8ba4b )
	ROM_LOAD( "mw2.2h",       0x3000, 0x1000, 0x56879f0d )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "mw2.5c",       0x0000, 0x0800, 0xc26231eb )
	ROM_LOAD( "mw2.5d",       0x0800, 0x0800, 0xbb48a646 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mw2.5f",       0x0000, 0x0800, 0xc5fa1aa0 )
	ROM_LOAD( "mw2.5h",       0x0800, 0x0800, 0xa6ccc652 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "mw2.clr",      0x0000, 0x0020, 0x99614c6c )
ROM_END

ROM_START( monwar2a )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "2c",           0x0000, 0x1000, 0xbc20b734 )
	ROM_LOAD( "2e",           0x1000, 0x1000, 0xdb6ffec2 )
	ROM_LOAD( "2f",           0x2000, 0x1000, 0x378931b8 )
	ROM_LOAD( "2h",           0x3000, 0x1000, 0x031dbc2c )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "mw2.5c",       0x0000, 0x0800, 0xc26231eb )
	ROM_LOAD( "mw2.5d",       0x0800, 0x0800, 0xbb48a646 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mw2.5f",       0x0000, 0x0800, 0xc5fa1aa0 )
	ROM_LOAD( "mw2.5h",       0x0800, 0x0800, 0xa6ccc652 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "mw2.clr",      0x0000, 0x0020, 0x99614c6c )
ROM_END

ROM_START( spdcoin )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
	ROM_LOAD( "spdcoin.2c",   0x0000, 0x1000, 0x65cf1e49 )
	ROM_LOAD( "spdcoin.2e",   0x1000, 0x1000, 0x1ee59232 )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "spdcoin.5c",   0x0000, 0x0800, 0xb4cf64b7 )
	ROM_LOAD( "spdcoin.5d",   0x0800, 0x0800, 0x92304df0 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "spdcoin.5f",   0x0000, 0x0800, 0xdd5f1dbc )
	ROM_LOAD( "spdcoin.5h",   0x0800, 0x0800, 0xab1fe81b )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "spdcoin.clr",  0x0000, 0x0020, 0x1a2ccc56 )
ROM_END

ROM_START( darkplnt )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "drkplt2c.dat", 0x0000, 0x1000, 0x5a0ca559 )
	ROM_LOAD( "drkplt2e.dat", 0x1000, 0x1000, 0x52e2117d )
	ROM_LOAD( "drkplt2g.dat", 0x2000, 0x1000, 0x4093219c )
	ROM_LOAD( "drkplt2j.dat", 0x3000, 0x1000, 0xb974c78d )
	ROM_LOAD( "drkplt2k.dat", 0x4000, 0x1000, 0x71a37385 )
	ROM_LOAD( "drkplt2l.dat", 0x5000, 0x1000, 0x5ad25154 )
	ROM_LOAD( "drkplt2m.dat", 0x6000, 0x1000, 0x8d2f0122 )
	ROM_LOAD( "drkplt2p.dat", 0x7000, 0x1000, 0x2d66253b )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "5c.snd",       0x0000, 0x1000, 0x672b9454 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "drkplt5f.dat", 0x0000, 0x0800, 0x2af0ee66 )
	ROM_LOAD( "drkplt5h.dat", 0x0800, 0x0800, 0x66ef3225 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "6e.cpu",       0x0000, 0x0020, 0x86b6e124 )
ROM_END

ROM_START( tazmania )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "2c.cpu",       0x0000, 0x1000, 0x932c5a06 )
	ROM_LOAD( "2e.cpu",       0x1000, 0x1000, 0xef17ce65 )
	ROM_LOAD( "2f.cpu",       0x2000, 0x1000, 0x43c7c39d )
	ROM_LOAD( "2h.cpu",       0x3000, 0x1000, 0xbe829694 )
	ROM_LOAD( "2j.cpu",       0x4000, 0x1000, 0x6e197271 )
	ROM_LOAD( "2k.cpu",       0x5000, 0x1000, 0xa1eb453b )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "rom0.snd",     0x0000, 0x0800, 0xb8d741f1 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5f.cpu",       0x0000, 0x0800, 0x2c5b612b )
	ROM_LOAD( "5h.cpu",       0x0800, 0x0800, 0x3f5ff3ac )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "colr6f.cpu",   0x0000, 0x0020, 0xfce333c7 )
ROM_END

ROM_START( tazmani2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "2ck.cpu",      0x0000, 0x1000, 0xbf0492bf )
	ROM_LOAD( "2ek.cpu",      0x1000, 0x1000, 0x6636c4d0 )
	ROM_LOAD( "2fk.cpu",      0x2000, 0x1000, 0xce59a57b )
	ROM_LOAD( "2hk.cpu",      0x3000, 0x1000, 0x8bda3380 )
	ROM_LOAD( "2jk.cpu",      0x4000, 0x1000, 0xa4095e35 )
	ROM_LOAD( "2kk.cpu",      0x5000, 0x1000, 0xf308ca36 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "rom0.snd",     0x0000, 0x0800, 0xb8d741f1 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5f.cpu",       0x0000, 0x0800, 0x2c5b612b )
	ROM_LOAD( "5h.cpu",       0x0800, 0x0800, 0x3f5ff3ac )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "colr6f.cpu",   0x0000, 0x0020, 0xfce333c7 )
ROM_END

ROM_START( calipso )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "calipso.2c",   0x0000, 0x1000, 0x0fcb703c )
	ROM_LOAD( "calipso.2e",   0x1000, 0x1000, 0xc6622f14 )
	ROM_LOAD( "calipso.2f",   0x2000, 0x1000, 0x7bacbaba )
	ROM_LOAD( "calipso.2h",   0x3000, 0x1000, 0xa3a8111b )
	ROM_LOAD( "calipso.2j",   0x4000, 0x1000, 0xfcbd7b9e )
	ROM_LOAD( "calipso.2l",   0x5000, 0x1000, 0xf7630cab )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound code */
	ROM_LOAD( "calipso.5c",   0x0000, 0x0800, 0x9cbc65ab )
	ROM_LOAD( "calipso.5d",   0x0800, 0x0800, 0xa225ee3b )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "calipso.5f",   0x0000, 0x2000, 0xfd4252e9 )
	ROM_LOAD( "calipso.5h",   0x2000, 0x2000, 0x1663a73a )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "calipso.clr",  0x0000, 0x0020, 0x01165832 )
ROM_END

ROM_START( anteater )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "ra1-2c",       0x0000, 0x1000, 0x58bc9393 )
	ROM_LOAD( "ra1-2e",       0x1000, 0x1000, 0x574fc6f6 )
	ROM_LOAD( "ra1-2f",       0x2000, 0x1000, 0x2f7c1fe5 )
	ROM_LOAD( "ra1-2h",       0x3000, 0x1000, 0xae8a5da3 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "ra4-5c",       0x0000, 0x0800, 0x87300b4f )
	ROM_LOAD( "ra4-5d",       0x0800, 0x0800, 0xaf4e5ffe )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ra6-5f",       0x1000, 0x0800, 0x4c3f8a08 )	/* we load the roms at 0x1000-0x1fff, they */
	ROM_LOAD( "ra6-5h",       0x1800, 0x0800, 0xb30c7c9f )	/* will be decrypted at 0x0000-0x0fff */

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "colr6f.cpu",   0x0000, 0x0020, 0xfce333c7 )
ROM_END

ROM_START( rescue )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "rb15acpu.bin", 0x0000, 0x1000, 0xd7e654ba )
	ROM_LOAD( "rb15bcpu.bin", 0x1000, 0x1000, 0xa93ea158 )
	ROM_LOAD( "rb15ccpu.bin", 0x2000, 0x1000, 0x058cd3d0 )
	ROM_LOAD( "rb15dcpu.bin", 0x3000, 0x1000, 0xd6505742 )
	ROM_LOAD( "rb15ecpu.bin", 0x4000, 0x1000, 0x604df3a4 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "rb15csnd.bin", 0x0000, 0x0800, 0x8b24bf17 )
	ROM_LOAD( "rb15dsnd.bin", 0x0800, 0x0800, 0xd96e4fb3 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rb15fcpu.bin", 0x1000, 0x0800, 0x4489d20c )	/* we load the roms at 0x1000-0x1fff, they */
	ROM_LOAD( "rb15hcpu.bin", 0x1800, 0x0800, 0x5512c547 )	/* will be decrypted at 0x0000-0x0fff */

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "rescue.clr",   0x0000, 0x0020, 0x40c6bcbd )
ROM_END

ROM_START( minefld )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "ma22c",        0x0000, 0x1000, 0x1367a035 )
	ROM_LOAD( "ma22e",        0x1000, 0x1000, 0x68946d21 )
	ROM_LOAD( "ma22f",        0x2000, 0x1000, 0x7663aee5 )
	ROM_LOAD( "ma22h",        0x3000, 0x1000, 0x9787475d )
	ROM_LOAD( "ma22j",        0x4000, 0x1000, 0x2ceceb54 )
	ROM_LOAD( "ma22l",        0x5000, 0x1000, 0x85138fc9 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "ma15c",        0x0000, 0x0800, 0x8bef736b )
	ROM_LOAD( "ma15d",        0x0800, 0x0800, 0xf67b3f97 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ma15f",        0x1000, 0x0800, 0x9f703006 )	/* we load the roms at 0x1000-0x1fff, they */
	ROM_LOAD( "ma15h",        0x1800, 0x0800, 0xed0dccb1 )	/* will be decrypted at 0x0000-0x0fff */

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "minefld.clr",  0x0000, 0x0020, 0x1877368e )
ROM_END

ROM_START( losttomb )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "2c",           0x0000, 0x1000, 0xd6176d2c )
	ROM_LOAD( "2e",           0x1000, 0x1000, 0xa5f55f4a )
	ROM_LOAD( "2f",           0x2000, 0x1000, 0x0169fa3c )
	ROM_LOAD( "2h-easy",      0x3000, 0x1000, 0x054481b6 )
	ROM_LOAD( "2j",           0x4000, 0x1000, 0x249ee040 )
	ROM_LOAD( "2l",           0x5000, 0x1000, 0xc7d2e608 )
	ROM_LOAD( "2m",           0x6000, 0x1000, 0xbc4bc5b1 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "5c",           0x0000, 0x0800, 0xb899be2a )
	ROM_LOAD( "5d",           0x0800, 0x0800, 0x6907af31 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5f",           0x1000, 0x0800, 0x61f137e7 )	/* we load the roms at 0x1000-0x1fff, they */
	ROM_LOAD( "5h",           0x1800, 0x0800, 0x5581de5f )	/* will be decrypted at 0x0000-0x0fff */

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "ltprom",       0x0000, 0x0020, 0x1108b816 )
ROM_END

ROM_START( losttmbh )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "2c",           0x0000, 0x1000, 0xd6176d2c )
	ROM_LOAD( "2e",           0x1000, 0x1000, 0xa5f55f4a )
	ROM_LOAD( "2f",           0x2000, 0x1000, 0x0169fa3c )
	ROM_LOAD( "lthard",       0x3000, 0x1000, 0xe32cbf0e )
	ROM_LOAD( "2j",           0x4000, 0x1000, 0x249ee040 )
	ROM_LOAD( "2l",           0x5000, 0x1000, 0xc7d2e608 )
	ROM_LOAD( "2m",           0x6000, 0x1000, 0xbc4bc5b1 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "5c",           0x0000, 0x0800, 0xb899be2a )
	ROM_LOAD( "5d",           0x0800, 0x0800, 0x6907af31 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5f",           0x1000, 0x0800, 0x61f137e7 )	/* we load the roms at 0x1000-0x1fff, they */
	ROM_LOAD( "5h",           0x1800, 0x0800, 0x5581de5f )	/* will be decrypted at 0x0000-0x0fff */

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "ltprom",       0x0000, 0x0020, 0x1108b816 )
ROM_END

ROM_START( superbon )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "2d.cpu",       0x0000, 0x1000, 0x60c0ba18 )
	ROM_LOAD( "2e.cpu",       0x1000, 0x1000, 0xddcf44bf )
	ROM_LOAD( "2f.cpu",       0x2000, 0x1000, 0xbb66c2d5 )
	ROM_LOAD( "2h.cpu",       0x3000, 0x1000, 0x74f4f04d )
	ROM_LOAD( "2j.cpu",       0x4000, 0x1000, 0x78effb08 )
	ROM_LOAD( "2l.cpu",       0x5000, 0x1000, 0xe9dcecbd )
	ROM_LOAD( "2m.cpu",       0x6000, 0x1000, 0x3ed0337e )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "5c",  	      0x0000, 0x0800, 0xb899be2a )
	ROM_LOAD( "5d.snd",       0x0800, 0x0800, 0x80640a04 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "5f.cpu",       0x0000, 0x0800, 0x5b9d4686 )
	ROM_LOAD( "5h.cpu",       0x0800, 0x0800, 0x58c29927 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "superbon.clr", 0x0000, 0x0020, 0x00000000 )
ROM_END

ROM_START( hustler )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "hustler.1",    0x0000, 0x1000, 0x94479a3e )
	ROM_LOAD( "hustler.2",    0x1000, 0x1000, 0x3cc67bcc )
	ROM_LOAD( "hustler.3",    0x2000, 0x1000, 0x9422226a )
	/* 3000-3fff space for diagnostics ROM */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "hustler.6",    0x0000, 0x0800, 0x7a946544 )
	ROM_LOAD( "hustler.7",    0x0800, 0x0800, 0x3db57351 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hustler.5f",   0x0000, 0x0800, 0x0bdfad0e )
	ROM_LOAD( "hustler.5h",   0x0800, 0x0800, 0x8e062177 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "hustler.clr",  0x0000, 0x0020, 0xaa1f7f5e )
ROM_END

ROM_START( billiard )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "a",            0x0000, 0x1000, 0xb7eb50c0 )
	ROM_LOAD( "b",            0x1000, 0x1000, 0x988fe1c5 )
	ROM_LOAD( "c",            0x2000, 0x1000, 0x7b8de793 )
	/* 3000-3fff space for diagnostics ROM */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "hustler.6",    0x0000, 0x0800, 0x7a946544 )
	ROM_LOAD( "hustler.7",    0x0800, 0x0800, 0x3db57351 )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hustler.5f",   0x0000, 0x0800, 0x0bdfad0e )
	ROM_LOAD( "hustler.5h",   0x0800, 0x0800, 0x8e062177 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "hustler.clr",  0x0000, 0x0020, 0xaa1f7f5e )
ROM_END

/* this is identical to billiard, but with a different memory map */
ROM_START( hustlerb )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "hustler.2c",   0x0000, 0x1000, 0x3a1ac6a9 )
	ROM_LOAD( "hustler.2f",   0x1000, 0x1000, 0xdc6752ec )
	ROM_LOAD( "hustler.2j",   0x2000, 0x1000, 0x27c1e0f8 )
	/* 3000-3fff space for diagnostics ROM */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the audio CPU */
	ROM_LOAD( "hustler.11d",  0x0000, 0x0800, 0xb559bfde )
	ROM_LOAD( "hustler.10d",  0x0800, 0x0800, 0x6ef96cfb )

	ROM_REGION( 0x1000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hustler.5f",   0x0000, 0x0800, 0x0bdfad0e )
	ROM_LOAD( "hustler.5h",   0x0800, 0x0800, 0x8e062177 )

	ROM_REGION( 0x0020, REGION_PROMS )
	ROM_LOAD( "hustler.clr",  0x0000, 0x0020, 0xaa1f7f5e )
ROM_END



static void init_moonwar2(void)
{
	/* Install special handler for the spinner */
	install_mem_read_handler(0, 0x9800, 0x9800, moonwar2_IN0_r);
}


static int bit(int i,int n)
{
	return ((i >> n) & 1);
}


static void init_anteater(void)
{
	/*
	*   Code To Decode Lost Tomb by Mirko Buffoni
	*   Optimizations done by Fabio Buffoni
	*/
	int i,j;
	unsigned char *RAM;


	/* The gfx ROMs are scrambled. Decode them. They have been loaded at 0x1000, */
	/* we write them at 0x0000. */
	RAM = memory_region(REGION_GFX1);

	for (i = 0;i < 0x1000;i++)
	{
		j = i & 0x9bf;
		j |= ( bit(i,4) ^ bit(i,9) ^ ( bit(i,2) & bit(i,10) ) ) << 6;
		j |= ( bit(i,2) ^ bit(i,10) ) << 9;
		j |= ( bit(i,0) ^ bit(i,6) ^ 1 ) << 10;
		RAM[i] = RAM[j + 0x1000];
	}
}

static void init_rescue(void)
{
	/*
	*   Code To Decode Lost Tomb by Mirko Buffoni
	*   Optimizations done by Fabio Buffoni
	*/
	int i,j;
	unsigned char *RAM;


	/* The gfx ROMs are scrambled. Decode them. They have been loaded at 0x1000, */
	/* we write them at 0x0000. */
	RAM = memory_region(REGION_GFX1);

	for (i = 0;i < 0x1000;i++)
	{
		j = i & 0xa7f;
		j |= ( bit(i,3) ^ bit(i,10) ) << 7;
		j |= ( bit(i,1) ^ bit(i,7) ) << 8;
		j |= ( bit(i,0) ^ bit(i,8) ) << 10;
		RAM[i] = RAM[j + 0x1000];
	}
}

static void init_minefld(void)
{
	/*
	*   Code To Decode Minefield by Mike Balfour and Nicola Salmoria
	*/
	int i,j;
	unsigned char *RAM;


	/* The gfx ROMs are scrambled. Decode them. They have been loaded at 0x1000, */
	/* we write them at 0x0000. */
	RAM = memory_region(REGION_GFX1);

	for (i = 0;i < 0x1000;i++)
	{
		j = i & 0xd5f;
		j |= ( bit(i,3) ^ bit(i,7) ) << 5;
		j |= ( bit(i,2) ^ bit(i,9) ^ ( bit(i,0) & bit(i,5) ) ^
				( bit(i,3) & bit(i,7) & ( bit(i,0) ^ bit(i,5) ))) << 7;
		j |= ( bit(i,0) ^ bit(i,5) ^ ( bit(i,3) & bit(i,7) ) ) << 9;
		RAM[i] = RAM[j + 0x1000];
	}
}

static void init_losttomb(void)
{
	/*
	*   Code To Decode Lost Tomb by Mirko Buffoni
	*   Optimizations done by Fabio Buffoni
	*/
	int i,j;
	unsigned char *RAM;


	/* The gfx ROMs are scrambled. Decode them. They have been loaded at 0x1000, */
	/* we write them at 0x0000. */
	RAM = memory_region(REGION_GFX1);

	for (i = 0;i < 0x1000;i++)
	{
		j = i & 0xa7f;
		j |= ( (bit(i,1) & bit(i,8)) | ((1 ^ bit(i,1)) & (bit(i,10)))) << 7;
		j |= ( bit(i,7) ^ (bit(i,1) & ( bit(i,7) ^ bit(i,10) ))) << 8;
		j |= ( (bit(i,1) & bit(i,7)) | ((1 ^ bit(i,1)) & (bit(i,8)))) << 10;
		RAM[i] = RAM[j + 0x1000];
	}
}

static void init_superbon(void)
{
	/*
	*   Code rom deryption worked out by hand by Chris Hardy.
	*/
	int i;
	unsigned char *RAM;


	RAM = memory_region(REGION_CPU1);

	for (i = 0;i < 0x1000;i++)
	{
		/* Code is encrypted depending on bit 7 and 9 of the address */
		switch (i & 0x280)
		{
			case 0x000:
				RAM[i] ^= 0x92;
				break;
			case 0x080:
				RAM[i] ^= 0x82;
				break;
			case 0x200:
				RAM[i] ^= 0x12;
				break;
			case 0x280:
				RAM[i] ^= 0x10;
				break;
		}
	}
}


static void init_hustler(void)
{
	int A;


	for (A = 0;A < 0x4000;A++)
	{
		unsigned char xormask;
		int bits[8];
		int i;
		unsigned char *RAM = memory_region(REGION_CPU1);


		for (i = 0;i < 8;i++)
			bits[i] = (A >> i) & 1;

		xormask = 0xff;
		if (bits[0] ^ bits[1]) xormask ^= 0x01;
		if (bits[3] ^ bits[6]) xormask ^= 0x02;
		if (bits[4] ^ bits[5]) xormask ^= 0x04;
		if (bits[0] ^ bits[2]) xormask ^= 0x08;
		if (bits[2] ^ bits[3]) xormask ^= 0x10;
		if (bits[1] ^ bits[5]) xormask ^= 0x20;
		if (bits[0] ^ bits[7]) xormask ^= 0x40;
		if (bits[4] ^ bits[6]) xormask ^= 0x80;

		RAM[A] ^= xormask;
	}

	/* the first ROM of the second CPU has data lines D0 and D1 swapped. Decode it. */
	{
		unsigned char *RAM = memory_region(REGION_CPU2);


		for (A = 0;A < 0x0800;A++)
			RAM[A] = (RAM[A] & 0xfc) | ((RAM[A] & 1) << 1) | ((RAM[A] & 2) >> 1);
	}
}

static void init_billiard(void)
{
	int A;


	for (A = 0;A < 0x4000;A++)
	{
		unsigned char xormask;
		int bits[8];
		int i;
		unsigned char *RAM = memory_region(REGION_CPU1);


		for (i = 0;i < 8;i++)
			bits[i] = (A >> i) & 1;

		xormask = 0x55;
		if (bits[2] ^ ( bits[3] &  bits[6])) xormask ^= 0x01;
		if (bits[4] ^ ( bits[5] &  bits[7])) xormask ^= 0x02;
		if (bits[0] ^ ( bits[7] & !bits[3])) xormask ^= 0x04;
		if (bits[3] ^ (!bits[0] &  bits[2])) xormask ^= 0x08;
		if (bits[5] ^ (!bits[4] &  bits[1])) xormask ^= 0x10;
		if (bits[6] ^ (!bits[2] & !bits[5])) xormask ^= 0x20;
		if (bits[1] ^ (!bits[6] & !bits[4])) xormask ^= 0x40;
		if (bits[7] ^ (!bits[1] &  bits[0])) xormask ^= 0x80;

		RAM[A] ^= xormask;

		for (i = 0;i < 8;i++)
			bits[i] = (RAM[A] >> i) & 1;

		RAM[A] =
			(bits[7] << 0) +
			(bits[0] << 1) +
			(bits[3] << 2) +
			(bits[4] << 3) +
			(bits[5] << 4) +
			(bits[2] << 5) +
			(bits[1] << 6) +
			(bits[6] << 7);
	}

	/* the first ROM of the second CPU has data lines D0 and D1 swapped. Decode it. */
	{
		unsigned char *RAM = memory_region(REGION_CPU2);


		for (A = 0;A < 0x0800;A++)
			RAM[A] = (RAM[A] & 0xfc) | ((RAM[A] & 1) << 1) | ((RAM[A] & 2) >> 1);
	}
}



GAME( 1981, scobra,   0,        type1,    scobrak,  0,        ROT90,  "Konami", "Super Cobra" )
GAME( 1981, scobras,  scobra,   type1,    scobra,   0,        ROT90,  "[Konami] (Stern license)", "Super Cobra (Stern)" )
GAME( 1981, scobrab,  scobra,   type1,    scobra,   0,        ROT90,  "bootleg", "Super Cobra (bootleg)" )
GAME( 1981, stratgyx, 0,        stratgyx, stratgyx, 0,        ROT0,   "Konami", "Strategy X" )
GAME( 1981, stratgys, stratgyx, stratgyx, stratgyx, 0,        ROT0,   "[Konami] (Stern license)", "Strategy X (Stern)" )
GAME( 1981, armorcar, 0,        armorcar, armorcar, 0,        ROT90,  "Stern", "Armored Car (set 1)" )
GAME( 1981, armorca2, armorcar, armorcar, armorcar, 0,        ROT90,  "Stern", "Armored Car (set 2)" )
GAME( 1981, moonwar2, 0,        type1,    moonwar2, moonwar2, ROT90,  "Stern", "Moon War II (set 1)" )
GAME( 1981, monwar2a, moonwar2, type1,    monwar2a, moonwar2, ROT90,  "Stern", "Moon War II (set 2)" )
GAME( 1984, spdcoin,  0,        type1,    spdcoin,  0,        ROT90,  "Stern",    "Speed Coin (prototype)" )
GAMEX(1982, darkplnt, 0,        type2,    darkplnt, 0,        ROT180, "Stern", "Dark Planet", GAME_NOT_WORKING )
GAME( 1982, tazmania, 0,        type1,    tazmania, 0,        ROT90,  "Stern", "Tazz-Mania (Scramble hardware)" )
GAME( 1982, tazmani2, tazmania, type2,    tazmania, 0,        ROT90,  "Stern", "Tazz-Mania (Strategy X hardware)" )
GAME( 1982, calipso,  0,        calipso,  calipso,  0,        ROT90,  "[Stern] (Tago license)", "Calipso" )
GAME( 1982, anteater, 0,        type1,    anteater, anteater, ROT90,  "[Stern] (Tago license)", "Anteater" )
GAME( 1982, rescue,   0,        rescue,   rescue,   rescue,   ROT90,  "Stern", "Rescue" )
GAME( 1983, minefld,  0,        minefld,  minefld,  minefld,  ROT90,  "Stern", "Minefield" )
GAME( 1982, losttomb, 0,        type1,    losttomb, losttomb, ROT90,  "Stern", "Lost Tomb (easy)" )
GAME( 1982, losttmbh, losttomb, type1,    losttomb, losttomb, ROT90,  "Stern", "Lost Tomb (hard)" )
GAME( 1982?,superbon, 0,        type1,    superbon, superbon, ROT90,  "bootleg", "Super Bond" )
GAME( 1981, hustler,  0,        hustler,  hustler,  hustler,  ROT90,  "Konami", "Video Hustler" )
GAME( 1981, billiard, hustler,  hustler,  hustler,  billiard, ROT90,  "bootleg", "The Billiards" )
GAME( 1981, hustlerb, hustler,  hustlerb, hustler,  0,        ROT90,  "bootleg", "Video Hustler (bootleg)" )
