#include "../machine/asteroid.cpp"
#include "../sndhrdw/asteroid.cpp"

/***************************************************************************

Asteroids Memory Map (preliminary)

Asteroids settings:

0 = OFF  1 = ON  X = Don't Care  $ = Atari suggests


8 SWITCH DIP
87654321
--------
XXXXXX11   English
XXXXXX10   German
XXXXXX01   French
XXXXXX00   Spanish
XXXXX1XX   4-ship game
XXXXX0XX   3-ship game
11XXXXXX   Free Play
10XXXXXX   1 Coin  for 2 Plays
01XXXXXX   1 Coin  for 1 Play
00XXXXXX   2 Coins for 1 Play

Asteroids Deluxe settings:

0 = OFF  1 = ON  X = Don't Care  $ = Atari suggests


8 SWITCH DIP (R5)
87654321
--------
XXXXXX11   English $
XXXXXX10   German
XXXXXX01   French
XXXXXX00   Spanish
XXXX11XX   2-4 ships
XXXX10XX   3-5 ships $
XXXX01XX   4-6 ships
XXXX00XX   5-7 ships
XXX1XXXX   1-play minimum $
XXX0XXXX   2-play minimum
XX1XXXXX   Easier gameplay for first 30000 points +
XX0XXXXX   Hard gameplay throughout the game      +
11XXXXXX   Bonus ship every 10,000 points $ !
10XXXXXX   Bonus ship every 12,000 points !
01XXXXXX   Bonus ship every 15,000 points !
00XXXXXX   No bonus ships (adds one ship at game start)

+ only with the newer romset
! not "every", but "at", e.g. only once.

Thanks to Gregg Woodcock for the info.

8 SWITCH DIP (L8)
87654321
--------
XXXXXX11   Free Play
XXXXXX10   1 Coin = 2 Plays
XXXXXX01   1 Coin = 1 Play
XXXXXX00   2 Coins = 1 Play $
XXXX11XX   Right coin mech * 1 $
XXXX10XX   Right coin mech * 4
XXXX01XX   Right coin mech * 5
XXXX00XX   Right coin mech * 6
XXX1XXXX   Center coin mech * 1 $
XXX0XXXX   Center coin mech * 2
111XXXXX   No bonus coins
110XXXXX   For every 2 coins inserted, game logic adds 1 more coin
101XXXXX   For every 4 coins inserted, game logic adds 1 more coin
100XXXXX   For every 4 coins inserted, game logic adds 2 more coins $
011XXXXX   For every 5 coins inserted, game logic adds 1 more coin
***************************************************************************/

