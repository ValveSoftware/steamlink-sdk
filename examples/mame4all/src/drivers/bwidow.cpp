/***************************************************************************

Black Widow memory map (preliminary)

0000-04ff RAM
0800      COIN_IN
0a00      IN1
0c00      IN2

2000-27ff Vector generator RAM
5000-7fff ROM

 * Black Widow cannot use the the earom routines
 * She writes into some locations at $2fac-$2fd7, which is clearly
 * the vector rom. Perhaps there is some address-logic that is not yet
 * emulated


BLACK WIDOW SWITCH SETTINGS (Atari, 1983)
-----------------------------------------

-------------------------------------------------------------------------------
Settings of 8-Toggle Switch on Black Widow CPU PCB (at D4)
 8   7   6   5   4   3   2   1   Option
-------------------------------------------------------------------------------
Off Off                          1 coin/1 credit <
On  On                           1 coin/2 credits
On  Off                          2 coins/1 credit
Off On                           Free play

        Off Off                  Right coin mechanism x 1 <
        On  Off                  Right coin mechanism x 4
        Off On                   Right coin mechanism x 5
        On  On                   Right coin mechanism x 6

                Off              Left coin mechanism x 1 <
                On               Left coin mechanism x 2

                    Off Off Off  No bonus coins (0)* <
                    Off On  On   No bonus coins (6)
                    On  On  On   No bonus coins (7)

                    On  Off Off  For every 2 coins inserted,
                                 logic adds 1 more coin (1)
                    Off On  Off  For every 4 coins inserted,
                                 logic adds 1 more coin (2)
                    On  On  Off  For every 4 coins inserted,
                                 logic adds 2 more coins (3)
                    Off Off On   For every 5 coins inserted,
                                 logic adds 1 more coin (4)
                    On  Off On   For every 3 coins inserted,
                                 logic adds 1 more coin (5)

-------------------------------------------------------------------------------

* The numbers in parentheses will appear on the BONUS ADDER line in the
  Operator Information Display (Figure 2-1) for these settings.
< Manufacturer's recommended setting

-------------------------------------------------------------------------------
Settings of 8-Toggle Switch on Black Widow CPU PCB (at B4)
 8   7   6   5   4   3   2   1   Option

Note: The bits are the exact opposite of the switch numbers - switch 8 is bit 0.
-------------------------------------------------------------------------------
Off Off                          Maximum start at level 13
On  Off                          Maximum start at level 21 <
Off On                           Maximum start at level 37
On  On                           Maximum start at level 53

        Off Off                  3 spiders per game <
        On  Off                  4 spiders per game
        Off On                   5 spiders per game
        On  On                   6 spiders per game

                Off Off          Easy game play
                On  Off          Medium game play <
                Off On           Hard game play
                On  On           Demonstration mode

                        Off Off  Bonus spider every 20,000 points <
                        On  Off  Bonus spider every 30,000 points
                        Off On   Bonus spider every 40,000 points
                        On  On   No bonus

-------------------------------------------------------------------------------

< Manufacturer's recommended setting


GRAVITAR SWITCH SETTINGS (Atari, 1982)
--------------------------------------

-------------------------------------------------------------------------------
Settings of 8-Toggle Switch on Gravitar PCB (at B4)
 8   7   6   5   4   3   2   1   Option
-------------------------------------------------------------------------------
Off On                           Free play
On  On                           1 coin for 2 credits
Off Off                          1 coin for 1 credit <
On  Off                          2 coins for 1 credit

        Off Off                  Right coin mechanism x 1 <
        On  Off                  Right coin mechanism x 4
        Off On                   Right coin mechanism x 5
        On  On                   Right coin mechanism x 6

                Off              Left coin mechanism x 1 <
                On               Left coin mechanism x 2

                    Off Off Off  No bonus coins <

                    Off On  Off  For every 4 coins inserted,
                                 logic adds 1 more coin
                    On  On  Off  For every 4 coins inserted,
                                 logic adds 2 more coins
                    Off Off On   For every 5 coins inserted,
                                 logic adds 1 more coin
                    On  Off On   For every 3 coins inserted,
                                 logic adds 1 more coin

                    Off On  On   No bonus coins
                    On  Off Off  ??? (not in manual!)
                    On  On  On   No bonus coins

-------------------------------------------------------------------------------

< Manufacturer's recommended setting

-------------------------------------------------------------------------------
Settings of 8-Toggle Switch on Gravitar PCB (at D4)
 8   7   6   5   4   3   2   1   Option
-------------------------------------------------------------------------------
                        On  On   No bonus
                        Off Off  Bonus ship every 10,000 points <
 d   d               d  On  Off  Bonus ship every 20,000 points
 e   e               e  Off On   Bonus ship every 30,000 points
 s   s               s
 U   U          On   U           Easy game play <
                Off              Hard game play
 t   t               t
 o   o  Off Off      o           3 ships per game
 N   N  On  Off      N           4 ships per game <
        Off On                   5 ships per game
        On  On                   6 ships per game

-------------------------------------------------------------------------------

< Manufacturer's recommended setting

Space Duel Settings
-------------------

(Settings of 8-Toggle Switch on Space Duel game PCB at D4)
Note: The bits are the exact opposite of the switch numbers - switch 8 is bit 0.

 8   7   6   5   4   3   2   1       Option
On  Off                         3 ships per game
Off Off                         4 ships per game $
On  On                          5 ships per game
Off On                          6 ships per game
        On  Off                *Easy game difficulty
        Off Off                 Normal game difficulty $
        On  On                  Medium game difficulty
        Off On                  Hard game difficulty
                Off Off         English $
                On  Off         German
                On  On          Spanish
                Off On          French
                                Bonus life granted every:
                        Off On  8,000 points
                        Off Off 10,000 points
                        On  Off 15,000 points
                        On  On  No bonus life

$Manufacturer's suggested settings
*Easy-In the beginning of the first wave, 3 targets appear on the
screen.  Targets increase by one in each new wave.
Normal-Space station action is the same as 'Easy'.  Fighter action has
4 targets in the beginning of the first wave.  Targets increase by 2
in each new wave.  Targets move faster and more targets enter.
Medium and Hard-In the beginning of the first wave, 4 targets appear
on the screen.  Targets increase by 2 in each new wave.  As difficulty
increases, targets move faster, and more targets enter.


(Settings of 8-Toggle Switch on Space Duel game PCB at B4)
 8   7   6   5   4   3   2   1       Option
Off On                          Free play
Off Off                        *1 coin for 1 game (or 1 player) $
On  On                          1 coin for 2 game (or 2 players)
On  Off                         2 coins for 1 game (or 1 player)
        Off Off                 Right coin mech x 1 $
        On  Off                 Right coin mech x 4
        Off On                  Right coin mech x 5
        On  On                  Right coin mech x 6
                Off             Left coin mech x 1 $
                On              Left coin mech x 2
                    Off Off Off No bonus coins $
                    Off On  Off For every 4 coins, game logic adds 1 more coin
                    On  On  Off For every 4 coins, game logic adds 2 more coin
                    Off On  On  For every 5 coins, game logic adds 1 more coin
                    On  Off On**For every 3 coins, game logic adds 1 more coin

$Manufacturer's suggested settings

**In operator Information Display, this option displays same as no bonus.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/vector.h"
#include "vidhrdw/avgdvg.h"
#include "machine/atari_vg.h"


#define IN_LEFT	(1 << 0)
#define IN_RIGHT (1 << 1)
#define IN_FIRE (1 << 2)
#define IN_SHIELD (1 << 3)
#define IN_THRUST (1 << 4)
#define IN_P1 (1 << 5)
#define IN_P2 (1 << 6)

/*

These 7 memory locations are used to read the 2 players' controls as well
as sharing some dipswitch info in the lower 4 bits pertaining to coins/credits
Typically, only the high 2 bits are read.

*/

