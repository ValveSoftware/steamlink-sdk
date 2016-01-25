/***************************************************************************

     TEMPEST
     -------
     HEX        R/W   D7 D6 D5 D4 D3 D2 D2 D0  function
     0000-07FF  R/W   D  D  D  D  D  D  D  D   program ram (2K)
     0800-080F   W                D  D  D  D   Colour ram

     0C00        R                         D   Right coin sw
     0C00        R                      D      Center coin sw
     0C00        R                   D         Left coin sw
     0C00        R                D            Slam sw
     0C00        R             D               Self test sw
     0C00        R          D                  Diagnostic step sw
     0C00        R       D                     Halt
     0C00        R    D                        3khz ??
     0D00        R    D  D  D  D  D  D  D  D   option switches
     0E00        R    D  D  D  D  D  D  D  D   option switches

     2000-2FFF  R/W   D  D  D  D  D  D  D  D   Vector Ram (4K)
     3000-3FFF   R    D  D  D  D  D  D  D  D   Vector Rom (4K)

     4000        W                         D   Right coin counter
     4000        W                      D      left  coin counter
     4000        W                D            Video invert - x
     4000        W             D               Video invert - y
     4800        W                             Vector generator GO

     5000        W                             WD clear
     5800        W                             Vect gen reset

     6000-603F   W    D  D  D  D  D  D  D  D   EAROM write
     6040        W    D  D  D  D  D  D  D  D   EAROM control
     6040        R    D                        Mathbox status
     6050        R    D  D  D  D  D  D  D  D   EAROM read

     6060        R    D  D  D  D  D  D  D  D   Mathbox read
     6070        R    D  D  D  D  D  D  D  D   Mathbox read
     6080-609F   W    D  D  D  D  D  D  D  D   Mathbox start

     60C0-60CF  R/W   D  D  D  D  D  D  D  D   Custom audio chip 1
     60D0-60DF  R/W   D  D  D  D  D  D  D  D   Custom audio chip 2

     60E0        R                         D   one player start LED
     60E0        R                      D      two player start LED
     60E0        R                   D         FLIP

     9000-DFFF  R     D  D  D  D  D  D  D  D   Program ROM (20K)

     notes: program ram decode may be incorrect, but it appears like
     this on the schematics, and the troubleshooting guide.

     ZAP1,FIRE1,FIRE2,ZAP2 go to pokey2 , bits 3,and 4
     (depending on state of FLIP)
     player1 start, player2 start are pokey2 , bits 5 and 6

     encoder wheel goes to pokey1 bits 0-3
     pokey1, bit4 is cocktail detect


TEMPEST SWITCH SETTINGS (Atari, 1980)
-------------------------------------


GAME OPTIONS:
(8-position switch at L12 on Analog Vector-Generator PCB)

1   2   3   4   5   6   7   8   Meaning
-------------------------------------------------------------------------
Off Off                         2 lives per game
On  On                          3 lives per game
On  Off                         4 lives per game
Off On                          5 lives per game
        On  On  Off             Bonus life every 10000 pts
        On  On  On              Bonus life every 20000 pts
        On  Off On              Bonus life every 30000 pts
        On  Off Off             Bonus life every 40000 pts
        Off On  On              Bonus life every 50000 pts
        Off On  Off             Bonus life every 60000 pts
        Off Off On              Bonus life every 70000 pts
        Off Off Off             No bonus lives
                    On  On      English
                    On  Off     French
                    Off On      German
                    Off Off     Spanish
                            On  1-credit minimum
                            Off 2-credit minimum


GAME OPTIONS:
(4-position switch at D/E2 on Math Box PCB)

1   2   3   4                   Meaning
-------------------------------------------------------------------------
    Off                         Minimum rating range: 1, 3, 5, 7, 9
    On                          Minimum rating range tied to high score
        Off Off                 Medium difficulty (see notes)
        Off On                  Easy difficulty (see notes)
        On  Off                 Hard difficulty (see notes)
        On  On                  Medium difficulty (see notes)


PRICING OPTIONS:
(8-position switch at N13 on Analog Vector-Generator PCB)

1   2   3   4   5   6   7   8   Meaning
-------------------------------------------------------------------------
On  On  On                      No bonus coins
On  On  Off                     For every 2 coins, game adds 1 more coin
On  Off On                      For every 4 coins, game adds 1 more coin
On  Off Off                     For every 4 coins, game adds 2 more coins
Off On  On                      For every 5 coins, game adds 1 more coin
Off On  Off                     For every 3 coins, game adds 1 more coin
On  Off                 Off On  Demonstration Mode (see notes)
Off Off                 Off On  Demonstration-Freeze Mode (see notes)
            On                  Left coin mech * 1
            Off                 Left coin mech * 2
                On  On          Right coin mech * 1
                On  Off         Right coin mech * 4
                Off On          Right coin mech * 5
                Off Off         Right coin mech * 6
                        Off On  Free Play
                        Off Off 1 coin 2 plays
                        On  On  1 coin 1 play
                        On  Off 2 coins 1 play


GAME SETTING NOTES:
-------------------

Demonstration Mode:
- Plays a normal game of Tempest, but pressing SUPERZAP sends you
  directly to the next level.

Demonstration-Freeze Mode:
- Just like Demonstration Mode, but with frozen screen action.

Both Demonstration Modes:
- Pressing RESET in either mode will cause the game to lock up.
  To recover, set switch 1 to On.
- You can start at any level from 1..81, so it's an easy way of
  seeing what the game can throw at you
- The score is zeroed at the end of the game, so you also don't
  have to worry about artificially high scores disrupting your
  scoring records as stored in the game's EAROM.

Easy Difficulty:
- Enemies move more slowly
- One less enemy shot on the screen at any given time

Hard Difficulty:
- Enemies move more quickly
- 1-4 more enemy shots on the screen at any given time
- One more enemy may be on the screen at any given time

High Scores:
- Changing toggles 1-5 at L12 (more/fewer lives, bonus ship levels)
  will erase the high score table.
- You should also wait 8-10 seconds after a game has been played
  before entering self-test mode or powering down; otherwise, you
  might erase or corrupt the high score table.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/mathbox.h"
#include "vidhrdw/avgdvg.h"
#include "vidhrdw/vector.h"
#include "machine/atari_vg.h"


static READ_HANDLER( tempest_IN0_r )
{
	int res;

	res = readinputport(0);

	if (avgdvg_done())
		res|=0x40;

	/* Emulate the 3Khz source on bit 7 (divide 1.5Mhz by 512) */
	if (cpu_gettotalcycles() & 0x100)
		res |=0x80;

	return res;
}