/***************************************************************************

Lunar Lander Memory Map (preliminary)

Lunar Lander settings:

0 = OFF  1 = ON  x = Don't Care  $ = Atari suggests


8 SWITCH DIP (P8) with -01 ROMs on PCB
87654321
--------
11xxxxxx   450 fuel units per coin
10xxxxxx   600 fuel units per coin
01xxxxxx   750 fuel units per coin  $
00xxxxxx   900 fuel units per coin
xxx0xxxx   Free play
xxx1xxxx   Coined play as determined by toggles 7 & 8  $
xxxx00xx   German instructions
xxxx01xx   Spanish instructions
xxxx10xx   French instructions
xxxx11xx   English instructions  $
xxxxxx11   Right coin == 1 credit/coin  $
xxxxxx10   Right coin == 4 credit/coin
xxxxxx01   Right coin == 5 credit/coin
xxxxxx00   Right coin == 6 credit/coin
           (Left coin always registers 1 credit/coin)


8 SWITCH DIP (P8) with -02 ROMs on PCB
87654321
--------
11x1xxxx   450 fuel units per coin
10x1xxxx   600 fuel units per coin
01x1xxxx   750 fuel units per coin  $
00x1xxxx   900 fuel units per coin
11x0xxxx   1100 fuel units per coin
10x0xxxx   1300 fuel units per coin
01x0xxxx   1550 fuel units per coin
00x0xxxx   1800 fuel units per coin
xx0xxxxx   Free play
xx1xxxxx   Coined play as determined by toggles 5, 7, & 8  $
xxxx00xx   German instructions
xxxx01xx   Spanish instructions
xxxx10xx   French instructions
xxxx11xx   English instructions  $
xxxxxx11   Right coin == 1 credit/coin  $
xxxxxx10   Right coin == 4 credit/coin
xxxxxx01   Right coin == 5 credit/coin
xxxxxx00   Right coin == 6 credit/coin
           (Left coin always registers 1 credit/coin)

Notes:

Known issues:

* Sound emu isn't perfect - sometimes explosions don't register in Asteroids
* The low background thrust in Lunar Lander isn't emulated
* Asteroids Deluxe and Lunar Lander both toggle the LEDs too frequently to be effectively emulated
* The ERROR message in Asteroids Deluxe self test is related to a pokey problem
***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/vector.h"
#include "vidhrdw/avgdvg.h"
#include "machine/atari_vg.h"

void asteroid_init_machine(void);
int asteroid_interrupt(void);
int llander_interrupt(void);

WRITE_HANDLER( asteroid_bank_switch_w );
WRITE_HANDLER( astdelux_bank_switch_w );
WRITE_HANDLER( astdelux_led_w );
WRITE_HANDLER( llander_led_w );

WRITE_HANDLER( asteroid_explode_w );
WRITE_HANDLER( asteroid_thump_w );
WRITE_HANDLER( asteroid_sounds_w );
int asteroid_sh_start(const struct MachineSound *msound);
void asteroid_sh_stop(void);
void asteroid_sh_update(void);

WRITE_HANDLER( astdelux_sounds_w );
int astdelux_sh_start(const struct MachineSound *msound);
void astdelux_sh_stop(void);
void astdelux_sh_update(void);

WRITE_HANDLER( llander_sounds_w );
WRITE_HANDLER( llander_snd_reset_w );
int llander_sh_start(const struct MachineSound *msound);
void llander_sh_stop(void);
void llander_sh_update(void);

READ_HANDLER( asteroid_IN0_r );
READ_HANDLER( asteroib_IN0_r );
READ_HANDLER( asteroid_IN1_r );
READ_HANDLER( asteroid_DSW1_r );
READ_HANDLER( llander_IN0_r );

void llander_init_colors (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int llander_start(void);
void llander_stop(void);
void llander_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



/* Lunar Lander mirrors page 0 and page 1. */
static unsigned char *llander_zeropage;

static READ_HANDLER( llander_zeropage_r )
{
	return llander_zeropage[offset & 0xff];
}

static WRITE_HANDLER( llander_zeropage_w )
{
	llander_zeropage[offset & 0xff] = data;
}



static struct MemoryReadAddress asteroid_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x2000, 0x2007, asteroid_IN0_r }, /* IN0 */
	{ 0x2400, 0x2407, asteroid_IN1_r }, /* IN1 */
	{ 0x2800, 0x2803, asteroid_DSW1_r }, /* DSW1 */
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x5000, 0x57ff, MRA_ROM }, /* vector rom */
	{ 0x6800, 0x7fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM }, /* for the reset / interrupt vectors */
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress asteroib_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x2000, 0x2000, asteroib_IN0_r }, /* IN0 */
	{ 0x2003, 0x2003, input_port_3_r }, /* hyperspace */
	{ 0x2400, 0x2407, asteroid_IN1_r }, /* IN1 */
	{ 0x2800, 0x2803, asteroid_DSW1_r }, /* DSW1 */
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x5000, 0x57ff, MRA_ROM }, /* vector rom */
	{ 0x6800, 0x7fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM }, /* for the reset / interrupt vectors */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress asteroid_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x3000, 0x3000, avgdvg_go_w },
	{ 0x3200, 0x3200, asteroid_bank_switch_w },
	{ 0x3400, 0x3400, watchdog_reset_w },
	{ 0x3600, 0x3600, asteroid_explode_w },
	{ 0x3a00, 0x3a00, asteroid_thump_w },
	{ 0x3c00, 0x3c05, asteroid_sounds_w },
	{ 0x4000, 0x47ff, MWA_RAM, &vectorram, &vectorram_size },
	{ 0x5000, 0x57ff, MWA_ROM }, /* vector rom */
	{ 0x6800, 0x7fff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress astdelux_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x2000, 0x2007, asteroid_IN0_r }, /* IN0 */
	{ 0x2400, 0x2407, asteroid_IN1_r }, /* IN1 */
	{ 0x2800, 0x2803, asteroid_DSW1_r }, /* DSW1 */
	{ 0x2c00, 0x2c0f, pokey1_r },
	{ 0x2c40, 0x2c7f, atari_vg_earom_r },
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x4800, 0x57ff, MRA_ROM }, /* vector rom */
	{ 0x6000, 0x7fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM }, /* for the reset / interrupt vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress astdelux_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x2405, 0x2405, astdelux_sounds_w }, /* thrust sound */
	{ 0x2c00, 0x2c0f, pokey1_w },
	{ 0x3000, 0x3000, avgdvg_go_w },
	{ 0x3200, 0x323f, atari_vg_earom_w },
	{ 0x3400, 0x3400, watchdog_reset_w },
	{ 0x3600, 0x3600, asteroid_explode_w },
	{ 0x3a00, 0x3a00, atari_vg_earom_ctrl_w },