static READ_HANDLER( spacduel_IN3_r ) {

	int res;
	int res1;
	int res2;

	res1 = readinputport(3);
	res2 = readinputport(4);
	res = 0x00;

	switch (offset & 0x07) {
		case 0:
			if (res1 & IN_SHIELD) res |= 0x80;
			if (res1 & IN_FIRE) res |= 0x40;
			break;
		case 1: /* Player 2 */
			if (res2 & IN_SHIELD) res |= 0x80;
			if (res2 & IN_FIRE) res |= 0x40;
			break;
		case 2:
			if (res1 & IN_LEFT) res |= 0x80;
			if (res1 & IN_RIGHT) res |= 0x40;
			break;
		case 3: /* Player 2 */
			if (res2 & IN_LEFT) res |= 0x80;
			if (res2 & IN_RIGHT) res |= 0x40;
			break;
		case 4:
			if (res1 & IN_THRUST) res |= 0x80;
			if (res1 & IN_P1) res |= 0x40;
			break;
		case 5:  /* Player 2 */
			if (res2 & IN_THRUST) res |= 0x80;
			break;
		case 6:
			if (res1 & IN_P2) res |= 0x80;
			break;
		case 7:
			res = (0x00 /* upright */ | (0 & 0x40));
			break;
		}
	return res;
	}