static WRITE_HANDLER( tempest_led_w )
{
	osd_led_w (0, ~(data >> 1));
	osd_led_w (1, ~data);
	/* FLIP is bit 0x04 */
}

static WRITE_HANDLER( tempest_coin_w )
{
	static int lastval;

	if (lastval == data) return;
	coin_counter_w (0, (data & 0x01));
	coin_counter_w (1, (data & 0x02));
	coin_counter_w (2, (data & 0x04));
	lastval = data;
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x0c00, 0x0c00, tempest_IN0_r },	/* IN0 */
	{ 0x0d00, 0x0d00, input_port_3_r },	/* DSW1 */
	{ 0x0e00, 0x0e00, input_port_4_r },	/* DSW2 */
	{ 0x2000, 0x2fff, MRA_RAM },
	{ 0x3000, 0x3fff, MRA_ROM },
	{ 0x6040, 0x6040, mb_status_r },
	{ 0x6050, 0x6050, atari_vg_earom_r },
	{ 0x6060, 0x6060, mb_lo_r },
	{ 0x6070, 0x6070, mb_hi_r },
	{ 0x60c0, 0x60cf, pokey1_r },
	{ 0x60d0, 0x60df, pokey2_r },
	{ 0x9000, 0xdfff, MRA_ROM },
	{ 0xf000, 0xffff, MRA_ROM },	/* for the reset / interrupt vectors */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM },
	{ 0x0800, 0x080f, tempest_colorram_w },
	{ 0x2000, 0x2fff, MWA_RAM, &vectorram, &vectorram_size },
	{ 0x3000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x4000, tempest_coin_w },
	{ 0x4800, 0x4800, avgdvg_go_w },
	{ 0x5000, 0x5000, watchdog_reset_w },
	{ 0x5800, 0x5800, avgdvg_reset_w },
	{ 0x6000, 0x603f, atari_vg_earom_w },
	{ 0x6040, 0x6040, atari_vg_earom_ctrl_w },
	{ 0x6080, 0x609f, mb_go_w },
	{ 0x60c0, 0x60cf, pokey1_w },
	{ 0x60d0, 0x60df, pokey2_w },
	{ 0x60e0, 0x60e0, tempest_led_w },
	{ 0x9000, 0xdfff, MWA_ROM },
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( tempest )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_TILT )
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BITX( 0x20, IP_ACTIVE_LOW, IPT_SERVICE, "Diagnostic Step", KEYCODE_F1, IP_JOY_NONE )
	/* bit 6 is the VG HALT bit. We set it to "low" */
	/* per default (busy vector processor). */
 	/* handled by tempest_IN0_r() */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	/* bit 7 is tied to a 3khz (?) clock */
 	/* handled by tempest_IN0_r() */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN1/DSW0 */
	/* This is the Tempest spinner input. It only uses 4 bits. */
	PORT_ANALOG( 0x0f, 0x00, IPT_DIAL, 25, 20, 0, 0)
	/* The next one is reponsible for cocktail mode.
	 * According to the documentation, this is not a switch, although
	 * it may have been planned to put it on the Math Box PCB, D/E2 )
	 */
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Cocktail ) )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_DIPNAME(  0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(     0x02, "Easy" )
	PORT_DIPSETTING(     0x03, "Medium1" )
	PORT_DIPSETTING(     0x00, "Medium2" )
	PORT_DIPSETTING(     0x01, "Hard" )
	PORT_DIPNAME(  0x04, 0x04, "Rating" )
	PORT_DIPSETTING(     0x04, "1, 3, 5, 7, 9" )
	PORT_DIPSETTING(     0x00, "tied to high score" )
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW1 - (N13 on analog vector generator PCB */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x00, "Right Coin" )
	PORT_DIPSETTING(    0x00, "*1" )
	PORT_DIPSETTING(    0x04, "*4" )
	PORT_DIPSETTING(    0x08, "*5" )
	PORT_DIPSETTING(    0x0c, "*6" )
	PORT_DIPNAME( 0x10, 0x00, "Left Coin" )
	PORT_DIPSETTING(    0x00, "*1" )
	PORT_DIPSETTING(    0x10, "*2" )
	PORT_DIPNAME( 0xe0, 0x00, "Bonus Coins" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPSETTING(    0x80, "1 each 5" )
	PORT_DIPSETTING(    0x40, "1 each 4 (+Demo)" )
	PORT_DIPSETTING(    0xa0, "1 each 3" )
	PORT_DIPSETTING(    0x60, "2 each 4 (+Demo)" )
	PORT_DIPSETTING(    0x20, "1 each 2" )
	PORT_DIPSETTING(    0xc0, "Freeze Mode" )
	PORT_DIPSETTING(    0xe0, "Freeze Mode" )

	PORT_START	/* DSW2 - (L12 on analog vector generator PCB */
	PORT_DIPNAME( 0x01, 0x00, "Minimum" )
	PORT_DIPSETTING(    0x00, "1 Credit" )
	PORT_DIPSETTING(    0x01, "2 Credit" )
	PORT_DIPNAME( 0x06, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x02, "French" )
	PORT_DIPSETTING(    0x04, "German" )
	PORT_DIPSETTING(    0x06, "Spanish" )
	PORT_DIPNAME( 0x38, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "10000" )
	PORT_DIPSETTING(    0x00, "20000" )
	PORT_DIPSETTING(    0x10, "30000" )
	PORT_DIPSETTING(    0x18, "40000" )
	PORT_DIPSETTING(    0x20, "50000" )
	PORT_DIPSETTING(    0x28, "60000" )
	PORT_DIPSETTING(    0x30, "70000" )
	PORT_DIPSETTING(    0x38, "None" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0xc0, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x80, "5" )
INPUT_PORTS_END


READ_HANDLER( input_port_1_bit_r ) { return (readinputport(1) & (1 << offset)) ? 0 : 228; }
READ_HANDLER( input_port_2_bit_r ) { return (readinputport(2) & (1 << offset)) ? 0 : 228; }

static struct POKEYinterface pokey_interface =
{
	2,	/* 2 chips */
	12096000/8,	/* 1.512 MHz */
	{ 50, 50 },
	/* The 8 pot handlers */
	{ input_port_1_bit_r, input_port_2_bit_r },
	{ input_port_1_bit_r, input_port_2_bit_r },
	{ input_port_1_bit_r, input_port_2_bit_r },
	{ input_port_1_bit_r, input_port_2_bit_r },
	{ input_port_1_bit_r, input_port_2_bit_r },
	{ input_port_1_bit_r, input_port_2_bit_r },
	{ input_port_1_bit_r, input_port_2_bit_r },
	{ input_port_1_bit_r, input_port_2_bit_r },
	/* The allpot handler */
	{ 0, 0 },
};



static struct MachineDriver machine_driver_tempest =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			12096000/8,	/* 1.512 MHz */
			readmem,writemem,0,0,
			interrupt,4 /* 4.1ms */
		}
	},
	60, 0,	/* frames per second, vblank duration (vector game, so no vblank) */
	1,
	0,

	/* video hardware */
	300, 400, { 0, 550, 0, 580 },
	0,
	256,0,
	avg_init_palette_multi,

	VIDEO_TYPE_VECTOR,
	0,
	avg_start_tempest,
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