/*	{ 0x3c00, 0x3c03, astdelux_led_w },*/ /* P1 LED, P2 LED, unknown, thrust? */
	{ 0x3c00, 0x3c03, MWA_NOP }, /* P1 LED, P2 LED, unknown, thrust? */
	{ 0x3c04, 0x3c04, astdelux_bank_switch_w },
	{ 0x3c05, 0x3c07, coin_counter_w },
	{ 0x4000, 0x47ff, MWA_RAM, &vectorram, &vectorram_size },
	{ 0x4800, 0x57ff, MWA_ROM }, /* vector rom */
	{ 0x6000, 0x7fff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress llander_readmem[] =
{
	{ 0x0000, 0x01ff, llander_zeropage_r },
	{ 0x2000, 0x2000, llander_IN0_r }, /* IN0 */
	{ 0x2400, 0x2407, asteroid_IN1_r }, /* IN1 */
	{ 0x2800, 0x2803, asteroid_DSW1_r }, /* DSW1 */
	{ 0x2c00, 0x2c00, input_port_3_r }, /* IN3 */
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0x4800, 0x5fff, MRA_ROM }, /* vector rom */
	{ 0x6000, 0x7fff, MRA_ROM },
	{ 0xf800, 0xffff, MRA_ROM }, /* for the reset / interrupt vectors */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress llander_writemem[] =
{
	{ 0x0000, 0x01ff, llander_zeropage_w, &llander_zeropage },
	{ 0x3000, 0x3000, avgdvg_go_w },
	{ 0x3200, 0x3200, llander_led_w },
	{ 0x3400, 0x3400, watchdog_reset_w },
	{ 0x3c00, 0x3c00, llander_sounds_w },
	{ 0x3e00, 0x3e00, llander_snd_reset_w },
	{ 0x4000, 0x47ff, MWA_RAM, &vectorram, &vectorram_size },
	{ 0x4800, 0x5fff, MWA_ROM }, /* vector rom */
	{ 0x6000, 0x7fff, MWA_ROM },
	{ -1 }  /* end of table */
};

INPUT_PORTS_START( asteroid )
	PORT_START /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	/* Bit 2 and 3 are handled in the machine dependent part. */
        /* Bit 2 is the 3 KHz source and Bit 3 the VG_HALT bit    */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BITX(0x20, IP_ACTIVE_HIGH, IPT_SERVICE, "Diagnostic Step", KEYCODE_F1, IP_JOY_NONE )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_2WAY )

	PORT_START /* DSW1 */
	PORT_DIPNAME( 0x03, 0x00, "Language" )
	PORT_DIPSETTING (   0x00, "English" )
	PORT_DIPSETTING (   0x01, "German" )
	PORT_DIPSETTING (   0x02, "French" )
	PORT_DIPSETTING (   0x03, "Spanish" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING (   0x04, "3" )
	PORT_DIPSETTING (   0x00, "4" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING (   0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING (   0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING (   0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Coinage ) )
	PORT_DIPSETTING (   0xc0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (   0x80, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (   0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Free_Play ) )
INPUT_PORTS_END

INPUT_PORTS_START( asteroib )
	PORT_START /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* resets */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* resets */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	/* Bit 7 is VG_HALT, handled in the machine dependant part */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_2WAY )

	PORT_START /* DSW1 */
	PORT_DIPNAME( 0x03, 0x00, "Language" )
	PORT_DIPSETTING (   0x00, "English" )
	PORT_DIPSETTING (   0x01, "German" )
	PORT_DIPSETTING (   0x02, "French" )
	PORT_DIPSETTING (   0x03, "Spanish" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING (   0x04, "3" )
	PORT_DIPSETTING (   0x00, "4" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING (   0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING (   0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Off ) )
	PORT_DIPSETTING (   0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Coinage ) )
	PORT_DIPSETTING (   0xc0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (   0x80, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (   0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (   0x00, DEF_STR( Free_Play ) )

	PORT_START /* hyperspace */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 )
INPUT_PORTS_END

INPUT_PORTS_START( astdelux )
	PORT_START /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	/* Bit 2 and 3 are handled in the machine dependent part. */
	/* Bit 2 is the 3 KHz source and Bit 3 the VG_HALT bit    */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BITX( 0x20, IP_ACTIVE_HIGH, IPT_SERVICE, "Diagnostic Step", KEYCODE_F1, IP_JOY_NONE )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_TILT )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_2WAY )

	PORT_START /* DSW 1 */
	PORT_DIPNAME( 0x03, 0x00, "Language" )
	PORT_DIPSETTING (   0x00, "English" )
	PORT_DIPSETTING (   0x01, "German" )
	PORT_DIPSETTING (   0x02, "French" )
	PORT_DIPSETTING (   0x03, "Spanish" )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING (   0x00, "2-4" )
	PORT_DIPSETTING (   0x04, "3-5" )
	PORT_DIPSETTING (   0x08, "4-6" )
	PORT_DIPSETTING (   0x0c, "5-7" )
	PORT_DIPNAME( 0x10, 0x00, "Minimum plays" )
	PORT_DIPSETTING (   0x00, "1" )
	PORT_DIPSETTING (   0x10, "2" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING (   0x00, "Hard" )
	PORT_DIPSETTING (   0x20, "Easy" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING (   0x00, "10000" )
	PORT_DIPSETTING (   0x40, "12000" )
	PORT_DIPSETTING (   0x80, "15000" )
	PORT_DIPSETTING (   0xc0, "None" )

	PORT_START /* DSW 2 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Coinage ) )
	PORT_DIPSETTING (   0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (   0x01, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (   0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (   0x03, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x0c, "Right Coin" )
	PORT_DIPSETTING (   0x00, "*6" )
	PORT_DIPSETTING (   0x04, "*5" )
	PORT_DIPSETTING (   0x08, "*4" )
	PORT_DIPSETTING (   0x0c, "*1" )
	PORT_DIPNAME( 0x10, 0x10, "Center Coin" )
	PORT_DIPSETTING (   0x00, "*2" )
	PORT_DIPSETTING (   0x10, "*1" )
	PORT_DIPNAME( 0xe0, 0x80, "Bonus Coins" )
	PORT_DIPSETTING (   0x60, "1 each 5" )
	PORT_DIPSETTING (   0x80, "2 each 4" )
	PORT_DIPSETTING (   0xa0, "1 each 4" )
	PORT_DIPSETTING (   0xc0, "1 each 2" )
	PORT_DIPSETTING (   0xe0, "None" )
INPUT_PORTS_END

INPUT_PORTS_START( llander )
	PORT_START /* IN0 */
	/* Bit 0 is VG_HALT, handled in the machine dependant part */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_SERVICE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	/* Of the rest, Bit 6 is the 3KHz source. 3,4 and 5 are unknown */
	PORT_BIT( 0x78, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_SERVICE, "Diagnostic Step", KEYCODE_F1, IP_JOY_NONE )

	PORT_START /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BITX(0x10, IP_ACTIVE_HIGH, IPT_START2, "Select Game", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x20, IP_ACTIVE_HIGH, IPT_BUTTON1, "Abort", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )

	PORT_START /* DSW1 */
	PORT_DIPNAME( 0x03, 0x01, "Right Coin" )
	PORT_DIPSETTING (   0x00, "*1" )
	PORT_DIPSETTING (   0x01, "*4" )
	PORT_DIPSETTING (   0x02, "*5" )
	PORT_DIPSETTING (   0x03, "*6" )
	PORT_DIPNAME( 0x0c, 0x00, "Language" )
	PORT_DIPSETTING (   0x00, "English" )
	PORT_DIPSETTING (   0x04, "French" )
	PORT_DIPSETTING (   0x08, "Spanish" )
	PORT_DIPSETTING (   0x0c, "German" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING (   0x00, "Normal" )
	PORT_DIPSETTING (   0x20, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xd0, 0x80, "Fuel units" )
	PORT_DIPSETTING (   0x00, "450" )
	PORT_DIPSETTING (   0x40, "600" )
	PORT_DIPSETTING (   0x80, "750" )
	PORT_DIPSETTING (   0xc0, "900" )
	PORT_DIPSETTING (   0x10, "1100" )
	PORT_DIPSETTING (   0x50, "1300" )
	PORT_DIPSETTING (   0x90, "1550" )
	PORT_DIPSETTING (   0xd0, "1800" )

	/* The next one is a potentiometer */
	PORT_START /* IN3 */
	PORT_ANALOGX( 0xff, 0x00, IPT_PADDLE|IPF_REVERSE, 100, 10, 0, 255, KEYCODE_UP, KEYCODE_DOWN, JOYCODE_1_UP, JOYCODE_1_DOWN )
INPUT_PORTS_END

INPUT_PORTS_START( llander1 )
	PORT_START /* IN0 */
	/* Bit 0 is VG_HALT, handled in the machine dependant part */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_SERVICE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	/* Of the rest, Bit 6 is the 3KHz source. 3,4 and 5 are unknown */
	PORT_BIT( 0x78, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_SERVICE, "Diagnostic Step", KEYCODE_F1, IP_JOY_NONE )

	PORT_START /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BITX(0x10, IP_ACTIVE_HIGH, IPT_START2, "Select Game", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BITX(0x20, IP_ACTIVE_HIGH, IPT_BUTTON1, "Abort", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_2WAY )

	PORT_START /* DSW1 */
	PORT_DIPNAME( 0x03, 0x01, "Right Coin" )
	PORT_DIPSETTING (   0x00, "*1" )
	PORT_DIPSETTING (   0x01, "*4" )
	PORT_DIPSETTING (   0x02, "*5" )
	PORT_DIPSETTING (   0x03, "*6" )
	PORT_DIPNAME( 0x0c, 0x00, "Language" )
	PORT_DIPSETTING (   0x00, "English" )
	PORT_DIPSETTING (   0x04, "French" )
	PORT_DIPSETTING (   0x08, "Spanish" )
	PORT_DIPSETTING (   0x0c, "German" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING (   0x00, "Normal" )
	PORT_DIPSETTING (   0x10, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xc0, 0x80, "Fuel units" )
	PORT_DIPSETTING (   0x00, "450" )
	PORT_DIPSETTING (   0x40, "600" )
	PORT_DIPSETTING (   0x80, "750" )
	PORT_DIPSETTING (   0xc0, "900" )

	/* The next one is a potentiometer */
	PORT_START /* IN3 */
	PORT_ANALOGX( 0xff, 0x00, IPT_PADDLE|IPF_REVERSE, 100, 10, 0, 255, KEYCODE_UP, KEYCODE_DOWN, JOYCODE_1_UP, JOYCODE_1_DOWN )
INPUT_PORTS_END



/*static void asteroid1_hisave(void)
{
	void *f;
	unsigned char *RAM = memory_region(REGION_CPU1);


	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
	{
		osd_fwrite(f,&RAM[0x001c],2*10+3*11);
		osd_fclose(f);
	}
}

static void asteroid_hisave(void)
{
	void *f;
	unsigned char *RAM = memory_region(REGION_CPU1);


	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
	{
		osd_fwrite(f,&RAM[0x001d],2*10+3*11);
		osd_fclose(f);
	}
}*/



/* Asteroids Deluxe now uses the earom routines
 * However, we keep the highscore location, just in case
 *		osd_fwrite(f,&RAM[0x0023],3*10+3*11);
 */

static struct CustomSound_interface asteroid_custom_interface = {
	asteroid_sh_start,
	asteroid_sh_stop,
	asteroid_sh_update
};

static struct MachineDriver machine_driver_asteroid =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1500000,	/* 1.5 Mhz */
			asteroid_readmem,asteroid_writemem,0,0,
			asteroid_interrupt,4	/* 250 Hz */
		}
	},
	60, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,
	asteroid_init_machine,

	/* video hardware */
	400, 300, { 0, 1040, 70, 950 },
	0,
	256+32768, 0,
	avg_init_palette_white,

	VIDEO_TYPE_VECTOR,
	0,
	dvg_start,
	dvg_stop,
	vector_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&asteroid_custom_interface
		}
	}
};

static struct MachineDriver machine_driver_asteroib =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1500000,	/* 1.5 Mhz */
			asteroib_readmem,asteroid_writemem,0,0,
			asteroid_interrupt,4	/* 250 Hz */
		}
	},
	60, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,
	asteroid_init_machine,

	/* video hardware */
	400, 300, { 0, 1040, 70, 950 },
	0,
	256+32768, 0,
	avg_init_palette_white,

	VIDEO_TYPE_VECTOR,
	0,
	dvg_start,
	dvg_stop,
	vector_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&asteroid_custom_interface
		}
	}
};