READ_HANDLER( bzone_IN0_r );

WRITE_HANDLER( bwidow_misc_w )
{
	/*
		0x10 = p1 led
		0x20 = p2 led
		0x01 = coin counter 1
		0x02 = coin counter 2
	*/
	static int lastdata;

	if (data == lastdata) return;
	osd_led_w (0, ~((data & 0x10) >> 4));
	osd_led_w (1, ~((data & 0x20) >> 5));
	coin_counter_w (0, data & 0x01);
	coin_counter_w (1, data & 0x02);
	lastdata = data;
}


static struct MemoryReadAddress bwidow_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x2000, 0x27ff, MRA_RAM },
	{ 0x2800, 0x5fff, MRA_ROM },
	{ 0x6000, 0x600f, pokey1_r },
	{ 0x6800, 0x680f, pokey2_r },
	{ 0x7000, 0x7000, atari_vg_earom_r },
	{ 0x7800, 0x7800, bzone_IN0_r },	/* IN0 */
	{ 0x8000, 0x8000, input_port_3_r },	/* IN1 */
	{ 0x8800, 0x8800, input_port_4_r },	/* IN1 */
	{ 0x9000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress bwidow_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x2000, 0x27ff, MWA_RAM, &vectorram, &vectorram_size },
	{ 0x2800, 0x5fff, MWA_ROM },
	{ 0x6000, 0x67ff, pokey1_w },
	{ 0x6800, 0x6fff, pokey2_w },
	{ 0x8800, 0x8800, bwidow_misc_w }, /* coin counters, leds */
	{ 0x8840, 0x8840, avgdvg_go_w },
	{ 0x8880, 0x8880, avgdvg_reset_w },
	{ 0x88c0, 0x88c0, MWA_NOP }, /* interrupt acknowledge */
	{ 0x8900, 0x8900, atari_vg_earom_ctrl_w },
	{ 0x8940, 0x897f, atari_vg_earom_w },
	{ 0x8980, 0x89ed, MWA_NOP }, /* watchdog clear */
	{ 0x9000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress spacduel_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x0800, 0x0800, bzone_IN0_r },	/* IN0 */
	{ 0x0900, 0x0907, spacduel_IN3_r },	/* IN1 */
	{ 0x0a00, 0x0a00, atari_vg_earom_r },
	{ 0x1000, 0x100f, pokey1_r },
	{ 0x1400, 0x140f, pokey2_r },
	{ 0x2000, 0x27ff, MRA_RAM },
	{ 0x2800, 0x3fff, MRA_ROM },
	{ 0x4000, 0x8fff, MRA_ROM },
	{ 0xf000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress spacduel_writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0905, 0x0906, MWA_NOP }, /* ignore? */
