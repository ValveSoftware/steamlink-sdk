#include "../vidhrdw/zaxxon.cpp"
#include "../sndhrdw/zaxxon.cpp"

/***************************************************************************

Notes:

- the duck season in razmataz is controlled by ff3c, maybe a timer. When it's
  0, it ends. We are currently returning 0xff so it never ends unitl you shoot
  stop.


Zaxxon memory map (preliminary)

0000-1fff ROM 3
2000-3fff ROM 2
4000-4fff ROM 1
6000-67ff RAM 1
6800-6fff RAM 2
8000-83ff Video RAM
a000-a0ff sprites

read:
c000      IN0
c001      IN1
c002      DSW0
c003      DSW1
c100      IN2
see the input_ports definition below for details on the input bits

write:
c000      coin A enable
c001      coin B enable
c002      coin aux enable
c003-c004 coin counters
c006      flip screen
ff3c-ff3f sound (see below)
fff0      interrupt enable
fff1      character color bank (not used during the game, but used in test mode)
fff8-fff9 background playfield position (11 bits)
fffa      background color bank (0 = standard  1 = reddish)
fffb      background enable

interrupts:
VBlank triggers IRQ, handled with interrupt mode 1
NMI enters the test mode.

Changes:
25 Jan 98 LBO
	* Added crude support for samples based on Frank's info. As of yet, I don't have
	  a set that matches the names - I need a way to edit the .wav files I have.
	  Hopefully I'll be able to create a good set shortly. I also don't know which
	  sounds "loop".
26 Jan 98 LBO
	* Fixed the sound support. I lack explosion samples and the base missile sample so
	  these are untested. I'm also unsure about the background noise. It seems to have
	  a variable volume so I've tried to reproduce that via just 1 sample.

12 Mar 98 ATJ
        * For the moment replaced Brad's version of the samples with mine from the Mame/P
          release. As yet, no variable volume, but I will be combining the features from
          Brad's driver into mine ASAP.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



READ_HANDLER( zaxxon_IN2_r );

extern unsigned char *zaxxon_char_color_bank;
extern unsigned char *zaxxon_background_position;
extern unsigned char *zaxxon_background_color_bank;
extern unsigned char *zaxxon_background_enable;
void zaxxon_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int  zaxxon_vh_start(void);
int  razmataz_vh_start(void);
void zaxxon_vh_stop(void);
void zaxxon_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void razmataz_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
extern int zaxxon_vid_type;

WRITE_HANDLER( zaxxon_sound_w );

/* in machine/segacrpt.c */
void szaxxon_decode(void);
void nprinces_decode(void);
void futspy_decode(void);



void zaxxon_init_machine(void)
{
	zaxxon_vid_type = 0;
}

void futspy_init_machine(void)
{
	zaxxon_vid_type = 2;
}

static READ_HANDLER( razmataz_unknown1_r )
{
	return rand() & 0xff;
}

static READ_HANDLER( razmataz_unknown2_r )
{
	return 0xff;
}

static int razmataz_dial_r(int num)
{
	static unsigned char pos[2];
	int delta,res;

	delta = readinputport(num);

	if (delta < 0x80)
	{
		/* right */
		pos[num] -= delta;
		res = (pos[num] << 1) | 1;
	}
	else
	{
		/* left */
		pos[num] += delta;
		res = (pos[num] << 1);
	}

	return res;
}

static READ_HANDLER( razmataz_dial_0_r )
{
	return razmataz_dial_r(0);
}