static struct POKEYinterface pokey_interface =
{
	1,	/* 1 chip */
	1500000,	/* 1.5 MHz??? */
	{ 100 },
	/* The 8 pot handlers */
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	/* The allpot handler */
	{ input_port_3_r }
};

static struct CustomSound_interface astdelux_custom_interface = {
	astdelux_sh_start,
	astdelux_sh_stop,
	astdelux_sh_update
};

static struct MachineDriver machine_driver_astdelux =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1500000,	/* 1.5 Mhz */
			astdelux_readmem,astdelux_writemem,0,0,
			asteroid_interrupt,4	/* 250 Hz */
		}
	},
	60, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,
	0,

	/* video hardware */
	400, 300, { 0, 1040, 70, 950 },
	0,
	256+32768, 0,
	avg_init_palette_astdelux,

	VIDEO_TYPE_VECTOR,
	0,
	dvg_start,
	dvg_stop,
	vector_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		},
		{
			SOUND_CUSTOM,
			&astdelux_custom_interface
		}
	},

	atari_vg_earom_handler
};


static struct CustomSound_interface llander_custom_interface =
{
	llander_sh_start,
	llander_sh_stop,
	llander_sh_update
};


static struct MachineDriver machine_driver_llander =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1500000,			/* 1.5 Mhz */
			llander_readmem, llander_writemem,0,0,
			llander_interrupt,6	/* 250 Hz */
		}
	},
	40, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,
	0,

	/* video hardware */
	400, 300, { 0, 1050, 0, 900 },
	0,
	256+32768, 0,
	llander_init_colors,

	VIDEO_TYPE_VECTOR,
	0,
	llander_start,
	llander_stop,
	llander_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_CUSTOM,
			&llander_custom_interface
		}
	}
};

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( asteroid )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "035145.02",    0x6800, 0x0800, 0x0cc75459 )
	ROM_LOAD( "035144.02",    0x7000, 0x0800, 0x096ed35c )
	ROM_LOAD( "035143.02",    0x7800, 0x0800, 0x312caa02 )
	ROM_RELOAD(            0xf800, 0x0800 )	/* for reset/interrupt vectors */
	/* Vector ROM */
	ROM_LOAD( "035127.02",    0x5000, 0x0800, 0x8b71fd9e )