/*
 * Tempest now uses the EAROM routines to load/save scores.
 * Just in case, here is a snippet of the old code:
 * if (memcmp(&RAM[0x0606],"\x07\x04\x01",3))
 *	osd_fread(f,&RAM[0x0600],0x200);
 */


ROM_START( tempest ) /* rev 3 */
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "136002.113",   0x9000, 0x0800, 0x65d61fe7 )
	ROM_LOAD( "136002.114",   0x9800, 0x0800, 0x11077375 )
	ROM_LOAD( "136002.115",   0xa000, 0x0800, 0xf3e2827a )
	ROM_LOAD( "136002.316",   0xa800, 0x0800, 0xaeb0f7e9 )
	ROM_LOAD( "136002.217",   0xb000, 0x0800, 0xef2eb645 )
	ROM_LOAD( "136002.118",   0xb800, 0x0800, 0xbeb352ab )
	ROM_LOAD( "136002.119",   0xc000, 0x0800, 0xa4de050f )
	ROM_LOAD( "136002.120",   0xc800, 0x0800, 0x35619648 )
	ROM_LOAD( "136002.121",   0xd000, 0x0800, 0x73d38e47 )
	ROM_LOAD( "136002.222",   0xd800, 0x0800, 0x707bd5c3 )
	ROM_RELOAD(             0xf800, 0x0800 ) /* for reset/interrupt vectors */
	/* Mathbox ROMs */
	ROM_LOAD( "136002.123",   0x3000, 0x0800, 0x29f7e937 )
	ROM_LOAD( "136002.124",   0x3800, 0x0800, 0xc16ec351 )