//	{ 0x0c00, 0x0c00, coin_counter_w }, /* coin out */
	{ 0x0c80, 0x0c80, avgdvg_go_w },
	{ 0x0d00, 0x0d00, MWA_NOP }, /* watchdog clear */
	{ 0x0d80, 0x0d80, avgdvg_reset_w },
	{ 0x0e00, 0x0e00, MWA_NOP }, /* interrupt acknowledge */
	{ 0x0e80, 0x0e80, atari_vg_earom_ctrl_w },
	{ 0x0f00, 0x0f3f, atari_vg_earom_w },
	{ 0x1000, 0x13ff, pokey1_w },
	{ 0x1400, 0x17ff, pokey2_w },
	{ 0x2000, 0x27ff, MWA_RAM, &vectorram, &vectorram_size },
	{ 0x2800, 0x3fff, MWA_ROM },
	{ 0x4000, 0x8fff, MWA_ROM },
	{ 0xf000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( bwidow )
	PORT_START	/* IN0 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_COIN1)
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_COIN2)
	PORT_BIT ( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_SERVICE, "Diagnostic Step", KEYCODE_F1, IP_JOY_NONE )
	/* bit 6 is the VG HALT bit. We set it to "low" */
	/* per default (busy vector processor). */
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	/* bit 7 is tied to a 3khz clock */
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME(0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING (  0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (  0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (  0x03, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (  0x02, DEF_STR( Free_Play ) )
	PORT_DIPNAME(0x0c, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING (  0x00, "*1" )
	PORT_DIPSETTING (  0x04, "*4" )
	PORT_DIPSETTING (  0x08, "*5" )
	PORT_DIPSETTING (  0x0c, "*6" )
	PORT_DIPNAME(0x10, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING (  0x00, "*1" )
	PORT_DIPSETTING (  0x10, "*2" )
	PORT_DIPNAME(0xe0, 0x00, "Bonus Coins" )
	PORT_DIPSETTING (  0x00, "None" )
	PORT_DIPSETTING (  0x20, "3 credits/2 coins" )
	PORT_DIPSETTING (  0x40, "5 credits/4 coins" )
	PORT_DIPSETTING (  0x60, "6 credits/4 coins" )
	PORT_DIPSETTING (  0x80, "6 credits/6 coins" )
	PORT_DIPSETTING (  0xa0, "4 credits/3 coins" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME(0x03, 0x01, "Max Start" )
	PORT_DIPSETTING (  0x00, "Lev 13" )
	PORT_DIPSETTING (  0x01, "Lev 21" )
	PORT_DIPSETTING (  0x02, "Lev 37" )
	PORT_DIPSETTING (  0x03, "Lev 53" )
	PORT_DIPNAME(0x0c, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING (  0x00, "3" )
	PORT_DIPSETTING (  0x04, "4" )
	PORT_DIPSETTING (  0x08, "5" )
	PORT_DIPSETTING (  0x0c, "6" )
	PORT_DIPNAME(0x30, 0x10, DEF_STR( Difficulty ) )
	PORT_DIPSETTING (  0x00, "Easy" )
	PORT_DIPSETTING (  0x10, "Medium" )
	PORT_DIPSETTING (  0x20, "Hard" )
	PORT_DIPSETTING (  0x30, "Demo" )
	PORT_DIPNAME(0xc0, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING (  0x00, "20000" )
	PORT_DIPSETTING (  0x40, "30000" )
	PORT_DIPSETTING (  0x80, "40000" )
	PORT_DIPSETTING (  0xc0, "None" )

	PORT_START	/* IN3 - Movement joystick */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP    | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN4 - Firing joystick */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END


INPUT_PORTS_START( gravitar )
	PORT_START	/* IN0 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_COIN1)
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_COIN2)
	PORT_BIT ( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_SERVICE, "Diagnostic Step", KEYCODE_F1, IP_JOY_NONE )
	/* bit 6 is the VG HALT bit. We set it to "low" */
	/* per default (busy vector processor). */
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	/* bit 7 is tied to a 3khz clock */
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_BIT( 0x03, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_DIPNAME(0x0c, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING (  0x00, "3" )
	PORT_DIPSETTING (  0x04, "4" )
	PORT_DIPSETTING (  0x08, "5" )
	PORT_DIPSETTING (  0x0c, "6" )
	PORT_DIPNAME(0x10, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING (  0x00, "Easy" )
	PORT_DIPSETTING (  0x10, "Hard" )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_DIPNAME(0xc0, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING (  0x00, "10000" )
	PORT_DIPSETTING (  0x40, "20000" )
	PORT_DIPSETTING (  0x80, "30000" )
	PORT_DIPSETTING (  0xc0, "None" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME(0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING (  0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (  0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (  0x03, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (  0x02, DEF_STR( Free_Play ) )
	PORT_DIPNAME(0x0c, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING (  0x00, "*1" )
	PORT_DIPSETTING (  0x04, "*4" )
	PORT_DIPSETTING (  0x08, "*5" )
	PORT_DIPSETTING (  0x0c, "*6" )
	PORT_DIPNAME(0x10, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING (  0x00, "*1" )
	PORT_DIPSETTING (  0x10, "*2" )
	PORT_DIPNAME(0xe0, 0x00, "Bonus Coins" )
	PORT_DIPSETTING (  0x00, "None" )
	PORT_DIPSETTING (  0x20, "3 credits/2 coins" )
	PORT_DIPSETTING (  0x40, "5 credits/4 coins" )
	PORT_DIPSETTING (  0x60, "6 credits/4 coins" )
	PORT_DIPSETTING (  0x80, "6 credits/6 coins" )
	PORT_DIPSETTING (  0xa0, "4 credits/3 coins" )

	PORT_START	/* IN3 - Player 1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_2WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN4 - Player 2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_2WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_2WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END


INPUT_PORTS_START( spacduel )
	PORT_START	/* IN0 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_COIN1)
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_COIN2)
	PORT_BIT ( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_SERVICE, "Diagnostic Step", KEYCODE_F1, IP_JOY_NONE )
	/* bit 6 is the VG HALT bit. We set it to "low" */
	/* per default (busy vector processor). */
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	/* bit 7 is tied to a 3khz clock */
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME(0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING (  0x01, "3" )
	PORT_DIPSETTING (  0x00, "4" )
	PORT_DIPSETTING (  0x03, "5" )
	PORT_DIPSETTING (  0x02, "6" )
	PORT_DIPNAME(0x0c, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING (  0x04, "Easy" )
	PORT_DIPSETTING (  0x00, "Normal" )
	PORT_DIPSETTING (  0x0c, "Medium" )
	PORT_DIPSETTING (  0x08, "Hard" )
	PORT_DIPNAME(0x30, 0x00, "Language" )
	PORT_DIPSETTING (  0x00, "English" )
	PORT_DIPSETTING (  0x10, "German" )
	PORT_DIPSETTING (  0x20, "French" )
	PORT_DIPSETTING (  0x30, "Spanish" )
	PORT_DIPNAME(0xc0, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING (  0xc0, "8000" )
	PORT_DIPSETTING (  0x00, "10000" )
	PORT_DIPSETTING (  0x40, "15000" )
	PORT_DIPSETTING (  0x80, "None" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME(0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING (  0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING (  0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING (  0x03, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING (  0x02, DEF_STR( Free_Play ) )
	PORT_DIPNAME(0x0c, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING (  0x00, "*1" )
	PORT_DIPSETTING (  0x04, "*4" )
	PORT_DIPSETTING (  0x08, "*5" )
	PORT_DIPSETTING (  0x0c, "*6" )
	PORT_DIPNAME(0x10, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING (  0x00, "*1" )
	PORT_DIPSETTING (  0x10, "*2" )
	PORT_DIPNAME(0xe0, 0x00, "Bonus Coins" )
	PORT_DIPSETTING (  0x00, "None" )
	PORT_DIPSETTING (  0x20, "3 credits/2 coins" )
	PORT_DIPSETTING (  0xa0, "4 credits/3 coins" )
	PORT_DIPSETTING (  0x40, "5 credits/4 coins" )
	PORT_DIPSETTING (  0x60, "6 credits/4 coins" )
	PORT_DIPSETTING (  0x80, "6 credits/6 coins" )

	/* See machine/spacduel.c for more info on these 2 ports */
	PORT_START	/* IN3 - Player 1 - spread over 8 memory locations */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* IN4 - Player 2 - spread over 8 memory locations */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END



static struct POKEYinterface pokey_interface =
{
	2,	/* 2 chips */
	1500000,	/* 1.5 MHz??? */
	{ 50, 50 },
	/* The 8 pot handlers */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	/* The allpot handler */
	{ input_port_1_r, input_port_2_r },
};



static struct MachineDriver machine_driver_bwidow =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1500000,	/* 1.5 Mhz */
			bwidow_readmem,bwidow_writemem,0,0,
			interrupt,4	/* 4.1ms */
		}
	},
	60, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,
	0,

	/* video hardware */
	400, 300, { 0, 480, 0, 440 },
	0,
	256, 0,
	avg_init_palette_multi,

	VIDEO_TYPE_VECTOR,
	0,
	avg_start,
	avg_stop,
	vector_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		}
	}
};

static struct MachineDriver machine_driver_gravitar =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1500000,	/* 1.5 Mhz */
			bwidow_readmem,bwidow_writemem,0,0,
			interrupt,4 /* 4.1ms */
		}
	},
	60, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,
	0,

	/* video hardware */
	400, 300, { 0, 420, 0, 400 },
	0,
	256, 0,
	avg_init_palette_multi,

	VIDEO_TYPE_VECTOR,
	0,
	avg_start,
	avg_stop,
	vector_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		}
	},

	atari_vg_earom_handler
};

static struct MachineDriver machine_driver_spacduel =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1500000,	/* 1.5 Mhz */
			spacduel_readmem,spacduel_writemem,0,0,
			interrupt,4 /* 5.4ms */
		}
	},
	45, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,
	0,

	/* video hardware */
	400, 300, { 0, 540, 0, 400 },
	0,
	256, 0,
	avg_init_palette_multi,

	VIDEO_TYPE_VECTOR,
	0,
	avg_start,
	avg_stop,
	vector_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		}
	},

	atari_vg_earom_handler
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( bwidow )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	/* Vector ROM */
	ROM_LOAD( "136017.107",   0x2800, 0x0800, 0x97f6000c )
	ROM_LOAD( "136017.108",   0x3000, 0x1000, 0x3da354ed )
	ROM_LOAD( "136017.109",   0x4000, 0x1000, 0x2fc4ce79 )
	ROM_LOAD( "136017.110",   0x5000, 0x1000, 0x0dd52987 )
	/* Program ROM */
	ROM_LOAD( "136017.101",   0x9000, 0x1000, 0xfe3febb7 )
	ROM_LOAD( "136017.102",   0xa000, 0x1000, 0x10ad0376 )
	ROM_LOAD( "136017.103",   0xb000, 0x1000, 0x8a1430ee )
	ROM_LOAD( "136017.104",   0xc000, 0x1000, 0x44f9943f )
	ROM_LOAD( "136017.105",   0xd000, 0x1000, 0xa046a2e2 )
	ROM_LOAD( "136017.106",   0xe000, 0x1000, 0x4dc28b22 )
	ROM_RELOAD(              0xf000, 0x1000 )	/* for reset/interrupt vectors */
ROM_END

ROM_START( gravitar )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	/* Vector ROM */
	ROM_LOAD( "136010.210",   0x2800, 0x0800, 0xdebcb243 )
	ROM_LOAD( "136010.207",   0x3000, 0x1000, 0x4135629a )
	ROM_LOAD( "136010.208",   0x4000, 0x1000, 0x358f25d9 )
	ROM_LOAD( "136010.309",   0x5000, 0x1000, 0x4ac78df4 )
	/* Program ROM */
	ROM_LOAD( "136010.301",   0x9000, 0x1000, 0xa2a55013 )
	ROM_LOAD( "136010.302",   0xa000, 0x1000, 0xd3700b3c )
	ROM_LOAD( "136010.303",   0xb000, 0x1000, 0x8e12e3e0 )
	ROM_LOAD( "136010.304",   0xc000, 0x1000, 0x467ad5da )
	ROM_LOAD( "136010.305",   0xd000, 0x1000, 0x840603af )
	ROM_LOAD( "136010.306",   0xe000, 0x1000, 0x3f3805ad )
	ROM_RELOAD(              0xf000, 0x1000 )	/* for reset/interrupt vectors */
ROM_END

ROM_START( gravitr2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	/* Vector ROM */
	ROM_LOAD( "136010.210",   0x2800, 0x0800, 0xdebcb243 )
	ROM_LOAD( "136010.207",   0x3000, 0x1000, 0x4135629a )
	ROM_LOAD( "136010.208",   0x4000, 0x1000, 0x358f25d9 )
	ROM_LOAD( "136010.209",   0x5000, 0x1000, 0x37034287 )
	/* Program ROM */
	ROM_LOAD( "136010.201",   0x9000, 0x1000, 0x167315e4 )
	ROM_LOAD( "136010.202",   0xa000, 0x1000, 0xaaa9e62c )
	ROM_LOAD( "136010.203",   0xb000, 0x1000, 0xae437253 )
	ROM_LOAD( "136010.204",   0xc000, 0x1000, 0x5d6bc29e )
	ROM_LOAD( "136010.205",   0xd000, 0x1000, 0x0db1ff34 )
	ROM_LOAD( "136010.206",   0xe000, 0x1000, 0x4521ca48 )
	ROM_RELOAD(              0xf000, 0x1000 )	/* for reset/interrupt vectors */
ROM_END

ROM_START( spacduel )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	/* Vector ROM */
	ROM_LOAD( "136006.106",   0x2800, 0x0800, 0x691122fe )
	ROM_LOAD( "136006.107",   0x3000, 0x1000, 0xd8dd0461 )
	/* Program ROM */
	ROM_LOAD( "136006.201",   0x4000, 0x1000, 0xf4037b6e )
	ROM_LOAD( "136006.102",   0x5000, 0x1000, 0x4c451e8a )
	ROM_LOAD( "136006.103",   0x6000, 0x1000, 0xee72da63 )
	ROM_LOAD( "136006.104",   0x7000, 0x1000, 0xe41b38a3 )
	ROM_LOAD( "136006.105",   0x8000, 0x1000, 0x5652710f )
	ROM_RELOAD(              0x9000, 0x1000 )
	ROM_RELOAD(              0xa000, 0x1000 )
	ROM_RELOAD(              0xb000, 0x1000 )
	ROM_RELOAD(              0xc000, 0x1000 )
	ROM_RELOAD(              0xd000, 0x1000 )
	ROM_RELOAD(              0xe000, 0x1000 )
	ROM_RELOAD(              0xf000, 0x1000 )	/* for reset/interrupt vectors */
ROM_END



GAME( 1982, bwidow,   0,        bwidow,   bwidow,   0, ROT0, "Atari", "Black Widow" )
GAME( 1982, gravitar, 0,        gravitar, gravitar, 0, ROT0, "Atari", "Gravitar (version 3)" )
GAME( 1982, gravitr2, gravitar, gravitar, gravitar, 0, ROT0, "Atari", "Gravitar (version 2)" )
GAME( 1980, spacduel, 0,        spacduel, spacduel, 0, ROT0, "Atari", "Space Duel" )