ROM_END

ROM_START( asteroi1 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "035145.01",    0x6800, 0x0800, 0xe9bfda64 )
	ROM_LOAD( "035144.01",    0x7000, 0x0800, 0xe53c28a9 )
	ROM_LOAD( "035143.01",    0x7800, 0x0800, 0x7d4e3d05 )
	ROM_RELOAD(            0xf800, 0x0800 )	/* for reset/interrupt vectors */
	/* Vector ROM */
	ROM_LOAD( "035127.01",    0x5000, 0x0800, 0x99699366 )
ROM_END

ROM_START( asteroib )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "035145ll.bin", 0x6800, 0x0800, 0x605fc0f2 )
	ROM_LOAD( "035144ll.bin", 0x7000, 0x0800, 0xe106de77 )
	ROM_LOAD( "035143ll.bin", 0x7800, 0x0800, 0x6b1d8594 )
	ROM_RELOAD(            0xf800, 0x0800 )	/* for reset/interrupt vectors */
	/* Vector ROM */
	ROM_LOAD( "035127.02",    0x5000, 0x0800, 0x8b71fd9e )
ROM_END

ROM_START( astdelux )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "036430.02",    0x6000, 0x0800, 0xa4d7a525 )
	ROM_LOAD( "036431.02",    0x6800, 0x0800, 0xd4004aae )
	ROM_LOAD( "036432.02",    0x7000, 0x0800, 0x6d720c41 )
	ROM_LOAD( "036433.03",    0x7800, 0x0800, 0x0dcc0be6 )
	ROM_RELOAD(               0xf800, 0x0800 )	/* for reset/interrupt vectors */
	/* Vector ROM */
	ROM_LOAD( "036800.02",    0x4800, 0x0800, 0xbb8cabe1 )
	ROM_LOAD( "036799.01",    0x5000, 0x0800, 0x7d511572 )