static READ_HANDLER( razmataz_dial_1_r )
{
	return razmataz_dial_r(1);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x4fff, MRA_ROM },
	{ 0x6000, 0x6fff, MRA_RAM },
	{ 0x8000, 0x83ff, MRA_RAM },
	{ 0xa000, 0xa0ff, MRA_RAM },
	{ 0xc000, 0xc000, input_port_0_r },	/* IN0 */
	{ 0xc001, 0xc001, input_port_1_r },	/* IN1 */
	{ 0xc002, 0xc002, input_port_3_r },	/* DSW0 */
	{ 0xc003, 0xc003, input_port_4_r },	/* DSW1 */
	{ 0xc100, 0xc100, input_port_2_r },	/* IN2 */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x4fff, MWA_ROM },
	{ 0x6000, 0x6fff, MWA_RAM },
	{ 0x8000, 0x83ff, videoram_w, &videoram, &videoram_size },
	{ 0xa000, 0xa0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc000, 0xc002, MWA_NOP },	/* coin enables */
	{ 0xc003, 0xc004, coin_counter_w },
	{ 0xff3c, 0xff3e, zaxxon_sound_w },
	{ 0xfff0, 0xfff0, interrupt_enable_w },
	{ 0xfff1, 0xfff1, MWA_RAM, &zaxxon_char_color_bank },
	{ 0xfff8, 0xfff9, MWA_RAM, &zaxxon_background_position },
	{ 0xfffa, 0xfffa, MWA_RAM, &zaxxon_background_color_bank },
	{ 0xfffb, 0xfffb, MWA_RAM, &zaxxon_background_enable },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress futspy_writemem[] =
{
	{ 0x0000, 0x4fff, MWA_ROM },
	{ 0x6000, 0x6fff, MWA_RAM },
	{ 0x8000, 0x83ff, videoram_w, &videoram, &videoram_size },
	{ 0xa000, 0xa0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc000, 0xc002, MWA_NOP },	/* coin enables */
	{ 0xe03c, 0xe03e, zaxxon_sound_w },
	{ 0xe0f0, 0xe0f0, interrupt_enable_w },
	{ 0xe0f1, 0xe0f1, MWA_RAM, &zaxxon_char_color_bank },
	{ 0xe0f8, 0xe0f9, MWA_RAM, &zaxxon_background_position },
	{ 0xe0fa, 0xe0fa, MWA_RAM, &zaxxon_background_color_bank },
	{ 0xe0fb, 0xe0fb, MWA_RAM, &zaxxon_background_enable },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress razmataz_readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6000, 0x6fff, MRA_RAM },
	{ 0x8000, 0x83ff, MRA_RAM },
	{ 0xa000, 0xa0ff, MRA_RAM },
	{ 0xc000, 0xc000, razmataz_dial_0_r },	/* dial pl 1 */
	{ 0xc002, 0xc002, input_port_3_r },	/* DSW0 */
	{ 0xc003, 0xc003, input_port_4_r },	/* DSW1 */
	{ 0xc004, 0xc004, input_port_6_r },	/* fire/start pl 1 */
	{ 0xc008, 0xc008, razmataz_dial_1_r },	/* dial pl 2 */
	{ 0xc00c, 0xc00c, input_port_7_r },	/* fire/start pl 2 */
	{ 0xc100, 0xc100, input_port_2_r },	/* coin */
	{ 0xc80a, 0xc80a, razmataz_unknown1_r },	/* needed, otherwise the game hangs */
	{ 0xff3c, 0xff3c, razmataz_unknown2_r },	/* timer? if 0, "duck season" ends */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress razmataz_writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x6fff, MWA_RAM },
	{ 0x8000, 0x83ff, videoram_w, &videoram, &videoram_size },
	{ 0xa000, 0xa0ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xc000, 0xc002, MWA_NOP },	/* coin enables */
	{ 0xc003, 0xc004, coin_counter_w },
	{ 0xe0f0, 0xe0f0, interrupt_enable_w },
	{ 0xe0f1, 0xe0f1, MWA_RAM, &zaxxon_char_color_bank },
	{ 0xe0f8, 0xe0f9, MWA_RAM, &zaxxon_background_position },
	{ 0xe0fa, 0xe0fa, MWA_RAM, &zaxxon_background_color_bank },
	{ 0xe0fb, 0xe0fb, MWA_RAM, &zaxxon_background_enable },
//	{ 0xff3c, 0xff3c, }, sound
	{ -1 }  /* end of table */
};


/***************************************************************************

  Zaxxon uses NMI to trigger the self test. We use a fake input port to
  tie that event to a keypress.

***************************************************************************/

static int zaxxon_interrupt(void)
{
	if (readinputport(5) & 1)	/* get status of the F2 key */
		return nmi_interrupt();	/* trigger self test */
	else return interrupt();
}