ROM_END

ROM_START( tempest1 ) /* rev 1 */
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "136002.113",   0x9000, 0x0800, 0x65d61fe7 )
	ROM_LOAD( "136002.114",   0x9800, 0x0800, 0x11077375 )
	ROM_LOAD( "136002.115",   0xa000, 0x0800, 0xf3e2827a )
	ROM_LOAD( "136002.116",   0xa800, 0x0800, 0x7356896c )
	ROM_LOAD( "136002.117",   0xb000, 0x0800, 0x55952119 )
	ROM_LOAD( "136002.118",   0xb800, 0x0800, 0xbeb352ab )
	ROM_LOAD( "136002.119",   0xc000, 0x0800, 0xa4de050f )
	ROM_LOAD( "136002.120",   0xc800, 0x0800, 0x35619648 )
	ROM_LOAD( "136002.121",   0xd000, 0x0800, 0x73d38e47 )
	ROM_LOAD( "136002.122",   0xd800, 0x0800, 0x796a9918 )
	ROM_RELOAD(             0xf800, 0x0800 ) /* for reset/interrupt vectors */
	/* Mathbox ROMs */
	ROM_LOAD( "136002.123",   0x3000, 0x0800, 0x29f7e937 )
	ROM_LOAD( "136002.124",   0x3800, 0x0800, 0xc16ec351 )