ROM_END

ROM_START( astdelu1 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "036430.01",    0x6000, 0x0800, 0x8f5dabc6 )
	ROM_LOAD( "036431.01",    0x6800, 0x0800, 0x157a8516 )
	ROM_LOAD( "036432.01",    0x7000, 0x0800, 0xfdea913c )
	ROM_LOAD( "036433.02",    0x7800, 0x0800, 0xd8db74e3 )
	ROM_RELOAD(               0xf800, 0x0800 )	/* for reset/interrupt vectors */
	/* Vector ROM */
	ROM_LOAD( "036800.01",    0x4800, 0x0800, 0x3b597407 )
	ROM_LOAD( "036799.01",    0x5000, 0x0800, 0x7d511572 )
ROM_END

ROM_START( llander )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "034572.02",    0x6000, 0x0800, 0xb8763eea )
	ROM_LOAD( "034571.02",    0x6800, 0x0800, 0x77da4b2f )
	ROM_LOAD( "034570.01",    0x7000, 0x0800, 0x2724e591 )
	ROM_LOAD( "034569.02",    0x7800, 0x0800, 0x72837a4e )
	ROM_RELOAD(            0xf800, 0x0800 )	/* for reset/interrupt vectors */
	/* Vector ROM */
	ROM_LOAD( "034599.01",    0x4800, 0x0800, 0x355a9371 )
	ROM_LOAD( "034598.01",    0x5000, 0x0800, 0x9c4ffa68 )
	/* This _should_ be the rom for international versions. */
	/* Unfortunately, is it not currently available. */
	ROM_LOAD( "034597.01",    0x5800, 0x0800, 0x00000000 )