INPUT_PORTS_START( zaxxon )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )	/* the self test calls this UP */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )	/* the self test calls this DOWN */
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* button 2 - unused */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )	/* the self test calls this UP */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )	/* the self test calls this DOWN */
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* button 2 - unused */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )
	/* the coin inputs must stay active for exactly one frame, otherwise */
	/* the game will keep inserting coins. */
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_HIGH, IPT_COIN3, 1 )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x03, "10000" )
	PORT_DIPSETTING(    0x01, "20000" )
	PORT_DIPSETTING(    0x02, "30000" )
	PORT_DIPSETTING(    0x00, "40000" )
	/* The Super Zaxxon manual lists the following as unused. */
	PORT_DIPNAME( 0x0c, 0x0c, "Difficulty???" )
	PORT_DIPSETTING(    0x0c, "Easy?" )
	PORT_DIPSETTING(    0x04, "Medium?" )
	PORT_DIPSETTING(    0x08, "Hard?" )
	PORT_DIPSETTING(    0x00, "Hardest?" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x03, DEF_STR ( Coin_B ) )
	PORT_DIPSETTING(    0x0f, DEF_STR ( 4C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR ( 3C_1C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR ( 2C_1C ) )
	PORT_DIPSETTING(    0x06, "2 Coins/1 Credit 5/3 6/4" )
	PORT_DIPSETTING(    0x0a, "2 Coins/1 Credit 3/2 4/3" )
	PORT_DIPSETTING(    0x03, DEF_STR ( 1C_1C ) )
	PORT_DIPSETTING(    0x02, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(    0x0c, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0x04, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0x0d, DEF_STR ( 1C_2C ) )
	PORT_DIPSETTING(    0x08, "1 Coin/2 Credits 5/11" )
	PORT_DIPSETTING(    0x00, "1 Coin/2 Credits 4/9" )
	PORT_DIPSETTING(    0x05, DEF_STR ( 1C_3C ) )
	PORT_DIPSETTING(    0x09, DEF_STR ( 1C_4C ) )
	PORT_DIPSETTING(    0x01, DEF_STR ( 1C_5C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR ( 1C_6C ) )
	PORT_DIPNAME( 0xf0, 0x30, DEF_STR ( Coin_A ) )
	PORT_DIPSETTING(    0xf0, DEF_STR ( 4C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR ( 3C_1C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR ( 2C_1C ) )
	PORT_DIPSETTING(    0x60, "2 Coins/1 Credit 5/3 6/4" )
	PORT_DIPSETTING(    0xa0, "2 Coins/1 Credit 3/2 4/3" )
	PORT_DIPSETTING(    0x30, DEF_STR ( 1C_1C ) )
	PORT_DIPSETTING(    0x20, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(    0xc0, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0x40, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0xd0, DEF_STR ( 1C_2C ) )
	PORT_DIPSETTING(    0x80, "1 Coin/2 Credits 5/11" )
	PORT_DIPSETTING(    0x00, "1 Coin/2 Credits 4/9" )
	PORT_DIPSETTING(    0x50, DEF_STR ( 1C_3C ) )
	PORT_DIPSETTING(    0x90, DEF_STR ( 1C_4C ) )
	PORT_DIPSETTING(    0x10, DEF_STR ( 1C_5C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR ( 1C_6C ) )

	PORT_START	/* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
INPUT_PORTS_END

INPUT_PORTS_START( futspy )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )	/* the self test calls this UP */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )	/* the self test calls this DOWN */
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )	/* the self test calls this UP */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )	/* the self test calls this DOWN */
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )
	/* the coin inputs must stay active for exactly one frame, otherwise */
	/* the game will keep inserting coins. */
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_HIGH, IPT_COIN3, 1 )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x00, DEF_STR ( Coin_A ) )
	PORT_DIPSETTING(    0x08, DEF_STR ( 4C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR ( 3C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR ( 2C_1C ) )
	PORT_DIPSETTING(    0x0a, "2 Coins/1 Credit 5/3 6/4" )
	PORT_DIPSETTING(    0x0b, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(    0x00, DEF_STR ( 1C_1C ) )
	PORT_DIPSETTING(    0x0e, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0x0d, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0x0c, "1 Coin/1 Credit 11/12" )
	PORT_DIPSETTING(    0x09, DEF_STR ( 2C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR ( 1C_2C ) )
	PORT_DIPSETTING(    0x0f, "1 Coin/2 Credits 5/11" )
	PORT_DIPSETTING(    0x02, DEF_STR ( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR ( 1C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR ( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR ( 1C_6C ) )
	PORT_DIPNAME( 0xf0, 0x00, DEF_STR ( Coin_B ) )
	PORT_DIPSETTING(    0x80, DEF_STR ( 4C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR ( 3C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR ( 2C_1C ) )
	PORT_DIPSETTING(    0xa0, "2 Coins/1 Credit 5/3 6/4" )
	PORT_DIPSETTING(    0xb0, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(    0x00, DEF_STR ( 1C_1C ) )
	PORT_DIPSETTING(    0xe0, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0xd0, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0xc0, "1 Coin/1 Credit 11/12" )
	PORT_DIPSETTING(    0x90, DEF_STR ( 2C_3C ) )
	PORT_DIPSETTING(    0x10, DEF_STR ( 1C_2C ) )
	PORT_DIPSETTING(    0xf0, "1 Coin/2 Credits 5/11" )
	PORT_DIPSETTING(    0x20, DEF_STR ( 1C_3C ) )
	PORT_DIPSETTING(    0x30, DEF_STR ( 1C_4C ) )
	PORT_DIPSETTING(    0x40, DEF_STR ( 1C_5C ) )
	PORT_DIPSETTING(    0x50, DEF_STR ( 1C_6C ) )


	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x08, "5" )
	PORT_BITX( 0,       0x0c, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20k 40k 60k" )
	PORT_DIPSETTING(    0x10, "30k 60k 90k" )
	PORT_DIPSETTING(    0x20, "40k 70k 100k" )
	PORT_DIPSETTING(    0x30, "40k 80k 120k" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x40, "Medium" )
	PORT_DIPSETTING(    0x80, "Hard" )
	PORT_DIPSETTING(    0xc0, "Hardest" )

	PORT_START	/* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )
INPUT_PORTS_END

INPUT_PORTS_START( razmataz )
	PORT_START	/* IN0 */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_CENTER | IPF_PLAYER1, 30, 15, 0, 0 )

	PORT_START	/* IN1 */
	PORT_ANALOG( 0xff, 0x00, IPT_DIAL | IPF_CENTER | IPF_PLAYER2 | IPF_REVERSE, 30, 15, 0, 0 )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	/* the coin inputs must stay active for exactly one frame, otherwise */
	/* the game will keep inserting coins. */
	PORT_BIT_IMPULSE( 0x20, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x40, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_HIGH, IPT_COIN3, 1 )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x10, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x30, "5" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x07, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR ( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR ( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR ( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR ( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR ( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR ( 1C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR ( 1C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR ( 1C_5C ) )
	PORT_DIPNAME( 0x38, 0x18, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR ( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR ( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR ( 2C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR ( 1C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR ( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR ( 1C_3C ) )
	PORT_DIPSETTING(    0x30, DEF_STR ( 1C_4C ) )
	PORT_DIPSETTING(    0x38, DEF_STR ( 1C_5C ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* FAKE */
	/* This fake input port is used to get the status of the F2 key, */
	/* and activate the test mode, which is triggered by a NMI */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE )

	PORT_START	/* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN4 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END



struct GfxLayout zaxxon_charlayout1 =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	3,	/* 3 bits per pixel (actually 2, the third plane is 0) */
	{ 2*256*8*8, 256*8*8, 0 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

struct GfxLayout zaxxon_charlayout2 =
{
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	3,	/* 3 bits per pixel */
	{ 2*1024*8*8, 1024*8*8, 0 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	32,32,	/* 32*32 sprites */
	64,	/* 64 sprites */
	3,	/* 3 bits per pixel */
	{ 2*64*128*8, 64*128*8, 0 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 24*8+4, 24*8+5, 24*8+6, 24*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8,
			64*8, 65*8, 66*8, 67*8, 68*8, 69*8, 70*8, 71*8,
			96*8, 97*8, 98*8, 99*8, 100*8, 101*8, 102*8, 103*8 },
	128*8	/* every sprite takes 128 consecutive bytes */
};

static struct GfxLayout futspy_spritelayout =
{
	32,32,	/* 32*32 sprites */
	128,	/* 128 sprites */
	3,	/* 3 bits per pixel */
	{ 2*128*128*8, 128*128*8, 0 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 24*8+4, 24*8+5, 24*8+6, 24*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8,
			64*8, 65*8, 66*8, 67*8, 68*8, 69*8, 70*8, 71*8,
			96*8, 97*8, 98*8, 99*8, 100*8, 101*8, 102*8, 103*8 },
	128*8	/* every sprite takes 128 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &zaxxon_charlayout1,   0, 32 },	/* characters */
	{ REGION_GFX2, 0, &zaxxon_charlayout2,   0, 32 },	/* background tiles */
	{ REGION_GFX3, 0, &spritelayout,  0, 32 },			/* sprites */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo futspy_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &zaxxon_charlayout1,   0, 32 },	/* characters */
	{ REGION_GFX2, 0, &zaxxon_charlayout2,   0, 32 },	/* background tiles */
	{ REGION_GFX3, 0, &futspy_spritelayout,  0, 32 },	/* sprites */
	{ -1 } /* end of array */
};



static const char *zaxxon_sample_names[] =
{
	"*zaxxon",
	"03.wav",	/* Homing Missile */
	"02.wav",	/* Base Missile */
	"01.wav",	/* Laser (force field) */
	"00.wav",	/* Battleship (end of level boss) */
	"11.wav",	/* S-Exp (enemy explosion) */
	"10.wav",	/* M-Exp (ship explosion) */
	"08.wav", 	/* Cannon (ship fire) */
	"23.wav",	/* Shot (enemy fire) */
	"21.wav",	/* Alarm 2 (target lock) */
	"20.wav",	/* Alarm 3 (low fuel) */
	"05.wav",	/* initial background noise */
	"04.wav",	/* looped asteroid noise */
	0
};

static struct Samplesinterface zaxxon_samples_interface =
{
	12,	/* 12 channels */
	25,	/* volume */
	zaxxon_sample_names
};


static struct MachineDriver machine_driver_zaxxon =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz ?? */
			readmem,writemem,0,0,
			zaxxon_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	zaxxon_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256,32*8,
	zaxxon_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	zaxxon_vh_start,
	zaxxon_vh_stop,
	zaxxon_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SAMPLES,
			&zaxxon_samples_interface
		}
	}
};

static struct MachineDriver machine_driver_futspy =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz ?? */
			readmem,futspy_writemem,0,0,
			zaxxon_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	futspy_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	futspy_gfxdecodeinfo,
	256,32*8,
	zaxxon_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	zaxxon_vh_start,
	zaxxon_vh_stop,
	zaxxon_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SAMPLES,
			&zaxxon_samples_interface
		}
	}
};

static struct MachineDriver machine_driver_razmataz =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz ?? */
			razmataz_readmem,razmataz_writemem,0,0,
			zaxxon_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	zaxxon_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256,32*8,
	zaxxon_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	razmataz_vh_start,
	zaxxon_vh_stop,
	razmataz_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( zaxxon )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "zaxxon.3",     0x0000, 0x2000, 0x6e2b4a30 )
	ROM_LOAD( "zaxxon.2",     0x2000, 0x2000, 0x1c9ea398 )
	ROM_LOAD( "zaxxon.1",     0x4000, 0x1000, 0x1c123ef9 )

	ROM_REGION( 0x1800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zaxxon.14",    0x0000, 0x0800, 0x07bf8c52 )	/* characters */
	ROM_LOAD( "zaxxon.15",    0x0800, 0x0800, 0xc215edcb )
	/* 1000-17ff empty space to convert the characters as 3bpp instead of 2 */

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zaxxon.6",     0x0000, 0x2000, 0x6e07bb68 )	/* background tiles */
	ROM_LOAD( "zaxxon.5",     0x2000, 0x2000, 0x0a5bce6a )
	ROM_LOAD( "zaxxon.4",     0x4000, 0x2000, 0xa5bf1465 )

	ROM_REGION( 0x6000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zaxxon.11",    0x0000, 0x2000, 0xeaf0dd4b )	/* sprites */
	ROM_LOAD( "zaxxon.12",    0x2000, 0x2000, 0x1c5369c7 )
	ROM_LOAD( "zaxxon.13",    0x4000, 0x2000, 0xab4e8a9a )

	ROM_REGION( 0x8000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* background tilemaps converted in vh_start */
	ROM_LOAD( "zaxxon.8",     0x0000, 0x2000, 0x28d65063 )
	ROM_LOAD( "zaxxon.7",     0x2000, 0x2000, 0x6284c200 )
	ROM_LOAD( "zaxxon.10",    0x4000, 0x2000, 0xa95e61fd )
	ROM_LOAD( "zaxxon.9",     0x6000, 0x2000, 0x7e42691f )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "zaxxon.u98",   0x0000, 0x0100, 0x6cc6695b ) /* palette */
	ROM_LOAD( "zaxxon.u72",   0x0100, 0x0100, 0xdeaa21f7 ) /* char lookup table */
ROM_END

ROM_START( zaxxon2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "3a",           0x0000, 0x2000, 0xb18e428a )
	ROM_LOAD( "zaxxon.2",     0x2000, 0x2000, 0x1c9ea398 )
	ROM_LOAD( "1a",           0x4000, 0x1000, 0x1977d933 )

	ROM_REGION( 0x1800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zaxxon.14",    0x0000, 0x0800, 0x07bf8c52 )	/* characters */
	ROM_LOAD( "zaxxon.15",    0x0800, 0x0800, 0xc215edcb )
	/* 1000-17ff empty space to convert the characters as 3bpp instead of 2 */

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zaxxon.6",     0x0000, 0x2000, 0x6e07bb68 )	/* background tiles */
	ROM_LOAD( "zaxxon.5",     0x2000, 0x2000, 0x0a5bce6a )
	ROM_LOAD( "zaxxon.4",     0x4000, 0x2000, 0xa5bf1465 )

	ROM_REGION( 0x6000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zaxxon.11",    0x0000, 0x2000, 0xeaf0dd4b )	/* sprites */
	ROM_LOAD( "zaxxon.12",    0x2000, 0x2000, 0x1c5369c7 )
	ROM_LOAD( "zaxxon.13",    0x4000, 0x2000, 0xab4e8a9a )

	ROM_REGION( 0x8000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* background tilemaps converted in vh_start */
	ROM_LOAD( "zaxxon.8",     0x0000, 0x2000, 0x28d65063 )
	ROM_LOAD( "zaxxon.7",     0x2000, 0x2000, 0x6284c200 )
	ROM_LOAD( "zaxxon.10",    0x4000, 0x2000, 0xa95e61fd )
	ROM_LOAD( "zaxxon.9",     0x6000, 0x2000, 0x7e42691f )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "zaxxon.u98",   0x0000, 0x0100, 0x6cc6695b ) /* palette */
	ROM_LOAD( "j214a2.72",    0x0100, 0x0100, 0xa9e1fb43 ) /* char lookup table */
ROM_END

ROM_START( zaxxonb )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "zaxxonb.3",    0x0000, 0x2000, 0x125bca1c )
	ROM_LOAD( "zaxxonb.2",    0x2000, 0x2000, 0xc088df92 )
	ROM_LOAD( "zaxxonb.1",    0x4000, 0x1000, 0xe7bdc417 )

	ROM_REGION( 0x1800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zaxxon.14",    0x0000, 0x0800, 0x07bf8c52 )	/* characters */
	ROM_LOAD( "zaxxon.15",    0x0800, 0x0800, 0xc215edcb )
	/* 1000-17ff empty space to convert the characters as 3bpp instead of 2 */

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zaxxon.6",     0x0000, 0x2000, 0x6e07bb68 )	/* background tiles */
	ROM_LOAD( "zaxxon.5",     0x2000, 0x2000, 0x0a5bce6a )
	ROM_LOAD( "zaxxon.4",     0x4000, 0x2000, 0xa5bf1465 )

	ROM_REGION( 0x6000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "zaxxon.11",    0x0000, 0x2000, 0xeaf0dd4b )	/* sprites */
	ROM_LOAD( "zaxxon.12",    0x2000, 0x2000, 0x1c5369c7 )
	ROM_LOAD( "zaxxon.13",    0x4000, 0x2000, 0xab4e8a9a )

	ROM_REGION( 0x8000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* background tilemaps converted in vh_start */
	ROM_LOAD( "zaxxon.8",     0x0000, 0x2000, 0x28d65063 )
	ROM_LOAD( "zaxxon.7",     0x2000, 0x2000, 0x6284c200 )
	ROM_LOAD( "zaxxon.10",    0x4000, 0x2000, 0xa95e61fd )
	ROM_LOAD( "zaxxon.9",     0x6000, 0x2000, 0x7e42691f )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "zaxxon.u98",   0x0000, 0x0100, 0x6cc6695b ) /* palette */
	ROM_LOAD( "zaxxon.u72",   0x0100, 0x0100, 0xdeaa21f7 ) /* char lookup table */
ROM_END

ROM_START( szaxxon )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "suzaxxon.3",   0x0000, 0x2000, 0xaf7221da )
	ROM_LOAD( "suzaxxon.2",   0x2000, 0x2000, 0x1b90fb2a )
	ROM_LOAD( "suzaxxon.1",   0x4000, 0x1000, 0x07258b4a )

	ROM_REGION( 0x1800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "suzaxxon.14",  0x0000, 0x0800, 0xbccf560c )	/* characters */
	ROM_LOAD( "suzaxxon.15",  0x0800, 0x0800, 0xd28c628b )
	/* 1000-17ff empty space to convert the characters as 3bpp instead of 2 */

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "suzaxxon.6",   0x0000, 0x2000, 0xf51af375 )	/* background tiles */
	ROM_LOAD( "suzaxxon.5",   0x2000, 0x2000, 0xa7de021d )
	ROM_LOAD( "suzaxxon.4",   0x4000, 0x2000, 0x5bfb3b04 )

	ROM_REGION( 0x6000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "suzaxxon.11",  0x0000, 0x2000, 0x1503ae41 )	/* sprites */
	ROM_LOAD( "suzaxxon.12",  0x2000, 0x2000, 0x3b53d83f )
	ROM_LOAD( "suzaxxon.13",  0x4000, 0x2000, 0x581e8793 )

	ROM_REGION( 0x8000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* background tilemaps converted in vh_start */
	ROM_LOAD( "suzaxxon.8",   0x0000, 0x2000, 0xdd1b52df )
	ROM_LOAD( "suzaxxon.7",   0x2000, 0x2000, 0xb5bc07f0 )
	ROM_LOAD( "suzaxxon.10",  0x4000, 0x2000, 0x68e84174 )
	ROM_LOAD( "suzaxxon.9",   0x6000, 0x2000, 0xa509994b )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "suzaxxon.u98", 0x0000, 0x0100, 0x15727a9f ) /* palette */
	ROM_LOAD( "suzaxxon.u72", 0x0100, 0x0100, 0xdeaa21f7 ) /* char lookup table */
ROM_END

ROM_START( futspy )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "fs_snd.u27",   0x0000, 0x2000, 0x7578fe7f )
	ROM_LOAD( "fs_snd.u28",   0x2000, 0x2000, 0x8ade203c )
	ROM_LOAD( "fs_snd.u29",   0x4000, 0x1000, 0x734299c3 )

	ROM_REGION( 0x1800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fs_snd.u68",   0x0000, 0x0800, 0x305fae2d )	/* characters */
	ROM_LOAD( "fs_snd.u69",   0x0800, 0x0800, 0x3c5658c0 )
	/* 1000-17ff empty space to convert the characters as 3bpp instead of 2 */

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fs_vid.113",   0x0000, 0x2000, 0x36d2bdf6 )	/* background tiles */
	ROM_LOAD( "fs_vid.112",   0x2000, 0x2000, 0x3740946a )
	ROM_LOAD( "fs_vid.111",   0x4000, 0x2000, 0x4cd4df98 )

	ROM_REGION( 0xc000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fs_vid.u77",   0x0000, 0x4000, 0x1b93c9ec )	/* sprites */
	ROM_LOAD( "fs_vid.u78",   0x4000, 0x4000, 0x50e55262 )
	ROM_LOAD( "fs_vid.u79",   0x8000, 0x4000, 0xbfb02e3e )

	ROM_REGION( 0x8000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* background tilemaps converted in vh_start */
	ROM_LOAD( "fs_vid.u91",   0x0000, 0x2000, 0x86da01f4 )
	ROM_LOAD( "fs_vid.u90",   0x2000, 0x2000, 0x2bd41d2d )
	ROM_LOAD( "fs_vid.u93",   0x4000, 0x2000, 0xb82b4997 )
	ROM_LOAD( "fs_vid.u92",   0x6000, 0x2000, 0xaf4015af )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "futrprom.u98", 0x0000, 0x0100, 0x9ba2acaa ) /* palette */
	ROM_LOAD( "futrprom.u72", 0x0100, 0x0100, 0xf9e26790 ) /* char lookup table */
ROM_END

ROM_START( razmataz )
	ROM_REGION( 2*0x10000, REGION_CPU1 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "u27",           0x0000, 0x2000, 0x254f350f )
	ROM_LOAD( "u28",           0x2000, 0x2000, 0x3a1eaa99 )
	ROM_LOAD( "u29",           0x4000, 0x2000, 0x0ee67e78 )

	ROM_REGION( 0x1800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1921.u68",      0x0000, 0x0800, 0x77f8ff5a )  /* characters */
	ROM_LOAD( "1922.u69",      0x0800, 0x0800, 0xcf63621e )
	/* 1000-17ff empty space to convert the characters as 3bpp instead of 2 */

	ROM_REGION( 0x6000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1934.113",      0x0000, 0x2000, 0x39bb679c )  /* background tiles */
	ROM_LOAD( "1933.112",      0x2000, 0x2000, 0x1022185e )
	ROM_LOAD( "1932.111",      0x4000, 0x2000, 0xc7a715eb )

	ROM_REGION( 0x6000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1925.u77",      0x0000, 0x2000, 0xa7965437 )  /* sprites */
	ROM_LOAD( "1926.u78",      0x2000, 0x2000, 0x9a3af434 )
	ROM_LOAD( "1927.u79",      0x4000, 0x2000, 0x0323de2b )

	ROM_REGION( 0x8000, REGION_GFX4 | REGIONFLAG_DISPOSE )	/* background tilemaps converted in vh_start */
	ROM_LOAD( "1929.u91",      0x0000, 0x2000, 0x55c7c757 )
	ROM_LOAD( "1928.u90",      0x2000, 0x2000, 0xe58b155b )
	ROM_LOAD( "1931.u93",      0x4000, 0x2000, 0x55fe0f82 )
	ROM_LOAD( "1930.u92",      0x6000, 0x2000, 0xf355f105 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "clr.u98",       0x0000, 0x0100, 0x0fd671af )	/* palette */
	ROM_LOAD( "clr.u72",       0x0100, 0x0100, 0x03233bc5 ) /* char lookup table */

	ROM_REGION( 0x1000, REGION_SOUND1 )	/* sound? */
	ROM_LOAD( "1923.u50",      0x0000, 0x0800, 0x59994a51 )
	ROM_LOAD( "1924.u51",      0x0800, 0x0800, 0xa75e0011 )
ROM_END



static void init_zaxxonb(void)
{
/*
	the values vary, but the translation mask is always laid out like this:

	  0 1 2 3 4 5 6 7 8 9 a b c d e f
	0 A A B B A A B B C C D D C C D D
	1 A A B B A A B B C C D D C C D D
	2 E E F F E E F F G G H H G G H H
	3 E E F F E E F F G G H H G G H H
	4 A A B B A A B B C C D D C C D D
	5 A A B B A A B B C C D D C C D D
	6 E E F F E E F F G G H H G G H H
	7 E E F F E E F F G G H H G G H H
	8 H H G G H H G G F F E E F F E E
	9 H H G G H H G G F F E E F F E E
	a D D C C D D C C B B A A B B A A
	b D D C C D D C C B B A A B B A A
	c H H G G H H G G F F E E F F E E
	d H H G G H H G G F F E E F F E E
	e D D C C D D C C B B A A B B A A
	f D D C C D D C C B B A A B B A A

	(e.g. 0xc0 is XORed with H)
	therefore in the following tables we only keep track of A, B, C, D, E, F, G and H.
*/
	static const unsigned char data_xortable[2][8] =
	{
		{ 0x0a,0x0a,0x22,0x22,0xaa,0xaa,0x82,0x82 },	/* ...............0 */
		{ 0xa0,0xaa,0x28,0x22,0xa0,0xaa,0x28,0x22 },	/* ...............1 */
	};
	static const unsigned char opcode_xortable[8][8] =
	{
		{ 0x8a,0x8a,0x02,0x02,0x8a,0x8a,0x02,0x02 },	/* .......0...0...0 */
		{ 0x80,0x80,0x08,0x08,0xa8,0xa8,0x20,0x20 },	/* .......0...0...1 */
		{ 0x8a,0x8a,0x02,0x02,0x8a,0x8a,0x02,0x02 },	/* .......0...1...0 */
		{ 0x02,0x08,0x2a,0x20,0x20,0x2a,0x08,0x02 },	/* .......0...1...1 */
		{ 0x88,0x0a,0x88,0x0a,0xaa,0x28,0xaa,0x28 },	/* .......1...0...0 */
		{ 0x80,0x80,0x08,0x08,0xa8,0xa8,0x20,0x20 },	/* .......1...0...1 */
		{ 0x88,0x0a,0x88,0x0a,0xaa,0x28,0xaa,0x28 },	/* .......1...1...0 */
		{ 0x02,0x08,0x2a,0x20,0x20,0x2a,0x08,0x02 } 	/* .......1...1...1 */
	};
	int A;
	unsigned char *rom = memory_region(REGION_CPU1);
	int diff = memory_region_length(REGION_CPU1) / 2;


	memory_set_opcode_base(0,rom+diff);

	for (A = 0x0000;A < 0x8000;A++)
	{
		int i,j;
		unsigned char src;


		src = rom[A];

		/* pick the translation table from bit 0 of the address */
		i = A & 1;

		/* pick the offset in the table from bits 1, 3 and 5 of the source data */
		j = ((src >> 1) & 1) + (((src >> 3) & 1) << 1) + (((src >> 5) & 1) << 2);
		/* the bottom half of the translation table is the mirror image of the top */
		if (src & 0x80) j = 7 - j;

		/* decode the ROM data */
		rom[A] = src ^ data_xortable[i][j];

		/* now decode the opcodes */
		/* pick the translation table from bits 0, 4, and 8 of the address */
		i = ((A >> 0) & 1) + (((A >> 4) & 1) << 1) + (((A >> 8) & 1) << 2);
		rom[A + diff] = src ^ opcode_xortable[i][j];
	}
}

static void init_szaxxon(void)
{
	szaxxon_decode();
}

static void init_futspy(void)
{
	futspy_decode();
}

static void init_razmataz(void)
{
	nprinces_decode();
}


GAMEX( 1982, zaxxon,   0,      zaxxon,   zaxxon,   0,        ROT90,  "Sega",    "Zaxxon (set 1)", GAME_NO_COCKTAIL )
GAMEX( 1982, zaxxon2,  zaxxon, zaxxon,   zaxxon,   0,        ROT90,  "Sega",    "Zaxxon (set 2)", GAME_NO_COCKTAIL )
GAMEX( 1982, zaxxonb,  zaxxon, zaxxon,   zaxxon,   zaxxonb,  ROT90,  "bootleg", "Jackson", GAME_NO_COCKTAIL )
GAMEX( 1982, szaxxon,  0,      zaxxon,   zaxxon,   szaxxon,  ROT90,  "Sega",    "Super Zaxxon", GAME_NO_COCKTAIL )
GAMEX( 1984, futspy,   0,      futspy,   futspy,   futspy,   ROT270, "Sega",    "Future Spy", GAME_NO_COCKTAIL )
GAMEX( 1983, razmataz, 0,      razmataz, razmataz, razmataz, ROT270, "Sega",    "Razzmatazz", GAME_NO_SOUND | GAME_NO_COCKTAIL )