ROM_END

ROM_START( tempest2 ) /* rev 2 */
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "136002.113",   0x9000, 0x0800, 0x65d61fe7 )
	ROM_LOAD( "136002.114",   0x9800, 0x0800, 0x11077375 )
	ROM_LOAD( "136002.115",   0xa000, 0x0800, 0xf3e2827a )
	ROM_LOAD( "136002.116",   0xa800, 0x0800, 0x7356896c )
	ROM_LOAD( "136002.217",   0xb000, 0x0800, 0xef2eb645 )
	ROM_LOAD( "136002.118",   0xb800, 0x0800, 0xbeb352ab )
	ROM_LOAD( "136002.119",   0xc000, 0x0800, 0xa4de050f )
	ROM_LOAD( "136002.120",   0xc800, 0x0800, 0x35619648 )
	ROM_LOAD( "136002.121",   0xd000, 0x0800, 0x73d38e47 )
	ROM_LOAD( "136002.222",   0xd800, 0x0800, 0x707bd5c3 )
	ROM_RELOAD(             0xf800, 0x0800 ) /* for reset/interrupt vectors */
	/* Mathbox ROMs */
	ROM_LOAD( "136002.123",   0x3000, 0x0800, 0x29f7e937 )
	ROM_LOAD( "136002.124",   0x3800, 0x0800, 0xc16ec351 )
ROM_END

ROM_START( temptube )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "136002.113",   0x9000, 0x0800, 0x65d61fe7 )
	ROM_LOAD( "136002.114",   0x9800, 0x0800, 0x11077375 )
	ROM_LOAD( "136002.115",   0xa000, 0x0800, 0xf3e2827a )
	ROM_LOAD( "136002.316",   0xa800, 0x0800, 0xaeb0f7e9 )
	ROM_LOAD( "136002.217",   0xb000, 0x0800, 0xef2eb645 )
	ROM_LOAD( "tube.118",     0xb800, 0x0800, 0xcefb03f0 )
	ROM_LOAD( "136002.119",   0xc000, 0x0800, 0xa4de050f )
	ROM_LOAD( "136002.120",   0xc800, 0x0800, 0x35619648 )
	ROM_LOAD( "136002.121",   0xd000, 0x0800, 0x73d38e47 )
	ROM_LOAD( "136002.222",   0xd800, 0x0800, 0x707bd5c3 )
	ROM_RELOAD(             0xf800, 0x0800 ) /* for reset/interrupt vectors */
	/* Mathbox ROMs */
	ROM_LOAD( "136002.123",   0x3000, 0x0800, 0x29f7e937 )
	ROM_LOAD( "136002.124",   0x3800, 0x0800, 0xc16ec351 )
ROM_END

#if 0 /* identical to rom_tempest, only different rom sizes */
ROM_START( tempest3 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "tempest.x",    0x9000, 0x1000, 0x0 )
	ROM_LOAD( "tempest.1",    0xa000, 0x1000, 0x0 )
	ROM_LOAD( "tempest.3",    0xb000, 0x1000, 0x0 )
	ROM_LOAD( "tempest.5",    0xc000, 0x1000, 0x0 )
	ROM_LOAD( "tempest.7",    0xd000, 0x1000, 0x0 )
	ROM_RELOAD(            0xf000, 0x1000 )	/* for reset/interrupt vectors */
	/* Mathbox ROMs */
	ROM_LOAD( "tempest.np3",  0x3000, 0x1000, 0x0 )
ROM_END
#endif



GAME( 1980, tempest,  0,       tempest, tempest, 0, ROT0, "Atari", "Tempest (rev 3)" )
GAME( 1980, tempest1, tempest, tempest, tempest, 0, ROT0, "Atari", "Tempest (rev 1)" )
GAME( 1980, tempest2, tempest, tempest, tempest, 0, ROT0, "Atari", "Tempest (rev 2)" )
GAME( 1980, temptube, tempest, tempest, tempest, 0, ROT0, "hack", "Tempest Tubes" )