ROM_END

ROM_START( llander1 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "034572.01",    0x6000, 0x0800, 0x2aff3140 )
	ROM_LOAD( "034571.01",    0x6800, 0x0800, 0x493e24b7 )
	ROM_LOAD( "034570.01",    0x7000, 0x0800, 0x2724e591 )
	ROM_LOAD( "034569.01",    0x7800, 0x0800, 0xb11a7d01 )
	ROM_RELOAD(            0xf800, 0x0800 )	/* for reset/interrupt vectors */
	/* Vector ROM */
	ROM_LOAD( "034599.01",    0x4800, 0x0800, 0x355a9371 )
	ROM_LOAD( "034598.01",    0x5000, 0x0800, 0x9c4ffa68 )
	/* This _should_ be the rom for international versions. */
	/* Unfortunately, is it not currently available. */
	ROM_LOAD( "034597.01",    0x5800, 0x0800, 0x00000000 )
ROM_END



GAME( 1979, asteroid, 0,        asteroid, asteroid, 0, ROT0, "Atari", "Asteroids (rev 2)" )
GAME( 1979, asteroi1, asteroid, asteroid, asteroid, 0, ROT0, "Atari", "Asteroids (rev 1)" )
GAME( 1979, asteroib, asteroid, asteroib, asteroib, 0, ROT0, "bootleg", "Asteroids (bootleg on Lunar Lander hardware)" )
GAME( 1980, astdelux, 0,        astdelux, astdelux, 0, ROT0, "Atari", "Asteroids Deluxe (rev 2)" )
GAME( 1980, astdelu1, astdelux, astdelux, astdelux, 0, ROT0, "Atari", "Asteroids Deluxe (rev 1)" )
GAME( 1979, llander,  0,        llander,  llander,  0, ROT0, "Atari", "Lunar Lander (rev 2)" )
GAME( 1979, llander1, llander,  llander,  llander1, 0, ROT0, "Atari", "Lunar Lander (rev 1)" )
